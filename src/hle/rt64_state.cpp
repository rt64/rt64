//
// RT64
//

#include "rt64_state.h"

#include <cassert>
#include <cinttypes>

#include "im3d/im3d.h"
#include "im3d/im3d_math.h"
#include "imgui/imgui.h"
#include "implot/implot.h"

#include "common/rt64_elapsed_timer.h"
#include "common/rt64_math.h"
#include "common/rt64_tmem_hasher.h"
#include "preset/rt64_preset_draw_call.h"
#include "preset/rt64_preset_light.h"

#include "rt64_application.h"
#include "rt64_interpreter.h"

//#define ASSERT_ON_BLENDER_EMULATION
#define SYNC_ON_EVERY_FB_PAIR 0

#define MI_INTR_DP          0x00000020
#define MI_INTR_SP          0x00000001

namespace RT64 {
    const float ShiftScaleMap[] = {
        1.0f,
        1.0f / 2.0f,
        1.0f / 4.0f,
        1.0f / 8.0f,
        1.0f / 16.0f,
        1.0f / 32.0f,
        1.0f / 64.0f,
        1.0f / 128.0f,
        1.0f / 256.0f,
        1.0f / 512.0f,
        1.0f / 1024.0f,
        32.0f,
        16.0f,
        8.0f,
        4.0f,
        2.0f
    };

    // State

    State::State(uint8_t *RDRAM, uint32_t *MI_INTR_REG, void (*checkInterrupts)()) {
        assert(RDRAM != nullptr);
        assert(MI_INTR_REG != nullptr);
        assert(checkInterrupts != nullptr);

        this->RDRAM = RDRAM;
        this->MI_INTR_REG = MI_INTR_REG;
        this->checkInterrupts = checkInterrupts;

        rsp = std::make_unique<RSP>(this);
        rdp = std::make_unique<RDP>(this);

        reset();
    }

    State::~State() { }

    void State::setup(const External &ext) {
        this->ext = ext;

        rspProcessor = std::make_unique<RSPProcessor>(ext.device);
        framebufferRenderer = std::make_unique<FramebufferRenderer>(ext.framebufferGraphicsWorker, false, ext.createdGraphicsAPI, ext.shaderLibrary);
        renderFramebufferManager = std::make_unique<RenderFramebufferManager>(ext.device);

        const RenderMultisampling multisampling = RasterShader::generateMultisamplingPattern(ext.userConfig->msaaSampleCount(), ext.device->getCapabilities().sampleLocations);
        renderTargetManager.setMultisampling(multisampling);
        renderTargetManager.setUsesHDR(ext.shaderLibrary->usesHDR);
        updateRenderFlagSampleCount();
    }

    void State::reset() {
        displayListAddress = 0;
        displayListCounter = 0;
        rdramCheckPending = true;
        workloadCounter = 0;
        lastScreenHash = 0;
        lastScreenFactorCounter = 0;
        lastWorkloadIndex = 0;
        addLightsOnFlush = false;
        activeSpriteCommand.replacementHash = 0;

        rsp->reset();
        rdp->reset();
        resetDrawCall();
    }

    void State::resetDrawCall() {
        drawStatus.reset();
        drawCall.uid = 0;
        drawCall.triangleCount = 0;
        drawCall.minWorldMatrix = USHRT_MAX;
        drawCall.maxWorldMatrix = 0;
        drawCall.rect.reset();
        drawCall.rectDsdx = 0;
        drawCall.rectDtdy = 0;
        drawCall.rectLeftOrigin = G_EX_ORIGIN_NONE;
        drawCall.rectRightOrigin = G_EX_ORIGIN_NONE;
        drawCall.scissorRect.reset();
        drawCall.scissorMode = 0;
        drawCall.scissorLeftOrigin = G_EX_ORIGIN_NONE;
        drawCall.scissorRightOrigin = G_EX_ORIGIN_NONE;
        drawCall.colorCombiner = { 0, 0 };
        drawCall.otherMode = { 0, 0 };
        drawCall.geometryMode = 0;
        drawCall.objRenderMode = 0;
        drawCall.fillColor = 0;
        drawCall.tileIndex = 0;
        drawCall.tileCount = 0;
        drawCall.loadIndex = 0;
        drawCall.loadCount = 0;
        drawCall.textureOn = 0;
        drawCall.textureTile = 0;
        drawCall.textureLevels = 0;
        drawCall.rdpParams.primColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        drawCall.rdpParams.primLOD = { 0.0f, 0.0f };
        drawCall.rdpParams.primDepth = { 0.0f, 0.0f };
        drawCall.rdpParams.envColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        drawCall.rdpParams.fogColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        drawCall.rdpParams.blendColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        drawCall.cullBothMask = 0;
        drawCall.shadingSmoothMask = 0;
        drawCall.NoN = false;
        drawCall.extendedType = DrawExtendedType::None;
        drawCall.extendedFlags = {};
        drawCall.drawStatusChanges = 0;

        // Mark everything as changed so everything on the draw call gets updated.
        drawStatus.changed = (1U << static_cast<uint32_t>(DrawAttribute::Count)) - 1;
    }
    
    void State::updateDrawStatusAttribute(DrawAttribute attribute) {
        drawStatus.setChanged(attribute);
    }

    bool State::checkDrawState() {
        if (!drawStatus.isChanged()) {
            return false;
        }

        flush();

        return true;
    }

    void State::loadDrawState() {
        const bool gpuCopiesEnabled = ext.emulatorConfig->framebuffer.copyWithGPU;
        bool textureCheck = false;
        if (drawStatus.isChanged(DrawAttribute::Combine)) {
            drawCall.colorCombiner = rdp->colorCombinerStack[rdp->colorCombinerStackSize - 1];
            textureCheck = true;
        }

        if (drawStatus.isChanged(DrawAttribute::OtherMode)) {
            drawCall.otherMode = rdp->otherMode;
            textureCheck = true;
        }

        if (drawStatus.isChanged(DrawAttribute::GeometryMode)) {
            drawCall.geometryMode = rsp->geometryModeStack[rsp->geometryModeStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::PrimColor)) {
            drawCall.rdpParams.primColor = rdp->primColorStack[rdp->primColorStackSize - 1];
            drawCall.rdpParams.primLOD = rdp->primLODStack[rdp->primColorStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::PrimDepth)) {
            drawCall.rdpParams.primDepth = rdp->primDepthStack[rdp->primDepthStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::EnvColor)) {
            drawCall.rdpParams.envColor = rdp->envColorStack[rdp->envColorStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::FogColor)) {
            drawCall.rdpParams.fogColor = rdp->fogColorStack[rdp->fogColorStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::FillColor)) {
            drawCall.fillColor = rdp->fillColorStack[rdp->fillColorStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::BlendColor)) {
            drawCall.rdpParams.blendColor = rdp->blendColorStack[rdp->blendColorStackSize - 1];
        }

        if (drawStatus.isChanged(DrawAttribute::Convert)) {
            drawCall.rdpParams.convertK[0] = rdp->convertK[0];
            drawCall.rdpParams.convertK[1] = rdp->convertK[1];
            drawCall.rdpParams.convertK[2] = rdp->convertK[2];
            drawCall.rdpParams.convertK[3] = rdp->convertK[3];
            drawCall.rdpParams.convertK[4] = rdp->convertK[4];
            drawCall.rdpParams.convertK[5] = rdp->convertK[5];
        }

        if (drawStatus.isChanged(DrawAttribute::Key)) {
            drawCall.rdpParams.keyCenter = rdp->keyCenter;
            drawCall.rdpParams.keyScale = rdp->keyScale;
        }

        if (drawStatus.isChanged(DrawAttribute::Scissor)) {
            drawCall.scissorRect = rdp->scissorRectStack[rdp->scissorStackSize - 1];
            drawCall.scissorMode = rdp->scissorModeStack[rdp->scissorStackSize - 1];
            drawCall.scissorLeftOrigin = rdp->extended.scissorLeftOrigin;
            drawCall.scissorRightOrigin = rdp->extended.scissorRightOrigin;
        }
        
        if (drawStatus.isChanged(DrawAttribute::Texture) || textureCheck) {
            // Detect the tile count based on the LOD setting.
            const bool usesLOD = (drawCall.otherMode.textLOD() == G_TL_LOD);
            if (usesLOD) {
                drawCall.tileCount = drawCall.textureLevels;
            }
            // Detect the tile count based on what texels it uses.
            else {
                const bool usesTexel0 = drawCall.colorCombiner.usesTexture(drawCall.otherMode, 0, false);
                const bool usesTexel1 = drawCall.colorCombiner.usesTexture(drawCall.otherMode, 1, false);
                if (usesTexel1) {
                    drawCall.tileCount = 2;
                }
                else if (usesTexel0) {
                    drawCall.tileCount = 1;
                }
                else {
                    drawCall.tileCount = 0;
                }
            }

            const int workloadCursor = ext.workloadQueue->writeCursor;
            Workload &workload = ext.workloadQueue->workloads[workloadCursor];
            if (drawCall.tileCount > 0) {
                DrawData &drawData = workload.drawData;
                assert(drawData.callTiles.size() == drawData.rdpTiles.size());
                drawCall.tileIndex = static_cast<uint32_t>(drawData.rdpTiles.size());

                const size_t requiredSize = drawCall.tileIndex + drawCall.tileCount;
                if ((drawData.rdpTiles.size() < requiredSize) || (drawData.callTiles.size() < requiredSize)) {
                    drawData.rdpTiles.resize(requiredSize, { });
                    drawData.callTiles.resize(requiredSize, { });
                }
                
                const uint8_t tileIndexBase = drawCall.textureTile;
                for (uint32_t t = 0; t < drawCall.tileCount; t++) {
                    const uint8_t tileIndex = (tileIndexBase + t) % RDP_TILES;
                    const auto &tile = rdp->tiles[tileIndex];
                    const uint32_t callTileIndex = drawCall.tileIndex + t;
                    auto &dstCallTile = drawData.callTiles[callTileIndex];

                    // Failsafe cases.
                    if (tile.line == 0) {
                        assert((drawCall.otherMode.cycleType() != G_CYC_COPY) && "Copy mode should be able to work even with tiles that aren't defined.");
                        dstCallTile.valid = false;
                        continue;
                    }

                    dstCallTile.valid = true;
                    dstCallTile.loadTile = tile;
                    dstCallTile.reinterpretTile = false;
                    dstCallTile.reinterpretSiz = 0;
                    dstCallTile.reinterpretFmt = 0;
                    dstCallTile.tlut = rdp->otherMode.textLUT();
                    dstCallTile.minTexcoord = { INT_MAX, INT_MAX };
                    dstCallTile.maxTexcoord = { INT_MIN, INT_MIN };

                    // Determine the line width that guarantees image alignment for this tile.
                    const bool RGBA32 = (tile.siz == G_IM_SIZ_32b) && (tile.fmt == G_IM_FMT_RGBA);
                    const uint32_t lineShift = RGBA32 ? 1 : 0;
                    dstCallTile.lineWidth = tile.line << ((4 + lineShift) - tile.siz);

                    // Estimate sampling width and height based on masks ands tiles, whichever are smaller.
                    const bool clampS = (tile.masks == 0) || (tile.cms & G_TX_CLAMP);
                    const bool clampT = (tile.maskt == 0) || (tile.cmt & G_TX_CLAMP);
                    const int32_t tileWidth = clampS ? std::max((tile.lrs - tile.uls + 4) / 4, 1) : 0xFFFF;
                    const int32_t tileHeight = clampT ? std::max((tile.lrt - tile.ult + 4) / 4, 1) : 0xFFFF;
                    const int32_t maskWidth = (tile.masks > 0) ? static_cast<int32_t>(1 << tile.masks) : 0xFFFF;
                    const int32_t maskHeight = (tile.maskt > 0) ? static_cast<int32_t>(1 << tile.maskt) : 0xFFFF;
                    dstCallTile.sampleWidth = static_cast<uint16_t>(std::min(tileWidth, maskWidth));
                    dstCallTile.sampleHeight = static_cast<uint16_t>(std::min(tileHeight, maskHeight));

                    // Check if we need to use raw TMEM decoding because the tile can sample more bytes than TMEM actually allows.
                    assert((dstCallTile.sampleWidth > 0) && (dstCallTile.sampleHeight > 0) && "Sample size calculation can only result in non-zero values.");
                    dstCallTile.rawTMEM = TMEMHasher::requiresRawTMEM(tile, dstCallTile.sampleWidth, dstCallTile.sampleHeight);
                    
                    auto &dstRDPTile = drawData.rdpTiles[drawCall.tileIndex + t];
                    dstRDPTile.fmt = tile.fmt;
                    dstRDPTile.siz = tile.siz;
                    dstRDPTile.stride = int(tile.line) << 3;
                    dstRDPTile.address = int(tile.tmem) << 3;
                    dstRDPTile.palette = tile.palette;
                    dstRDPTile.masks = (tile.masks > 0) ? (1 << tile.masks) : 0;
                    dstRDPTile.maskt = (tile.maskt > 0) ? (1 << tile.maskt) : 0;
                    dstRDPTile.shifts = ShiftScaleMap[tile.shifts];
                    dstRDPTile.shiftt = ShiftScaleMap[tile.shiftt];
                    dstRDPTile.uls = float(tile.uls);
                    dstRDPTile.ult = float(tile.ult);
                    dstRDPTile.cms = tile.cms | ((tile.masks == 0) ? G_TX_CLAMP : 0);
                    dstRDPTile.cmt = tile.cmt | ((tile.maskt == 0) ? G_TX_CLAMP : 0);

                    // Since we can't simulate not sending the triangle coefficients individually and it falls in the realm
                    // of undefined behavior, when the texture state is off, we merely clamp the result.
                    if (drawCall.textureOn) {
                        dstRDPTile.lrs = float(tile.lrs);
                        dstRDPTile.lrt = float(tile.lrt);
                    }
                    else {
                        dstRDPTile.lrs = float(tile.uls);
                        dstRDPTile.lrt = float(tile.ult);
                        dstRDPTile.cms |= G_TX_CLAMP;
                        dstRDPTile.cmt |= G_TX_CLAMP;
                    }

                    auto clampAlignedToMask = [](uint8_t mask, uint16_t ul, uint16_t lr) {
                        uint16_t maskSize = 1 << mask;
                        uint16_t ulPixel = ul / 4;
                        uint16_t lrPixel = (lr + 4) / 4;
                        return (lrPixel - ulPixel) == maskSize;
                    };

                    bool nativeSamplerSupported = true;
                    if ((tile.masks > 0) && (tile.cms & G_TX_CLAMP)) {
                        nativeSamplerSupported = nativeSamplerSupported && clampAlignedToMask(tile.masks, tile.uls, tile.lrs);
                    }

                    if ((tile.maskt > 0) && (tile.cmt & G_TX_CLAMP)) {
                        nativeSamplerSupported = nativeSamplerSupported && clampAlignedToMask(tile.maskt, tile.ult, tile.lrt);
                    }
                    
                    // Check if there's any valid tile copies that could be used. The line width requirement has to match for 
                    // the tile copy to make sense, but whether enough pixels are available in the copy according to the sampling
                    // done by the calls is determined in a later step.
                    FramebufferManager::CheckCopyResult checkResult;
                    checkResult = framebufferManager.checkTileCopyTMEM(tile.tmem, dstCallTile.lineWidth, tile.siz, tile.fmt, tile.uls);
                    dstCallTile.syncRequired = checkResult.syncRequired;

                    if (gpuCopiesEnabled && checkResult.valid()) {
                        dstCallTile.tileCopyUsed = true;
                        dstCallTile.tmemHashOrID = checkResult.tileId;
                        dstCallTile.tileCopyWidth = checkResult.tileWidth;
                        dstCallTile.tileCopyHeight = checkResult.tileHeight;

                        // We must force reinterpretation if a LUT format is used.
                        const uint32_t tlutFormat = rdp->otherMode.textLUT();
                        const bool usesTLUT = (tlutFormat > 0);
                        dstCallTile.reinterpretTile = checkResult.reinterpret || usesTLUT;
                        dstCallTile.reinterpretSiz = checkResult.siz;
                        dstCallTile.reinterpretFmt = checkResult.fmt;

                        // Native samplers can't apply the texel shift and mask that tile reinterpretation requires.
                        if (dstCallTile.reinterpretTile) {
                            nativeSamplerSupported = false;
                        }
                    }
                    else {
                        dstCallTile.tileCopyUsed = false;
                        dstCallTile.tmemHashOrID = rdp->tileReplacementHashes[tileIndex];
                    }

                    if (nativeSamplerSupported) {
                        auto nativeSamplerFromAddressing = [](uint8_t cms, uint8_t cmt) {
                            switch (cms) {
                            case G_TX_WRAP:
                                switch (cmt) {
                                case G_TX_WRAP:                  return NATIVE_SAMPLER_WRAP_WRAP;
                                case G_TX_MIRROR:                return NATIVE_SAMPLER_WRAP_MIRROR;
                                case G_TX_CLAMP:                 return NATIVE_SAMPLER_WRAP_CLAMP;
                                case (G_TX_MIRROR | G_TX_CLAMP): return NATIVE_SAMPLER_WRAP_CLAMP;
                                default:                         return NATIVE_SAMPLER_NONE;
                                }
                            case G_TX_MIRROR:
                                switch (cmt) {
                                case G_TX_WRAP:                  return NATIVE_SAMPLER_MIRROR_WRAP;
                                case G_TX_MIRROR:                return NATIVE_SAMPLER_MIRROR_MIRROR;
                                case G_TX_CLAMP:                 return NATIVE_SAMPLER_MIRROR_CLAMP;
                                case (G_TX_MIRROR | G_TX_CLAMP): return NATIVE_SAMPLER_MIRROR_CLAMP;
                                default:                         return NATIVE_SAMPLER_NONE;
                                }
                            case G_TX_CLAMP:
                            case (G_TX_MIRROR | G_TX_CLAMP):
                                switch (cmt) {
                                case G_TX_WRAP:                  return NATIVE_SAMPLER_CLAMP_WRAP;
                                case G_TX_MIRROR:                return NATIVE_SAMPLER_CLAMP_MIRROR;
                                case G_TX_CLAMP:                 return NATIVE_SAMPLER_CLAMP_CLAMP;
                                case (G_TX_MIRROR | G_TX_CLAMP): return NATIVE_SAMPLER_CLAMP_CLAMP;
                                default:                         return NATIVE_SAMPLER_NONE;
                                }
                            default:
                                return NATIVE_SAMPLER_NONE;
                            }
                        };

                        uint8_t nativeCms = (tile.masks == 0) ? (tile.cms | G_TX_CLAMP) : tile.cms;
                        uint8_t nativeCmt = (tile.maskt == 0) ? (tile.cmt | G_TX_CLAMP) : tile.cmt;
                        dstRDPTile.nativeSampler = nativeSamplerFromAddressing(nativeCms, nativeCmt);;
                    }
                    else {
                        dstRDPTile.nativeSampler = NATIVE_SAMPLER_NONE;
                    }
                }
            }
            
            // Increase the load operation count.
            size_t loadOperationTotal = workload.drawData.loadOperations.size();
            drawCall.loadCount += uint32_t(loadOperationTotal - workload.drawRanges.loadOperations.second);
            drawCall.loadIndex = uint32_t(loadOperationTotal) - drawCall.loadCount;
            workload.drawRanges.loadOperations.second = loadOperationTotal;
        }

        if (drawStatus.isChanged(DrawAttribute::Lights)) {
            addLightsOnFlush = rsp->lightCount > 0;
        }

        if (drawStatus.isChanged(DrawAttribute::ObjRenderMode)) {
            drawCall.objRenderMode = rsp->objRenderMode;
        }

        if (drawStatus.isChanged(DrawAttribute::ExtendedType)) {
            drawCall.extendedType = rsp->extended.drawExtendedType;
            drawCall.extendedData = rsp->extended.drawExtendedData;
        }

        if (drawStatus.isChanged(DrawAttribute::ExtendedFlags)) {
            drawCall.extendedFlags = rdp->extended.drawExtendedFlags;
        }

        drawStatus.clearChanges();
    }

    void State::flush() {
        if (drawCall.triangleCount == 0) {
            return;
        }

        // Assign some parameters to the draw call.
        const int workloadCursor = ext.workloadQueue->writeCursor;
        Workload &workload = ext.workloadQueue->workloads[workloadCursor];
        FramebufferPair &fbPair = workload.fbPairs[workload.currentFramebufferPairIndex()];
        drawCall.cullBothMask = rsp->cullBothMask;
        drawCall.shadingSmoothMask = rsp->shadingSmoothMask;
        drawCall.NoN = rsp->NoN;
        drawCall.drawStatusChanges = drawStatus.changed;
        drawCall.callIndex = workload.gameCallCount++;

        // Add the draw call to the FB pair.
        GameCall gameCall;
        gameCall.callDesc = drawCall;
        fbPair.addGameCall(gameCall);

        // Assign the indices or increase the count of the active sprite command if it exists.
        if (activeSpriteCommand.replacementHash != 0) {
            if (activeSpriteCommand.callCount == 0) {
                activeSpriteCommand.fbPairIndex = workload.fbPairCount - 1;
                activeSpriteCommand.projIndex = fbPair.projectionCount - 1;
                activeSpriteCommand.callIndex = fbPair.projections[activeSpriteCommand.projIndex].gameCallCount - 1;
            }

            activeSpriteCommand.callCount++;
        }

        // Lights have been changed in this call.
        if (addLightsOnFlush) {
            Projection &proj = fbPair.projections[fbPair.projectionCount - 1];
            const GBI *curGBI = ext.interpreter->hleGBI;
            const bool usesPointLighting = curGBI->flags.pointLighting && (rsp->geometryModeStack[rsp->geometryModeStackSize - 1] & G_POINT_LIGHTING);
            for (int i = 0; i < rsp->lightCount; i++) {
                proj.lightManager.processLight(this, i, usesPointLighting);
            }

            proj.lightManager.processAmbientLight(this, rsp->lightCount);
        }
        
        // Handle extended type behavior during recording.
        switch (drawCall.extendedType) {
        case DrawExtendedType::VertexTestZ:
            extended.vertexTestZActive = true;
            break;
        case DrawExtendedType::EndVertexTestZ:
            extended.vertexTestZActive = false;
            break;
        default:
            if (extended.vertexTestZActive) {
                workload.extended.testZIndexCount += drawCall.triangleCount * 3;
            }

            break;
        };
        
        // Reset attributes for the next draw call to be recorded.
        drawCall.rectDsdx = 0;
        drawCall.rectDtdy = 0;
        drawCall.loadIndex = 0;
        drawCall.loadCount = 0;
        drawCall.minWorldMatrix = USHRT_MAX;
        drawCall.maxWorldMatrix = 0;
        drawCall.triangleCount = 0;
    }
    
    void State::submitFramebufferPair(FramebufferPair::FlushReason flushReason) {
        const int workloadCursor = ext.workloadQueue->writeCursor;
        Workload &workload = ext.workloadQueue->workloads[workloadCursor];

        // Ignore if the amount of framebuffer pairs isn't bigger than the amount of framebuffer pairs submitted.
        if (workload.fbPairCount <= workload.fbPairSubmitted) {
            return;
        }
        
        const int fbPairIndex = workload.currentFramebufferPairIndex();
        FramebufferPair &fbPair = workload.fbPairs[fbPairIndex];
        fbPair.flushReason = flushReason;

        // Add all the pending framebuffer operations to the first one that was found.
        flushFramebufferOperations(fbPair);

        if (!fbPair.drawColorRect.isEmpty()) {
            auto &colorImg = fbPair.colorImage;
            auto &depthImg = fbPair.depthImage;
            uint32_t colorHeight = fbPair.drawColorRect.bottom(true);
            uint32_t colorWriteWidth = (colorHeight > 1) ? colorImg.width : std::min(fbPair.drawColorRect.right(true), int32_t(colorImg.width));
            uint32_t depthWriteWidth = 0;
            RT64::Framebuffer *colorFb = &framebufferManager.get(colorImg.address, colorImg.siz, colorImg.width, colorHeight);
            colorImg.formatChanged = colorFb->widthChanged || colorFb->sizChanged || colorFb->rdramChanged;
            colorFb->clearChanged();
            colorFb->addDitherPatterns(fbPair.ditherPatterns);

            RT64::Framebuffer *depthFb = nullptr;
            if (fbPair.depthRead || fbPair.depthWrite) {
                if (!fbPair.drawDepthRect.isNull()) {
                    depthWriteWidth = (fbPair.drawColorRect.bottom(true) > 1) ? colorImg.width : std::min(fbPair.drawDepthRect.right(true), int32_t(colorImg.width));
                }

                depthFb = &framebufferManager.get(depthImg.address, G_IM_SIZ_16b, colorImg.width, colorHeight);
                depthImg.formatChanged = depthFb->widthChanged || depthFb->sizChanged || depthFb->rdramChanged;
                depthFb->clearChanged();

                // Detect a fast path by checking if the framebuffer pair previous to the current one only did fill rects.
                // This is a very good indicator that the previous framebuffer pair was only intended to clear the depth
                // buffer and we can skip the color to depth conversion.
                int previousFbPairIndex = workload.currentFramebufferPairIndex() - 1;
                if (previousFbPairIndex >= 0) {
                    FramebufferPair &previousFbPair = workload.fbPairs[previousFbPairIndex];
                    if (previousFbPair.fillRectOnly && (previousFbPair.colorImage.address == depthImg.address) && (previousFbPair.colorImage.siz == G_IM_SIZ_16b)) {
                        previousFbPair.fastPaths.clearDepthOnly = true;
                    }
                }
            }

            // Synchronization will be required unless direct reinterpretation is possible (which is not implemented yet).
            if (colorImg.formatChanged || depthImg.formatChanged) {
                fbPair.syncRequired = true;
            }

            uint32_t colorFbBytes = colorFb->imageRowBytes(colorWriteWidth) * colorFb->height;
            uint64_t writeTimestamp = framebufferManager.nextWriteTimestamp();
            colorFb->RAMBytes = std::max(colorFb->RAMBytes, colorFbBytes);
            colorFb->modifiedBytes = std::max(int32_t(colorFb->modifiedBytes - colorFbBytes), 0);
            colorFb->lastWriteFmt = colorImg.fmt;
            colorFb->lastWriteTimestamp = writeTimestamp;
            framebufferManager.changeRAM(colorFb, colorFb->addressStart, colorFb->addressStart + colorFbBytes);

            bool depthWrite = fbPair.depthWrite && (depthFb != nullptr);
            if (depthWrite && (depthWriteWidth > 0)) {
                uint32_t depthFbBytes = depthFb->imageRowBytes(depthWriteWidth) * depthFb->height;
                depthFb->RAMBytes = std::max(depthFb->RAMBytes, depthFbBytes);
                depthFb->modifiedBytes = std::max(int32_t(depthFb->modifiedBytes - depthFbBytes), 0);
                depthFb->lastWriteFmt = G_IM_FMT_DEPTH;
                depthFb->lastWriteTimestamp = writeTimestamp;
                framebufferManager.changeRAM(depthFb, depthFb->addressStart, depthFb->addressStart + depthFbBytes);
            }
        }

        // Store the process Id this framebuffer pair was submitted under.
        fbPair.displayListAddress = displayListAddress;
        fbPair.displayListCounter = displayListCounter;

        // Mark various flags so the projection and FB pair information gets reset as soon as possible on the next draw call.
        rdp->colorImage.changed = true;
        rdp->depthImage.changed = true;

        // Increase the submitted framebuffer counter.
        workload.fbPairSubmitted++;
    }
    
    void State::checkRDRAM() {
        if (!rdramCheckPending) {
            return;
        }
        
        assert(drawFbOperations.empty() && "There should be no pending framebuffer operations when this is started.");
        assert(drawFbDiscards.empty() && "There should be no pending framebuffer discards when this is started.");

        const int workloadCursor = ext.workloadQueue->writeCursor;
        Workload &workload = ext.workloadQueue->workloads[workloadCursor];
        const uint32_t fbPairIndex = workload.currentFramebufferPairIndex();
        {
            framebufferManager.storeRAM(workload.fbStorage, RDRAM, fbPairIndex);
            framebufferManager.checkRAM(RDRAM, differentFbs, true);
            if (!differentFbs.empty()) {
                RenderWorkerExecution execution(ext.framebufferGraphicsWorker);
                framebufferManager.uploadRAM(ext.framebufferGraphicsWorker, differentFbs.data(), differentFbs.size(), workload.fbChangePool, RDRAM, true, drawFbOperations, drawFbDiscards, ext.shaderLibrary);
            }

            framebufferManager.performDiscards(drawFbDiscards);
        }

        rdramCheckPending = false;
    }

    void State::fullSyncFramebufferPairTiles(Workload &workload, FramebufferPair &fbPair, uint32_t &loadOpCursor, uint32_t &rdpTileCursor) {
        auto loadOperation = [&](uint32_t loadOpIndex) {
            const auto &loadOp = workload.drawData.loadOperations[loadOpIndex];

            // For emulating how Rice hashes textures, we need information about the load operation that was performed in the particular TMEM address.
            rdp->rice.lastLoadOpByTMEM[loadOp.tile.tmem] = loadOp;

            switch (loadOp.type) {
            case LoadOperation::Type::Block:
                rdp->loadBlockOperation(loadOp.tile, loadOp.texture, false);
                break;
            case LoadOperation::Type::Tile:
                rdp->loadTileOperation(loadOp.tile, loadOp.texture, false);
                break;
            case LoadOperation::Type::TLUT:
                rdp->loadTLUTOperation(loadOp.tile, loadOp.texture, false);
                break;
            };
        };

        auto uploadTile = [&](uint32_t rdpTileIndex, FramebufferPair &fbPair, const DrawCall &drawCall) {
            auto &callTile = workload.drawData.callTiles[rdpTileIndex];
            if (!callTile.valid) {
                return;
            }

            if (!callTile.tileCopyUsed) {
                if (callTile.tmemHashOrID != 0) {
                    textureManager.uploadEmpty(this, ext.textureCache, workload.submissionFrame, callTile.sampleWidth, callTile.sampleHeight, callTile.tmemHashOrID);
                }
                else if (callTile.rawTMEM) {
                    callTile.tmemHashOrID = textureManager.uploadTMEM(this, callTile.loadTile, ext.textureCache, workload.submissionFrame, 0, RDP_TMEM_BYTES, callTile.sampleWidth, callTile.sampleHeight, callTile.tlut);
                }
                else {
                    callTile.tmemHashOrID = textureManager.uploadTexture(this, callTile.loadTile, ext.textureCache, workload.submissionFrame, callTile.sampleWidth, callTile.sampleHeight, callTile.tlut);
                }
            }
            else if (callTile.reinterpretTile) {
                bool ulScaleS = true;
                bool ulScaleT = true;
                interop::uint2 texelShift = { 0, 0 };
                interop::uint2 texelMask = { UINT_MAX, UINT_MAX };

                // In cases where the tile pixel size is smaller than tile copy pixel size, we check for certain alignment conditions
                // to improve the quality of the final result when the effect's resolution is increased.
                // Perfect alignment of rectDsdx to a clean multiplier is also required to validate any of these conditions.
                if ((callTile.reinterpretSiz > callTile.loadTile.siz) && ((drawCall.rectDsdx & 0x3FF) == 0)) {
                    uint8_t sizDifference = callTile.reinterpretSiz - callTile.loadTile.siz;

                    // If the rect scale is higher than 1, we can set a mask and a shift of the texels that should be sampled
                    // based on the scroll component. The scroll component is usually used in these cases to select the particular
                    // column that needs to be sampled when doing reinterpretation that expands the pixel.
                    // This enhancement is also not allowed if uls has a fractional component.
                    uint16_t rectMultiplier = drawCall.rectDsdx >> 10;
                    const bool powerOfTwo = ((rectMultiplier & (rectMultiplier - 1)) == 0);
                    if ((rectMultiplier > 1) && powerOfTwo && (callTile.loadTile.uls & 0x3) == 0) {
                        texelMask.x = ~(rectMultiplier - 1);
                        texelShift.x = (callTile.loadTile.uls >> 2) % rectMultiplier;

                        // This part fixes an artifact caused by how the scaling at high resolution affects the uls component. This is
                        // left under a special condition since it falls under the category of a developer intended fix rather than
                        // hardware behavior. Since the scaling of the coordinates can result in them ending up at a spot that
                        // introduces artifacts when they were intended to shift a particular effect by only one pixel, this
                        // fix can detect if the scroll is within a certain tolerance of the edge of the texture and disable the
                        // coordinate from being affected by the high resolution scaling. The tolerance is determined by the amount of
                        // pixels that result from expanding the original pixel.
                        //
                        // This helps with effects such as the greyscale ("VisMono") scenes.
                        //
                        const bool ulsFix = ext.enhancementConfig->framebuffer.reinterpretFixULS;
                        if (ulsFix && ((callTile.loadTile.uls >> 2) <= (1 << sizDifference))) {
                            ulScaleS = false;
                        }
                    }
                }

                // Upload the TLUT as raw TMEM if the reinterpretation requires it.
                uint64_t tlutHash = 0;
                if (callTile.tlut > 0) {
                    const bool CI4 = (callTile.loadTile.siz == G_IM_SIZ_4b);
                    const uint16_t byteOffset = (RDP_TMEM_BYTES >> 1) + (CI4 ? (callTile.loadTile.palette << 7) : 0);
                    const uint16_t byteCount = CI4 ? 0x100 : 0x800;
                    tlutHash = textureManager.uploadTMEM(this, {}, ext.textureCache, workload.submissionFrame, byteOffset, byteCount, 0, 0, 0);
                }

                // Create a tile copy and tile reinterperation operation and queue it.
                uint64_t newTileId = framebufferManager.findTileCopyId(callTile.tileCopyWidth, callTile.tileCopyHeight);
                FramebufferOperation fbOp = framebufferManager.makeTileReintepretation(callTile.tmemHashOrID, callTile.reinterpretSiz, callTile.reinterpretFmt,
                    newTileId, callTile.loadTile.siz, callTile.loadTile.fmt, ulScaleS, ulScaleT, texelShift, texelMask, tlutHash, callTile.tlut);

                fbPair.startFbOperations.emplace_back(fbOp);
                callTile.tmemHashOrID = newTileId;
            }
        };

        for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
            const Projection &proj = fbPair.projections[p];
            for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                const GameCall &gameCall = proj.gameCalls[d];

                // Perform all load operations.
                if (gameCall.callDesc.loadCount > 0) {
                    const uint32_t loadOpLimit = gameCall.callDesc.loadIndex + gameCall.callDesc.loadCount;
                    while (loadOpCursor < loadOpLimit) {
                        loadOperation(loadOpCursor);
                        loadOpCursor++;
                    }
                }

                // Perform all tile sampling operations.
                if (gameCall.callDesc.tileCount > 0) {
                    const uint32_t rdpTileLimit = gameCall.callDesc.tileIndex + gameCall.callDesc.tileCount;
                    while (rdpTileCursor < rdpTileLimit) {
                        uploadTile(rdpTileCursor, fbPair, gameCall.callDesc);
                        rdpTileCursor++;
                    }
                }
            }
        }
    }
    
    void State::fullSync() {
        flush();
        submitFramebufferPair(FramebufferPair::FlushReason::ProcessDisplayListsEnd);

        // Append any framebuffer operations to the end of the last framebuffer pair.
        int workloadCursor = ext.workloadQueue->writeCursor;
        Workload &workload = ext.workloadQueue->workloads[workloadCursor];
        if (hasFramebufferOperationsPending()) {
            // If no framebuffer pair is present to do this, we make a placeholder one with the current images.
            if (workload.fbPairCount == 0) {
                workload.addFramebufferPair(rdp->colorImage.address, rdp->colorImage.fmt, rdp->colorImage.siz, rdp->colorImage.width, rdp->depthImage.address);
            }

            FramebufferPair &lastFbPair = workload.fbPairs[workload.fbPairCount - 1];
            flushFramebufferOperations(lastFbPair);
        }
        
        // Copy the current state's extended parameters into the workload.
        workload.extended.ditherNoiseStrength = extended.ditherNoiseStrength;

        auto emitTileWarning = [&](CommandWarning warning, size_t tileIndex) {
            warning.indexType = CommandWarning::IndexType::TileIndex;
            warning.tile.index = uint32_t(tileIndex);
            workload.commandWarnings.emplace_back(warning);
        };

        // Validate all tile copies to be used during the rendering.
        const bool extendedRenderToRAMSet = (extended.renderToRAM < UINT8_MAX);
        const bool renderToRDRAM = extendedRenderToRAMSet ? (extended.renderToRAM != 0) : ext.emulatorConfig->framebuffer.renderToRAM;
        const bool warningsEnabled = ext.userConfig->developerMode;
        const bool linearFiltering = !ext.userConfig->threePointFiltering;
        const size_t callTileCount = workload.drawData.callTiles.size();
        for (size_t t = 0; t < callTileCount; t++) {
            DrawCallTile &callTile = workload.drawData.callTiles[t];
            if (!callTile.valid) {
                continue;
            }

            if (warningsEnabled) {
                const auto &loadTile = callTile.loadTile;

                // Using horizontal clamp but coordinates are in the wrong order.
                if (((loadTile.cms & G_TX_CLAMP) || (loadTile.masks == 0)) && (loadTile.uls > loadTile.lrs)) {
                    emitTileWarning(CommandWarning::format("Texture tile #%u: The tile uses horizontal clamp, but the left coordinate "
                        "is a higher value than the right coordinate. Only one column of pixels can be used.", t), t);
                }

                // Using vertical clamp but coordinates are in the wrong order.
                if (((loadTile.cmt & G_TX_CLAMP) || (loadTile.maskt == 0)) && (loadTile.ult > loadTile.lrt)) {
                    emitTileWarning(CommandWarning::format("Texture tile #%u: The tile uses vertical clamp, but the top coordinate "
                        "is a higher value than the bottom coordinate. Only one row of pixels can be used.", t), t);
                }

            }

            // The rest of the validation doesn't apply to tiles that aren't GPU copies or sampling TMEM directly.
            if (!callTile.tileCopyUsed && !callTile.rawTMEM) {
                continue;
            }

            // Figure out the max sampling area based on the tracked texture coordinates.
            interop::int2 minTexcoord;
            interop::int2 maxTexcoord;
            const auto &loadTile = callTile.loadTile;
            const int32_t xScroll = (loadTile.uls >> 2);
            const int32_t yScroll = (loadTile.ult >> 2);
            const float xScale = ShiftScaleMap[loadTile.shifts];
            const float yScale = ShiftScaleMap[loadTile.shiftt];
            if (callTile.minTexcoord.x > callTile.maxTexcoord.x) {
                minTexcoord.x = INT_MIN;
                maxTexcoord.x = INT_MAX;
            }
            else {
                minTexcoord.x = int(callTile.minTexcoord.x * xScale) - xScroll;
                maxTexcoord.x = int(ceill(callTile.maxTexcoord.x * xScale)) - xScroll;
            }

            if (callTile.minTexcoord.y > callTile.maxTexcoord.y) {
                minTexcoord.y = INT_MIN;
                maxTexcoord.y = INT_MAX;
            }
            else {
                maxTexcoord.y = int(ceill(callTile.maxTexcoord.y * yScale)) - yScroll;
                minTexcoord.y = int(callTile.minTexcoord.y * yScale) - yScroll;
            }

            // Check the clamp or lack of mask.
            if ((loadTile.cms & G_TX_CLAMP) || (loadTile.masks == 0)) {
                minTexcoord.x = std::max(minTexcoord.x, 0);
                maxTexcoord.x = std::min(maxTexcoord.x, int((loadTile.lrs - loadTile.uls) + 3) >> 2);
            }

            if ((loadTile.cmt & G_TX_CLAMP) || (loadTile.maskt == 0)) {
                minTexcoord.y = std::max(minTexcoord.y, 0);
                maxTexcoord.y = std::min(maxTexcoord.y, int((loadTile.lrt - loadTile.ult) + 3) >> 2);
            }

            // If mask is enabled, clamp the range to the mask.
            if (loadTile.masks > 0) {
                int maskWidth = 1 << loadTile.masks;
                if (minTexcoord.x < 0) {
                    maxTexcoord.x = maskWidth;
                }

                minTexcoord.x = std::max(minTexcoord.x, 0);
                maxTexcoord.x = std::min(maxTexcoord.x, maskWidth);
            }

            if (loadTile.maskt > 0) {
                int maskHeight = 1 << loadTile.maskt;
                if (minTexcoord.y < 0) {
                    maxTexcoord.y = maskHeight;
                }

                minTexcoord.y = std::max(minTexcoord.y, 0);
                maxTexcoord.y = std::min(maxTexcoord.y, maskHeight);
            }

            // Invalidate the use of the tile copy if the texcoord range can't fit inside the tile copy.
            if (callTile.tileCopyUsed) {
                const bool insufficientWidth = (maxTexcoord.x > callTile.tileCopyWidth);
                const bool insufficientHeight = (maxTexcoord.y > callTile.tileCopyHeight);
                if (insufficientWidth || insufficientHeight) {
                    callTile.tileCopyUsed = false;
                }
                else {
                    callTile.rawTMEM = false;
                }
            }

            // Determine if the use of raw TMEM is still required with the texture coordinates that were found.
            if (callTile.rawTMEM) {
                // FIXME: A safety check to only do this on textures that were determined to be big was added until the
                // correctness of the texcoord determination can be guaranteed to not cause issues elsewhere.
                const bool bigTextureCheck = (callTile.sampleWidth > 0x1000) || (callTile.sampleHeight > 0x1000);
                if (bigTextureCheck) {
                    callTile.sampleWidth = maxTexcoord.x;
                    callTile.sampleHeight = maxTexcoord.y;
                    callTile.rawTMEM = TMEMHasher::requiresRawTMEM(callTile.loadTile, maxTexcoord.x, maxTexcoord.y);
                }
            }

            // Emit warnings for tiles using raw TMEM fallback.
            if (warningsEnabled && callTile.rawTMEM) {
                emitTileWarning(CommandWarning::format("Texture tile #%u: The tile can sample more bytes than what TMEM can hold "
                    "due to using a big mask or big clamp coordinates. Unable to provide a fast path for texture sampling.", t), t);
            }
        }

        // Fill out all the rendering data for the framebuffer pairs that will be uploaded.
        const UserConfiguration::Upscale2D upscale2D = ext.userConfig->upscale2D;
        const bool scaleLOD = ext.enhancementConfig->textureLOD.scale;
        const bool usesHDR = ext.shaderLibrary->usesHDR;
        const std::vector<uint32_t> &faceIndices = workload.drawData.faceIndices;
        const std::vector<int16_t> &posShorts = workload.drawData.posShorts;
        uint32_t faceIndex = uint32_t(workload.drawRanges.faceIndices.first);
        uint32_t rawVertexIndex = uint32_t(workload.drawRanges.triPosFloats.first) / 4;
        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            auto &fbPair = workload.fbPairs[f];
            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                auto &proj = fbPair.projections[p];
                for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                    GameCall &gameCall = proj.gameCalls[d];
                    auto &callDesc = gameCall.callDesc;
                    auto &lerpDesc = gameCall.lerpDesc;
                    auto &debuggerDesc = gameCall.debuggerDesc;
                    auto &meshDesc = gameCall.meshDesc;
                    auto &shaderDesc = gameCall.shaderDesc;
                    lerpDesc.enabled = true;
                    debuggerDesc.highlightColor = 0;

#               if SCRIPT_ENABLED
                    lerpDesc.matchCallback = nullptr;
#               endif

                    if (proj.usesViewport()) {
                        meshDesc.faceIndicesStart = faceIndex;
                        faceIndex += callDesc.triangleCount * 3;
                    }
                    else {
                        meshDesc.rawVertexStart = rawVertexIndex;
                        rawVertexIndex += callDesc.triangleCount * 3;
                    }

                    shaderDesc.colorCombiner = callDesc.colorCombiner;
                    shaderDesc.otherMode = callDesc.otherMode;
                    shaderDesc.flags = {};

                    // Check if the blender uses a standard fog cycle. We override it and indicate it on
                    // the material for the RT path to use its own fog handling.
                    interop::Blender::EmulationRequirements blenderEmuReqs = interop::Blender::checkEmulationRequirements(callDesc.otherMode);
#               ifdef ASSERT_ON_BLENDER_EMULATION
                    assert(blenderEmuReqs.simpleEmulation || (blenderEmuReqs.approximateEmulation != interop::Blender::Approximation::None));
#               endif
                    
                    if (warningsEnabled && !blenderEmuReqs.simpleEmulation && (blenderEmuReqs.approximateEmulation == interop::Blender::Approximation::None)) {
                        CommandWarning warning = CommandWarning::format("Unable to provide a fast rendering path for blender with non-standard behavior. The render mode might not be intentional.");
                        warning.indexType = CommandWarning::IndexType::CallIndex;
                        warning.call.fbPairIndex = f;
                        warning.call.projIndex = p;
                        warning.call.callIndex = d;
                        workload.commandWarnings.emplace_back(warning);
                    }
                    
                    // Describe the shader.
                    const bool forceLinearFiltering = (callDesc.extendedFlags.forceTrueBilerp == G_EX_BILERP_ALL) || ((callDesc.extendedFlags.forceTrueBilerp == G_EX_BILERP_ONLY) && (callDesc.otherMode.textFilt() == G_TF_BILERP));
                    const bool oneCycleHardwareBug = (callDesc.otherMode.cycleType() == G_CYC_1CYCLE);
                    auto &flags = shaderDesc.flags;
                    flags.rect = (proj.type == Projection::Type::Rectangle);
                    flags.linearFiltering = linearFiltering || forceLinearFiltering;
                    flags.usesTexture0 = callDesc.colorCombiner.usesTexture(callDesc.otherMode, 0, oneCycleHardwareBug);
                    flags.usesTexture1 = callDesc.colorCombiner.usesTexture(callDesc.otherMode, 1, oneCycleHardwareBug);
                    flags.blenderApproximation = static_cast<unsigned>(blenderEmuReqs.approximateEmulation);
                    flags.usesHDR = usesHDR;
                    flags.sampleCount = renderFlagSampleCount;

                    // Set whether the LOD should be scaled to the display resolution according to the configuration mode and the extended GBI flags.
                    const bool usesLOD = (callDesc.otherMode.textLOD() == G_TL_LOD);
                    flags.upscaleLOD = usesLOD && (scaleLOD || callDesc.extendedFlags.forceScaleLOD);
                    
                    // Set whether the texture coordinates should be scaled or not based on the current configuration mode and the extended GBI flags.
                    const bool forcedUpscale2D = (flags.rect == 0) || (upscale2D == UserConfiguration::Upscale2D::All) || callDesc.extendedFlags.forceUpscale2D;
                    const bool upscaleIfScaledRect = (upscale2D == UserConfiguration::Upscale2D::ScaledOnly) && flags.rect && !callDesc.identityRectScale();
                    flags.upscale2D = forcedUpscale2D || upscaleIfScaledRect;

                    if (proj.usesViewport()) {
                        flags.culling = (callDesc.geometryMode & callDesc.cullBothMask) != 0;
                        flags.smoothShade = (callDesc.geometryMode & callDesc.shadingSmoothMask) != 0;
                        flags.NoN = callDesc.NoN;
                    }
                    else {
                        flags.culling = false;
                        flags.smoothShade = true;
                        flags.NoN = true;
                    }

                    // Specialize shaders based on their tile parameters.
                    flags.canDecodeTMEM = false;
                    if (callDesc.tileCount > 0) {
                        auto &callTiles = workload.drawData.callTiles;
                        const bool canDoNativeSamplers = (callDesc.tileCount <= 2);
                        for (uint32_t t = 0; t < callDesc.tileCount; t++) {
                            const DrawCallTile &callTile = callTiles[callDesc.tileIndex + t];
                            if (callTile.rawTMEM) {
                                flags.canDecodeTMEM = true;
                            }

                            if (callTile.syncRequired && !callTile.tileCopyUsed) {
                                fbPair.syncRequired = true;
                            }
                        }

                        // If the shader uses LOD, we check if all tiles use the same addressing mode as the first tile so it
                        // can be optimized to not rely on branching to determine how to sample the textures.
                        const auto &rdpTiles = workload.drawData.rdpTiles;
                        const interop::RDPTile &tile0 = rdpTiles[callDesc.tileIndex];
                        if (usesLOD && (callDesc.tileCount > 1)) {
                            bool sameModes = true;
                            for (uint32_t t = 1; (t < callDesc.tileCount) && sameModes; t++) {
                                const interop::RDPTile &tileLevel = rdpTiles[callDesc.tileIndex + t];
                                sameModes = 
                                    (tileLevel.cms == tile0.cms) && 
                                    (tileLevel.cmt == tile0.cmt) && 
                                    (tileLevel.fmt == tile0.fmt) &&
                                    (tileLevel.siz == tile0.siz);
                            }

                            if (sameModes) {
                                flags.dynamicTiles = false;
                                flags.cms0 = tile0.cms;
                                flags.cmt0 = tile0.cmt;
                                flags.cms1 = tile0.cms;
                                flags.cmt1 = tile0.cmt;
                                flags.nativeSampler0 = tile0.nativeSampler;
                                flags.nativeSampler1 = tile0.nativeSampler;
                            }
                            else {
                                flags.dynamicTiles = true;
                                flags.cms0 = flags.cmt0 = 0;
                                flags.cms1 = flags.cmt1 = 0;
                                flags.nativeSampler0 = flags.nativeSampler1 = NATIVE_SAMPLER_NONE;
                            }
                        }
                        else {
                            flags.dynamicTiles = false;
                            flags.cms0 = tile0.cms;
                            flags.cmt0 = tile0.cmt;
                            flags.nativeSampler0 = tile0.nativeSampler;

                            if (callDesc.tileCount > 1) {
                                const interop::RDPTile &tile1 = rdpTiles[callDesc.tileIndex + 1];
                                flags.cms1 = tile1.cms;
                                flags.cmt1 = tile1.cmt;
                                flags.nativeSampler1 = tile1.nativeSampler;
                            }
                            // If there's only one tile available and it can sample TEXEL1 for some reason (e.g. LOD),
                            // the safest approach is to copy over the parameters from the first tile.
                            else {
                                flags.cms1 = tile0.cms;
                                flags.cmt1 = tile0.cmt;
                                flags.nativeSampler1 = tile0.nativeSampler;
                            }
                        }
                    }
                    // Shader doesn't use tiles at all.
                    else {
                        flags.dynamicTiles = false;
                        flags.cms0 = flags.cmt0 = 0;
                        flags.cms1 = flags.cmt1 = 0;
                        flags.nativeSampler0 = flags.nativeSampler1 = NATIVE_SAMPLER_NONE;
                    }

                    // Submit the shader to the cache so compilation can start right away.
                    // Ignore any calls that use fill type, as they're emulated without using a shader.
                    if (callDesc.otherMode.cycleType() != G_CYC_FILL) {
                        ext.rasterShaderCache->submit(shaderDesc);
                    }

                    interop::RenderParams renderParams;
                    renderParams.ccL = shaderDesc.colorCombiner.L;
                    renderParams.ccH = shaderDesc.colorCombiner.H;
                    renderParams.omL = shaderDesc.otherMode.L;
                    renderParams.omH = shaderDesc.otherMode.H;
                    renderParams.flags = shaderDesc.flags;

                    workload.drawData.rdpParams.emplace_back(gameCall.callDesc.rdpParams);
                    workload.drawData.renderParams.emplace_back(renderParams);
                }
            }
        }

        // Ensure there's as many GPU tiles as call tiles available in the vector.
        workload.drawData.gpuTiles.resize(workload.drawData.callTiles.size());

        // Start uploading the entire draw data for all the framebuffer pairs that were processed.
        workload.updateDrawDataRanges();
        workload.uploadDrawData(ext.framebufferGraphicsWorker, ext.drawDataUploader);
        workload.updateOutputBuffers(ext.framebufferGraphicsWorker);

        // Upload the transforms directly.
        ext.transformsUploader->submit(ext.framebufferGraphicsWorker, {
            { workload.drawData.viewProjTransforms.data(), workload.drawRanges.viewProjTransforms, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &workload.drawBuffers.viewProjTransformsBuffer},
            { workload.drawData.worldTransforms.data(), workload.drawRanges.worldTransforms, sizeof(interop::float4x4), RenderBufferFlag::STORAGE, { }, &workload.drawBuffers.worldTransformsBuffer},
        });

        if (renderToRDRAM) {
            // Run RSP processor.
            RSPProcessor::ProcessParams rspParams;
            rspParams.worker = ext.framebufferGraphicsWorker;
            rspParams.drawData = &workload.drawData;
            rspParams.drawBuffers = &workload.drawBuffers;
            rspParams.outputBuffers = &workload.outputBuffers;
            rspParams.prevFrameWeight = 0.0f;
            rspParams.curFrameWeight = 1.0f;
            rspProcessor->process(rspParams);
        }

        // Switch the data ranges.
        uint32_t loadOpCursor = uint32_t(workload.drawRanges.loadOperations.first);
        uint32_t rdpTileCursor = uint32_t(workload.drawRanges.rdpTiles.first);
        uint32_t gpuTileCursor = rdpTileCursor;
        workload.nextDrawDataRanges();

        // Make sure pipelines for the ubershader have been created before rendering. This condition is executed regardless
        // of whether rendering to RAM is active so the workload queue is guaranteed to have the pipelines ready as well.
        ext.rasterShaderCache->shaderUber->waitForPipelineCreation();

        if (renderToRDRAM) {
            const hlslpp::float2 resolutionScale(1.0f, 1.0f);
            RT64::Framebuffer *colorFb = nullptr;
            RT64::Framebuffer *depthFb = nullptr;
            uint32_t colorHeight = 0;
            uint32_t colorWriteWidth = 0;
            uint32_t depthWriteWidth = 0;
            RenderFramebufferKey fbKey;
            RenderTarget *colorTarget = nullptr;
            RenderTarget *depthTarget = nullptr;
            uint32_t rtWidth = 0;
            uint32_t rtHeight = 0;
            uint32_t colorRowStart = 0;
            uint32_t colorRowEnd = 0;
            uint32_t depthRowStart = 0;
            uint32_t depthRowEnd = 0;
            auto getFramebufferPairs = [&](uint32_t f) {
                FramebufferPair &fbPair = workload.fbPairs[f];
                if (!fbPair.drawColorRect.isEmpty()) {
                    const auto &colorImg = fbPair.colorImage;
                    const auto &depthImg = fbPair.depthImage;
                    colorHeight = fbPair.drawColorRect.bottom(true);
                    colorWriteWidth = (colorHeight > 1) ? colorImg.width : std::min(fbPair.drawColorRect.right(true), int32_t(colorImg.width));
                    depthWriteWidth = 0;
                    colorFb = &framebufferManager.get(colorImg.address, colorImg.siz, colorImg.width, colorHeight);
                    depthFb = nullptr;
                    if (fbPair.depthRead || fbPair.depthWrite) {
                        if (!fbPair.drawDepthRect.isEmpty()) {
                            depthWriteWidth = (fbPair.drawDepthRect.bottom(true) > 1) ? colorImg.width : std::min(fbPair.drawDepthRect.right(true), int32_t(colorImg.width));
                        }

                        depthFb = &framebufferManager.get(depthImg.address, G_IM_SIZ_16b, colorImg.width, colorHeight);
                        depthFb->everUsedAsDepth = true;
                    }

                    colorRowStart = fbPair.drawColorRect.top(false);
                    colorRowEnd = fbPair.drawColorRect.bottom(true);
                    depthRowStart = 0;
                    depthRowEnd = 0;
                    if (depthWriteWidth > 0) {
                        depthRowStart = fbPair.drawDepthRect.top(false);
                        depthRowEnd = fbPair.drawDepthRect.bottom(true);
                    }

                    return true;
                }
                else {
                    return false;
                }
            };

            auto getTargetsFromPair = [&](uint32_t f) {
                if (getFramebufferPairs(f)) {
                    fbKey = RenderFramebufferKey();
                    fbKey.colorTargetKey = RenderTargetKey(colorFb->addressStart, colorFb->width, colorFb->siz, Framebuffer::Type::Color);

                    colorTarget = &renderTargetManager.get(fbKey.colorTargetKey);
                    depthTarget = nullptr;

                    // Ensure dimensions are the same for the color and depth targets.
                    rtWidth = std::max(uint32_t(colorFb->width), colorTarget->width);
                    rtHeight = std::max(colorHeight, colorTarget->height);

                    // Retrieve the depth target.
                    if (depthFb != nullptr) {
                        fbKey.depthTargetKey = RenderTargetKey(depthFb->addressStart, depthFb->width, depthFb->siz, Framebuffer::Type::Depth);
                        depthTarget = &renderTargetManager.get(fbKey.depthTargetKey);
                        rtWidth = std::max(rtWidth, depthTarget->width);
                        rtHeight = std::max(rtHeight, depthTarget->height);
                    }

                    return true;
                }
                else {
                    return false;
                }
            };

            uint32_t framebufferIndex = 0;
            auto renderSetup = [&]() {
                scratchFbChangePool.reset();
                ext.framebufferGraphicsWorker->commandList->begin();
                framebufferManager.resetOperations();
                framebufferRenderer->resetFramebuffers(ext.framebufferGraphicsWorker, false, workload.extended.ditherNoiseStrength, renderTargetManager.multisampling);
                framebufferIndex = 0;
            };

            thread_local std::unordered_set<RenderTarget *> resizedTargets;
            thread_local std::vector<std::pair<RenderTarget *, RenderTarget *>> colorDepthPairs;
            uint32_t framebufferPairCursor = 0;
            bool firstSynchronizationPerformed = false;
            auto checkRenderTargetSafety = [&](bool checkPairs) {
                if (checkPairs) {
                    for (auto colorDepthPair : colorDepthPairs) {
                        if (colorDepthPair.second->resize(ext.framebufferGraphicsWorker, colorDepthPair.first->width, colorDepthPair.first->height)) {
                            resizedTargets.emplace(colorDepthPair.second);
                        }
                    }
                }

                for (RenderTarget *renderTarget : resizedTargets) {
                    renderFramebufferManager->destroyAllWithRenderTarget(renderTarget);
                }

                resizedTargets.clear();
            };

            auto renderAndSynchronize = [&](uint32_t maxFramebufferPair) {
                // Preprocess all the framebuffer operations.
                uint32_t pairCursor = framebufferPairCursor;
                while (pairCursor < maxFramebufferPair) {
                    const FramebufferPair &fbPair = workload.fbPairs[pairCursor];
                    framebufferManager.setupOperations(ext.framebufferGraphicsWorker, fbPair.startFbOperations, resolutionScale, renderTargetManager, &resizedTargets);
                    framebufferManager.setupOperations(ext.framebufferGraphicsWorker, fbPair.endFbOperations, resolutionScale, renderTargetManager, &resizedTargets);
                    pairCursor++;
                }

                checkRenderTargetSafety(!resizedTargets.empty());

                // Add all framebuffers for rendering.
                pairCursor = framebufferPairCursor;
                while (pairCursor < maxFramebufferPair) {
                    const FramebufferPair &fbPair = workload.fbPairs[pairCursor];
                    if (getTargetsFromPair(pairCursor)) {
                        RenderFramebufferStorage &fbStorage = renderFramebufferManager->get(fbKey, colorTarget, (depthTarget != nullptr) ? depthTarget : dummyDepthTarget.get());
                        FramebufferRenderer::DrawParams drawParams;
                        drawParams.worker = ext.framebufferGraphicsWorker;
                        drawParams.fbStorage = &fbStorage;
                        drawParams.curWorkload = &workload;
                        drawParams.fbPairIndex = pairCursor;
                        drawParams.fbWidth = fbPair.colorImage.width;
                        drawParams.fbHeight = colorHeight;
                        drawParams.targetWidth = fbPair.colorImage.width;
                        drawParams.targetHeight = colorHeight;
                        drawParams.rasterShaderCache = ext.rasterShaderCache;
                        drawParams.resolutionScale = resolutionScale;
                        drawParams.aspectRatioSource = 1.0f;
                        drawParams.aspectRatioTarget = 1.0f;
                        drawParams.extAspectPercentage = 0.0f;
                        drawParams.horizontalMisalignment = 0.0f;
                        drawParams.rtEnabled = false;
                        drawParams.submissionFrame = workload.submissionFrame;
                        drawParams.deltaTimeMs = 0.0f;
                        drawParams.ubershadersOnly = false;
                        drawParams.fixRectLR = false;
                        drawParams.postBlendNoise = ext.emulatorConfig->dither.postBlendNoise;
                        drawParams.postBlendNoiseNegative = ext.emulatorConfig->dither.postBlendNoiseNegative;
                        drawParams.maxGameCall = UINT_MAX;
                        framebufferRenderer->addFramebuffer(drawParams);
                    }

                    pairCursor++;
                }

                // Synchronize all texture uploads before filling out the GPU tiles and update the texture cache on the framebuffer renderer.
                ext.textureCache->waitForGPUUploads();
                framebufferRenderer->updateTextureCache(ext.textureCache);

                thread_local std::vector<BufferUploader *> bufferUploaders;
                bufferUploaders.clear();

                // Create all GPU tile mappings and upload them.
                bool uploadGPUTiles = (gpuTileCursor < rdpTileCursor);
                if (uploadGPUTiles) {
                    std::pair<size_t, size_t> gpuTileRange;
                    gpuTileRange.first = gpuTileCursor;
                    gpuTileRange.second = rdpTileCursor;
                    framebufferRenderer->createGPUTiles(&workload.drawData.callTiles[gpuTileCursor], rdpTileCursor - gpuTileCursor,
                        &workload.drawData.gpuTiles[gpuTileCursor], &framebufferManager, ext.textureCache, workload.submissionFrame);

                    // Upload the GPU tiles.
                    ext.tilesUploader->submit(ext.framebufferGraphicsWorker, {
                        { workload.drawData.gpuTiles.data(), gpuTileRange, sizeof(interop::GPUTile), RenderBufferFlag::STORAGE, { }, &workload.drawBuffers.gpuTilesBuffer}
                    });

                    gpuTileCursor = rdpTileCursor;

                    // Queue tile uploader.
                    bufferUploaders.emplace_back(ext.tilesUploader);
                }

                // Synchronize the draw data uploaders and the processors the first time.
                RSPProcessor *queuedProcessor = nullptr;
                if (!firstSynchronizationPerformed) {
                    bufferUploaders.emplace_back(ext.drawDataUploader);
                    bufferUploaders.emplace_back(ext.transformsUploader);
                    queuedProcessor = rspProcessor.get();
                    firstSynchronizationPerformed = true;
                }

                // Record all setup previous to drawing any of the recorded framebuffers.
                framebufferRenderer->endFramebuffers(ext.framebufferGraphicsWorker, &workload.drawBuffers, &workload.outputBuffers, false);
                framebufferRenderer->recordSetup(ext.framebufferGraphicsWorker, bufferUploaders, queuedProcessor, nullptr, &workload.outputBuffers, false);

                // Record the command list for the framebuffer pairs.
                pairCursor = framebufferPairCursor;
                while (pairCursor < maxFramebufferPair) {
                    FramebufferPair &fbPair = workload.fbPairs[pairCursor];
                    framebufferManager.recordOperations(ext.framebufferGraphicsWorker, &workload.fbChangePool, &workload.fbStorage, ext.shaderLibrary, ext.textureCache,
                        fbPair.startFbOperations, renderTargetManager, resolutionScale, pairCursor, workload.submissionFrame);

                    if (getTargetsFromPair(pairCursor)) {
                        const auto &colorImg = fbPair.colorImage;
                        const auto &depthImg = fbPair.depthImage;
                        bool colorFormatUpdated = false;
                        if (colorImg.formatChanged) {
                            colorFb->discardLastWrite();
                        }
                        else if (colorFb->isLastWriteDifferent(Framebuffer::Type::Color)) {
                            RenderTargetKey otherColorTargetKey(colorFb->addressStart, colorFb->width, colorFb->siz, colorFb->lastWriteType);
                            RenderTarget &otherColorTarget = renderTargetManager.get(otherColorTargetKey);
                            if (!otherColorTarget.isEmpty()) {
                                const FixedRect &r = colorFb->lastWriteRect;
                                colorTarget->copyFromTarget(ext.framebufferGraphicsWorker, &otherColorTarget, r.left(false), r.top(false), r.width(false, true), r.height(false, true), ext.shaderLibrary);
                                colorFb->discardLastWrite();
                                colorFormatUpdated = true;
                            }
                        }

                        // Determine if it needs to read the entire target from RAM or just a portion of it.
                        if (((colorFb->readHeight == 0) && !colorFormatUpdated) || colorImg.formatChanged) {
                            colorTarget->clearColorTarget(ext.framebufferGraphicsWorker);
                            colorFb->readHeight = 0;
                        }

                        if (colorFb->height > colorFb->readHeight) {
                            uint32_t readRowBytes = colorFb->imageRowBytes(colorImg.width);
                            uint32_t readRowCount = colorFb->height - colorFb->readHeight;
                            uint32_t readFbBytes = readRowBytes * readRowCount;
                            uint32_t storageAddress = colorImg.address + readRowBytes * colorFb->readHeight;
                            workload.fbStorage.store(pairCursor, storageAddress, &RDRAM[storageAddress], readFbBytes);
                            FramebufferChange *colorFbChange = colorFb->readChangeFromStorage(ext.framebufferGraphicsWorker, workload.fbStorage, scratchFbChangePool, Framebuffer::Type::Color,
                                colorImg.fmt, pairCursor, colorFb->readHeight, readRowCount, ext.shaderLibrary);

                            if (colorFbChange != nullptr) {
                                colorTarget->copyFromChanges(ext.framebufferGraphicsWorker, *colorFbChange, colorFb->width, readRowCount, colorFb->readHeight, ext.shaderLibrary);
                            }

                            colorFb->readHeight = colorFb->height;
                        }

                        bool depthResized = false;
                        bool depthFormatUpdated = false;
                        bool depthFbTypeChanged = false;
                        bool depthFbChanged = false;
                        if (depthFb != nullptr) {
                            depthFbTypeChanged = (depthFb->lastWriteType == Framebuffer::Type::Color);

                            if (depthImg.formatChanged) {
                                depthFb->discardLastWrite();
                                depthFormatUpdated = true;
                            }
                            else if (depthFb->isLastWriteDifferent(Framebuffer::Type::Depth)) {
                                RenderTargetKey otherDepthTargetKey(depthFb->addressStart, depthFb->width, depthFb->siz, depthFb->lastWriteType);
                                RenderTarget &otherDepthTarget = renderTargetManager.get(otherDepthTargetKey);
                                if (!otherDepthTarget.isEmpty()) {
                                    const FixedRect &r = depthFb->lastWriteRect;
                                    depthTarget->copyFromTarget(ext.framebufferGraphicsWorker, &otherDepthTarget, r.left(false), r.top(false), r.width(false, true), r.height(false, true), ext.shaderLibrary);
                                    depthFb->discardLastWrite();
                                    depthFormatUpdated = true;
                                }
                            }

                            // Determine if it needs to read the entire target from RAM or just a portion of it.
                            if (((depthFb->readHeight == 0) && !depthFormatUpdated) || depthImg.formatChanged) {
                                depthTarget->clearDepthTarget(ext.framebufferGraphicsWorker);
                                depthFb->readHeight = 0;
                            }

                            if (depthFb->height > depthFb->readHeight) {
                                uint32_t readRowBytes = depthFb->imageRowBytes(colorImg.width);
                                uint32_t readRowCount = depthFb->height - depthFb->readHeight;
                                uint32_t readFbBytes = readRowBytes * readRowCount;
                                uint32_t storageAddress = depthImg.address + readRowBytes * depthFb->readHeight;
                                workload.fbStorage.store(pairCursor, storageAddress, &RDRAM[storageAddress], readFbBytes);
                                FramebufferChange *depthFbChange = depthFb->readChangeFromStorage(ext.framebufferGraphicsWorker, workload.fbStorage, scratchFbChangePool, Framebuffer::Type::Depth,
                                    G_IM_FMT_DEPTH, pairCursor, depthFb->readHeight, readRowCount, ext.shaderLibrary);

                                if (depthFbChange != nullptr) {
                                    depthTarget->copyFromChanges(ext.framebufferGraphicsWorker, *depthFbChange, depthFb->width, readRowCount, depthFb->readHeight, ext.shaderLibrary);
                                    depthFbChanged = true;
                                }

                                depthFb->readHeight = depthFb->height;
                            }
                        }

                        framebufferRenderer->recordFramebuffer(ext.framebufferGraphicsWorker, framebufferIndex++);

                        const uint64_t writeTimestamp = framebufferManager.nextWriteTimestamp();
                        colorFb->lastWriteRect.merge(fbPair.drawColorRect);
                        colorFb->lastWriteType = Framebuffer::Type::Color;

                        const bool depthWrite = (depthFbChanged || depthFbTypeChanged || fbPair.depthWrite) && (depthFb != nullptr);
                        if (depthWrite && (depthWriteWidth > 0)) {
                            depthFb->lastWriteRect.merge(fbPair.drawDepthRect);
                            depthFb->lastWriteType = Framebuffer::Type::Depth;
                        }

                        // Copy results from render targets back to RAM.
                        colorFb->copyRenderTargetToNative(ext.framebufferGraphicsWorker, colorTarget, colorWriteWidth, colorRowStart, colorRowEnd, colorImg.fmt, ditherRandomSeed++, ext.shaderLibrary);

                        if (depthWriteWidth > 0) {
                            depthFb->copyRenderTargetToNative(ext.framebufferGraphicsWorker, depthTarget, depthWriteWidth, depthRowStart, depthRowEnd, G_IM_FMT_DEPTH, ditherRandomSeed++, ext.shaderLibrary);
                        }
                    }

                    framebufferManager.recordOperations(ext.framebufferGraphicsWorker, &workload.fbChangePool, &workload.fbStorage, ext.shaderLibrary, ext.textureCache,
                        fbPair.endFbOperations, renderTargetManager, resolutionScale, pairCursor, workload.submissionFrame);

                    pairCursor++;
                }

                ext.framebufferGraphicsWorker->commandList->end();
                framebufferRenderer->waitForUploaders();
                ext.framebufferGraphicsWorker->execute();
                ext.framebufferGraphicsWorker->wait();

                pairCursor = framebufferPairCursor;
                while (pairCursor < maxFramebufferPair) {
                    if (getFramebufferPairs(pairCursor)) {
                        colorFb->copyNativeToRAM(&RDRAM[colorFb->addressStart], colorWriteWidth, colorRowStart, std::min(colorRowEnd, colorFb->height));

                        if (depthWriteWidth > 0) {
                            depthFb->copyNativeToRAM(&RDRAM[depthFb->addressStart], depthWriteWidth, depthRowStart, std::min(depthRowEnd, depthFb->height));
                        }
                    }

                    pairCursor++;
                }

                framebufferPairCursor = maxFramebufferPair;
            };

            // Before performing any rendering, render targets must be resized to their maximum possible size during the workload.
            resizedTargets.clear();
            colorDepthPairs.clear();

            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                if (getTargetsFromPair(f)) {
                    // Resize the native target buffers.
                    colorFb->nativeTarget.resetBufferHistory();

                    if (depthFb != nullptr) {
                        depthFb->nativeTarget.resetBufferHistory();
                    }

                    // Resize the color target if necessary.
                    if (colorTarget->resize(ext.framebufferGraphicsWorker, rtWidth, rtHeight)) {
                        resizedTargets.emplace(colorTarget);
                        colorFb->readHeight = 0;
                    }

                    // Set up the dummy target used for rendering the depth if no depth framebuffer is active.
                    if (depthFb == nullptr) {
                        if (dummyDepthTarget == nullptr) {
                            dummyDepthTarget = std::make_unique<RenderTarget>(0, Framebuffer::Type::Depth, renderTargetManager.multisampling, renderTargetManager.usesHDR);
                            dummyDepthTarget->setupDepth(ext.framebufferGraphicsWorker, rtWidth, rtHeight);
                        }

                        if ((dummyDepthTarget != nullptr) && dummyDepthTarget->resize(ext.framebufferGraphicsWorker, rtWidth, rtHeight)) {
                            resizedTargets.emplace(dummyDepthTarget.get());
                        }

                        colorDepthPairs.emplace_back(colorTarget, dummyDepthTarget.get());
                    }
                    // Resize the depth target if necessary.
                    else if (depthTarget != nullptr) {
                        colorDepthPairs.emplace_back(colorTarget, depthTarget);

                        if (depthTarget->resize(ext.framebufferGraphicsWorker, rtWidth, rtHeight)) {
                            resizedTargets.emplace(depthTarget);
                            depthFb->readHeight = 0;
                        }
                    }
                }
            }

            checkRenderTargetSafety(true);

            // Indicate to the texture cache the textures must not be deleted.
            ext.textureCache->incrementLock();

            // Perform any preliminar setup before processing the framebuffer pairs.
            renderSetup();

            // Start loading tiles, sampling tiles and drawing the framebuffer pairs as required.
            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                FramebufferPair &fbPair = workload.fbPairs[f];
                const bool gpuCopiesEnabled = ext.emulatorConfig->framebuffer.copyWithGPU;
#       if SYNC_ON_EVERY_FB_PAIR == 0
                if (!gpuCopiesEnabled)
#       endif
                {
                    fbPair.syncRequired = (f > 0);
                }

                if (fbPair.syncRequired) {
                    renderAndSynchronize(f);
                    renderSetup();
                }

                fullSyncFramebufferPairTiles(workload, fbPair, loadOpCursor, rdpTileCursor);
            }

            // Render any remaining batches of framebuffers.
            renderAndSynchronize(workload.fbPairCount);
        }
        else {
            // Process all tiles.
            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                fullSyncFramebufferPairTiles(workload, workload.fbPairs[f], loadOpCursor, rdpTileCursor);
            }

            // Do not render anything, just wait for the data to be uploaded.
            ext.framebufferGraphicsWorker->commandList->begin();
            ext.drawDataUploader->commandListBeforeBarriers(ext.framebufferGraphicsWorker);
            ext.transformsUploader->commandListBeforeBarriers(ext.framebufferGraphicsWorker);
            ext.drawDataUploader->commandListCopyResources(ext.framebufferGraphicsWorker);
            ext.transformsUploader->commandListCopyResources(ext.framebufferGraphicsWorker);
            ext.drawDataUploader->commandListAfterBarriers(ext.framebufferGraphicsWorker);
            ext.transformsUploader->commandListAfterBarriers(ext.framebufferGraphicsWorker);
            ext.framebufferGraphicsWorker->commandList->end();
            ext.drawDataUploader->wait();
            ext.transformsUploader->wait();
            ext.framebufferGraphicsWorker->execute();
            ext.framebufferGraphicsWorker->wait();
            ext.textureCache->waitForGPUUploads();
        }

        // Evict from the texture cache that are too old and should no longer be maintained.
        // The texture manager should also be notified of any hashes that were removed.
        if (ext.textureCache->evict(workloadCounter, evictedTextureHashes)) {
            textureManager.removeHashes(evictedTextureHashes);
        }

        if (renderToRDRAM) {
            // Indicate to the texture cache it's safe to delete the textures if no locks are active.
            ext.textureCache->decrementLock();

            framebufferManager.hashTracking(RDRAM);

            advanceFramebufferRenderer();

            // Indicate the next time a display list is parsed, RDRAM should be checked.
            rdramCheckPending = true;
        }

#   if SCRIPT_ENABLED
        // Notify the current script the frame is about to end and should be processed.
        if (ext.app->currentScript != nullptr) {
            ext.app->currentScript->processFullSync();
            workload.pointLights = scriptLights;
        }
#   endif

        // Store the presentation rate in the workload.
        if (extended.refreshRate != UINT16_MAX) {
            workload.viOriginalRate = extended.refreshRate;
        }
        else {
            workload.viOriginalRate = viHistory.logicalRateFromFactors();
        }

        // Log and reset profilers.
        dlCpuProfiler.end();
        dlCpuProfiler.log();
        screenCpuProfiler.log();
        screenCpuProfiler.reset();

        // Inspect the current workload before submission.
        lastWorkloadIndex = ext.workloadQueue->writeCursor;
        if (ext.userConfig->developerMode) {
            inspect();
        }

        // Add any lights to the workload from the presets that are activated.
        for (const auto &it : lightsLibrary.presetMap) {
            if (!it.second.enabled) {
                continue;
            }

            for (const auto &lightIt : it.second.lightMap) {
                if (!lightIt.second.enabled) {
                    continue;
                }

                workload.pointLights.emplace_back(lightIt.second.description);
            }
        }

        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            FramebufferPair &fbPair = workload.fbPairs[f];
            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                Projection &proj = fbPair.projections[p];
                for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                    GameCall &gameCall = proj.gameCalls[d];
                    auto &extraParams = gameCall.callDesc.extraParams;
                    extraParams.lockMask = 0.0f;
                    extraParams.ignoreNormalFactor = 0.0f;
                    extraParams.uvDetailScale = 1.0f;
                    extraParams.reflectionFactor = 0.0f;
                    extraParams.reflectionFresnelFactor = 1.0f;
                    extraParams.roughnessFactor = 0.0f;
                    extraParams.refractionFactor = 0.0f;
                    extraParams.shadowCatcherFactor = 0.0f;
                    extraParams.specularColor = { 0.5f, 0.5f, 0.5f };
                    extraParams.specularExponent = 5.0f;
                    extraParams.solidAlphaMultiplier = 1.0f;
                    extraParams.shadowAlphaMultiplier = 1.0f;
                    extraParams.diffuseColorMix = { 0.0f, 0.0f, 0.0f, 0.0f };
                    extraParams.depthOrderBias = 0.0f;
                    extraParams.depthDecalBias = 0.0f;
                    extraParams.shadowRayBias = 1.0f;
                    extraParams.selfLight.x = 0.0f;
                    extraParams.selfLight.y = 0.0f;
                    extraParams.selfLight.z = 0.0f;
                    extraParams.lightGroupMaskBits = 0xFFFFFFFF;
                    extraParams.rspLightDiffuseMix = gameConfig.rspLightAsDiffuse ? gameConfig.rspLightIntensity : 0.0f;

                    // TODO: Reimplement proper decals in RT.
                    const float DepthDecalBias = 0.02f;
                    if (gameCall.callDesc.otherMode.zMode() == ZMODE_DEC) {
                        extraParams.depthOrderBias = DepthDecalBias;
                        extraParams.depthDecalBias = DepthDecalBias;
                    }
                    else if (!gameCall.callDesc.otherMode.zCmp() && !gameCall.callDesc.otherMode.zUpd()) {
                        extraParams.selfLight = { 1.0f, 1.0f, 1.0f };
                        extraParams.lightGroupMaskBits = 0;
                    }

                    const DrawCallKey callKey = DrawCallKey::fromDrawCall(gameCall.callDesc, workload.drawData, *ext.textureCache);
                    const auto keyRange = drawCallLibrary.findMaterialsInCache(callKey, materialLibrary);
                    const auto &lightPresetMap = lightsLibrary.presetMap;
                    for (auto it = keyRange.first; it != keyRange.second; it++) {
                        const PresetMaterial &presetMaterial = drawCallLibrary.materialCache[it->second];
                        extraParams.applyExtraAttributes(presetMaterial.description);
                        gameCall.lerpDesc.enabled = presetMaterial.interpolation.enabled;
#                   if SCRIPT_ENABLED
                        gameCall.lerpDesc.matchCallback = presetMaterial.interpolation.callMatchCallback;
#                   endif

                        // TODO: Cache lights as well.
                        const std::string &lightPreset = presetMaterial.light.presetName;
                        if (!lightPreset.empty()) {
                            auto lightIt = lightPresetMap.find(lightPreset);
                            if (lightIt != lightPresetMap.end()) {
                                const auto &lightMap = lightIt->second.lightMap;
                                for (const auto &light : lightMap) {
                                    if (!light.second.enabled) {
                                        continue;
                                    }

                                    interop::PointLight pointLight = light.second.description;
                                    const interop::float4x4 &worldTransform = workload.drawData.worldTransforms[gameCall.callDesc.maxWorldMatrix];
                                    const float lightScale = presetMaterial.light.scale;
                                    pointLight.position.x = pointLight.position.x * lightScale + worldTransform[3][0];
                                    pointLight.position.y = pointLight.position.y * lightScale + worldTransform[3][1];
                                    pointLight.position.z = pointLight.position.z * lightScale + worldTransform[3][2];
                                    pointLight.attenuationRadius *= lightScale;

                                    const interop::float4 &primColor = gameCall.callDesc.rdpParams.primColor;
                                    const interop::float4 &envColor = gameCall.callDesc.rdpParams.envColor;
                                    const float primColorTint = presetMaterial.light.primColorTint;
                                    if (primColorTint > 0.0f) {
                                        const hlslpp::float3 tintColor = hlslpp::lerp(hlslpp::float3(1.0f, 1.0f, 1.0f), hlslpp::float3(primColor.x, primColor.y, primColor.z), primColorTint);
                                        pointLight.diffuseColor = pointLight.diffuseColor * tintColor;
                                        pointLight.specularColor = pointLight.specularColor * tintColor;
                                    }

                                    const float primAlphaAttenuation = presetMaterial.light.primAlphaAttenuation;
                                    if (primAlphaAttenuation > 0.0f) {
                                        const float alpha = 1.0f + (primColor.w - 1.0f) * primAlphaAttenuation;
                                        pointLight.attenuationRadius *= alpha;
                                    }

                                    const float envColorTint = presetMaterial.light.envColorTint;
                                    if (envColorTint > 0.0f) {
                                        const hlslpp::float3 tintColor = hlslpp::lerp(hlslpp::float3(1.0f, 1.0f, 1.0f), hlslpp::float3(envColor.x, envColor.y, envColor.z), envColorTint);
                                        pointLight.diffuseColor = pointLight.diffuseColor * tintColor;
                                        pointLight.specularColor = pointLight.specularColor * tintColor;
                                    }

                                    const float envAlphaAttenuation = presetMaterial.light.envAlphaAttenuation;
                                    if (envAlphaAttenuation > 0.0f) {
                                        const float alpha = 1.0f + (envColor.w - 1.0f) * envAlphaAttenuation;
                                        pointLight.attenuationRadius *= alpha;
                                    }

                                    // TODO: Needs to be associated to an scene.
                                    workload.pointLights.emplace_back(pointLight);
                                }
                            }
                        }
                    }

                    workload.drawData.extraParams.emplace_back(gameCall.callDesc.extraParams);
                }

                if (proj.type == Projection::Type::Perspective) {
                    // Add estimated sun light if enabled.
                    if (gameConfig.estimateSunLight) {
                        proj.addPointLight(proj.lightManager.estimatedSunLight(gameConfig.sunLightIntensity, gameConfig.sunLightDistance));
                    }
                }

#if 0
                // Add all the lights identified from the projection.
                for (const interop::PointLight &light : proj.lightManager.pointLights) {
                    proj.addPointLight(light);
                }
#endif
            }
        }
        
        // Advance the workload queue at the end of a full synchronization.
        advanceWorkload(workload, false);
        ext.workloadQueue->advanceToNextWorkload();

        // Make sure the profiler starts after the workload is advanced to ignore any waiting time.
        dlCpuProfiler.reset();
        dlCpuProfiler.start();
        
        // Submit a presentation event instantly if applicable.
        const bool presentEarly = !ext.presentQueue->viewRDRAM && (ext.enhancementConfig->presentation.mode == EnhancementConfiguration::Presentation::Mode::PresentEarly);
        if (presentEarly && (workload.fbPairCount > 0)) {
            thread_local std::unordered_set<uint32_t> colorImageAddressSet;
            colorImageAddressSet.clear();
            
            bool viPresented = false;
            for (int32_t f = workload.fbPairCount - 1; (f >= 0) && !viPresented; f--) {
                const FramebufferPair &fbPair = workload.fbPairs[f];
                const auto &colorImg = fbPair.colorImage;
                if (colorImageAddressSet.find(colorImg.address) != colorImageAddressSet.end()) {
                    continue;
                }
                else {
                    colorImageAddressSet.insert(colorImg.address);
                }

                if (!fbPair.earlyPresentCandidate()) {
                    continue;
                }

                for (size_t h = 0; (h < viHistory.history.size()) && !viPresented; h++) {
                    const VIHistory::Present &entry = viHistory.history[h];
                    const uint32_t fbAddress = entry.vi.fbAddress();
                    const hlslpp::uint2 fbSize = entry.vi.fbSize();
                    const uint8_t fbSiz = entry.vi.fbSiz();
                    if ((colorImg.address == fbAddress) &&
                        (colorImg.width == fbSize.x) &&
                        (colorImg.siz == fbSiz))
                    {
                        updateScreen(entry.vi, true);
                        viPresented = true;
                    }
                }
            }
        }

        // Reset the next workload.
        workloadCursor = ext.workloadQueue->writeCursor;
        Workload &nextWorkload = ext.workloadQueue->workloads[workloadCursor];
        nextWorkload.begin(workloadCounter++);

        // Indicate on the framebuffer manager that all tile copies are free to be used again.
        framebufferManager.clearUsedTileCopies();

        // Reset the tracked max height of all framebuffers.
        framebufferManager.resetTracking();

        // Extended GBI must be enabled again when the display list starts.
        disableExtendedGBI();
        clearExtended();
    }
    
    void State::updateScreen(const VI &newVI, bool fromEarlyPresent) {
        // If the debugger has paused the plugin, keep submitting the last workload and screen VI for rendering and a present event.
        if (debuggerInspector.paused && !fromEarlyPresent) {
            if (ext.userConfig->developerMode) {
                inspect();
            }
            
            // Wait for the workload and present that has been submitted already to be processed.
            ext.workloadQueue->waitForWorkloadId(workloadId);
            ext.presentQueue->waitForPresentId(presentId);
            ext.workloadQueue->waitForIdle();
            ext.presentQueue->waitForIdle();

            // Re-submit the last workload and present to the queue.
            Workload &lastWorkload = ext.workloadQueue->workloads[ext.workloadQueue->previousWriteCursor()];
            Present &lastPresent = ext.presentQueue->presents[ext.presentQueue->previousWriteCursor()];
            advanceWorkload(lastWorkload, true);
            advancePresent(lastPresent, true);
            ext.workloadQueue->repeatLastWorkload();
            ext.presentQueue->repeatLastPresent();

            if (debuggerInspector.paused) {
                return;
            }
        }
        
        const int presentCursor = ext.presentQueue->writeCursor;
        Present &present = ext.presentQueue->presents[presentCursor];
        present.fbOperations.clear();
        present.storage.clear();

        const bool presentEarly = !ext.presentQueue->viewRDRAM && (ext.enhancementConfig->presentation.mode == EnhancementConfiguration::Presentation::Mode::PresentEarly);
        const uint32_t screenFbAddress = newVI.fbAddress();
        const hlslpp::uint2 screenFbSize = newVI.fbSize();
        const uint8_t screenFbSiz = newVI.fbSiz();
        const bool viVisible = newVI.visible();
        bool viDifferent = false;
        if (!fromEarlyPresent) {
            // Keep last known screen VI updated.
            viDifferent = (newVI != lastScreenVI);
            lastScreenVI = newVI;

            // Push to VI history if it's different.
            if (viDifferent && viVisible) {
                viChangedProfiler.logAndRestart();
                viHistory.pushVI(newVI, screenFbSize.x);
                viHistory.pushFactor(lastScreenFactorCounter + 1);
                lastScreenFactorCounter = 0;
            }
            else if (viVisible) {
                lastScreenFactorCounter++;
            }

            // We ignore processing the updateScreen altogether in early present mode.
            if (presentEarly) {
                return;
            }
        }

        screenCpuProfiler.start();
        bool fbChangesMade = false;
        bool screenChangesMade = false;
        if (newVI.visible()) {
            // See if there's an existing framebuffer that lines up with the VI. If there is, we support reading 
            // CPU changes directly to it and recreating them in the render thread at low resolution.
            RenderWorker *worker = ext.framebufferGraphicsWorker;
            Framebuffer *screenFb = framebufferManager.find(screenFbAddress);
            if (screenFb != nullptr) {
                // Check compatibility of the high resolution framebuffer with the VI first.
                // Ensure both the siz and width are the same.
                if ((screenFbSize.x == screenFb->width) && (screenFbSiz == screenFb->siz)) {
                    const uint8_t *fbRAM = &RDRAM[screenFb->addressStart];
                    uint64_t currentHash = XXH3_64bits(fbRAM, screenFb->RAMBytes);
                    if (currentHash != screenFb->RAMHash) {
                        {
                            RenderWorkerExecution workerExecution(worker);
                            thread_local std::vector<uint32_t> fbDiscards;
                            const std::scoped_lock lock(ext.presentQueue->screenFbChangePoolMutex);
                            framebufferManager.uploadRAM(worker, &screenFb, 1, ext.presentQueue->screenFbChangePool, RDRAM, false, present.fbOperations, fbDiscards, ext.shaderLibrary);
                            fbDiscards.clear();

                            const hlslpp::float2 resolutionScale(1.0f, 1.0f);
                            framebufferManager.performOperations(worker, &ext.presentQueue->screenFbChangePool, nullptr, ext.shaderLibrary, ext.textureCache, present.fbOperations,
                                renderTargetManager, resolutionScale, 0, workloadCounter, nullptr);
                        }

                        // Only update the hash if it's not a discard.
                        if (present.fbOperations.front().type == FramebufferOperation::Type::WriteChanges) {
                            screenFb->RAMHash = currentHash;
                            fbChangesMade = true;
                        }
                        else {
                            present.fbOperations.clear();
                        }
                    }
                }
                else {
                    screenFb = nullptr;
                }
            }

            // Store the RAM required by the VI so the render thread can display it if necessary.
            if (screenFbSiz >= G_IM_SIZ_16b) {
                uint32_t screenFbBytes = uint32_t(screenFbSize.x * screenFbSize.y) << (screenFbSiz - 1);
                present.storage.resize(screenFbBytes);
                memcpy(present.storage.data(), &RDRAM[screenFbAddress], screenFbBytes);
                uint64_t newScreenHash = XXH3_64bits(present.storage.data(), screenFbBytes);
                screenChangesMade = (newScreenHash != lastScreenHash);
                lastScreenHash = newScreenHash;
            }
        }
        
        // We only push a new present event to the timeline when it's necessary.
        if (fromEarlyPresent || viDifferent || fbChangesMade || screenChangesMade) {
            // Push a new renderer event to the timeline for presenting this VI.
            present.screenVI = newVI;
            advancePresent(present, false);
            ext.presentQueue->advanceToNextPresent();

            // This presentation originates from screen changes being made manually by the CPU
            // or the game somehow not doing some form of buffering (instant input romhacks).
            if (!fromEarlyPresent && !viDifferent && !fbChangesMade && screenChangesMade) {
                viHistory.pushFactor(lastScreenFactorCounter);
                lastScreenFactorCounter = 0;
            }
        }

        screenCpuProfiler.end();
    }

    void State::updateMultisampling() {
        const RenderMultisampling multisampling = RasterShader::generateMultisamplingPattern(ext.userConfig->msaaSampleCount(), ext.device->getCapabilities().sampleLocations);
        renderFramebufferManager->destroyAll();
        renderTargetManager.destroyAll();
        renderTargetManager.setMultisampling(multisampling);
        dummyDepthTarget.reset();
        framebufferRenderer->updateMultisampling();
        updateRenderFlagSampleCount();
    }

    void State::inspect() {
        ext.presentQueue->inspectorMutex.lock();
        Inspector *inspector = ext.presentQueue->inspector.get();
        if (inspector == nullptr) {
            ext.presentQueue->inspectorMutex.unlock();
            return;
        }

        enum class InspectorMode {
            None,
            Light,
            Material,
            DrawCall,
            Scene
        };

        UserConfiguration &userConfig = *ext.userConfig;
        EmulatorConfiguration &emulatorConfig = *ext.emulatorConfig;
        EnhancementConfiguration &enhancementConfig = *ext.enhancementConfig;
        bool genConfigChanged = false;
        bool resConfigChanged = false;
        bool enhanceConfigChanged = false;
        bool emulatorConfigChanged = false;
        bool shaderCacheChanged = false;
        const uint32_t previousMSAACount = userConfig.msaaSampleCount();
        Workload &workload = ext.workloadQueue->workloads[lastWorkloadIndex];
        InspectorMode inspectorMode = InspectorMode::None;
        inspector->newFrame(ext.framebufferGraphicsWorker);

        if (ext.app->freeCamClearQueued) {
            debuggerInspector.camera.enabled = false;
            ext.app->freeCamClearQueued = false;
        }

        if (debuggerInspector.camera.enabled) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            cameraController.moveCursor(debuggerInspector.camera, { mousePos.x, mousePos.y }, { displaySize.x, displaySize.y });
        }

        /*
        const float aspectRatio = static_cast<float>(windowWidth) / windowHeight;
        hlslpp::float2 cursorPosF = {
            static_cast<float>(cursorPos.x) / windowWidth,
            static_cast<float>(cursorPos.y) / windowHeight
        };

        hlslpp::float2 cursorNDCPos = {
            (static_cast<float>(cursorPos.x) - windowLeft) / windowWidth * 2.0f - 1.0f,
            1.0f - 2.0f * ((static_cast<float>(cursorPos.y) - windowTop) / windowHeight)
        };

        //projectionInspector.updateView(previousFrame.projMatrix, previousFrame.viewMatrix, previousFrame.invViewMatrix, previousFrame.fovRadians, previousFrame.nearPlane, previousFrame.farPlane);
        //projectionInspector.moveCursor(cursorPosF);
        //
        //// Use the camera data from the projection inspector to set up Im3d.
        //const auto &camControl = projectionInspector.cameraControl;
        //hlslpp::float3 viewPos = camControl.invViewMatrix[3].xyz;
        //hlslpp::float4 viewDir = hlslpp::mul(hlslpp::float4(0.0f, 0.0f, 1.0f, 0.0f), camControl.invViewMatrix);
        //Im3d::Mat4 invView;
        //Im3d::Mat4 invProj;
        //float widthScale = (4.0f / 3.0f) / aspectRatio;
        //memcpy(invView.m, &camControl.invViewMatrix[0][0], sizeof(invView.m));
        //memcpy(invProj.m, &camControl.projMatrix[0][0], sizeof(invProj.m));
        //invProj = Im3d::Inverse(invProj);
        //Im3d::Vec4 rayDir = invProj * Im3d::Vec4(cursorNDCPos.x / widthScale, cursorNDCPos.y, 1.0f, 1.0f);
        //rayDir.w = 0.0f;
        //rayDir = Im3d::Normalize(invView * rayDir);
        //
        //Im3d::AppData &appData = Im3d::GetAppData();
        //appData.m_deltaTime = 1.0f / 30.0f;
        //appData.m_viewportSize = Im3d::Vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        //appData.m_viewOrigin = Im3d::Vec3(viewPos.x, viewPos.y, viewPos.z);
        //appData.m_viewDirection = Im3d::Normalize(Im3d::Vec3(viewDir.x, viewDir.y, viewDir.z));
        //appData.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
        //appData.m_projOrtho = false;
        //appData.m_projScaleY = tanf(camControl.fovRadians * 0.5f) * 2.0f;
        //appData.m_snapTranslation = 0.0f;
        //appData.m_snapRotation = 0.0f;
        //appData.m_snapScale = 0.0f;
        //appData.m_cursorRayOrigin = Im3d::Vec3(viewPos.x, viewPos.y, viewPos.z);
        //appData.m_cursorRayDirection = Im3d::Vec3(rayDir.x, rayDir.y, rayDir.z);
        //appData.m_keyDown[Im3d::Mouse_Left] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        */

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::GetIO().WantCaptureMouse) {
            // We need to figure out the dimensions of where the viewport is being rendered at first.
            ext.sharedQueueResources->configurationMutex.lock();
            const hlslpp::float2 resolutionScale = ext.sharedQueueResources->resolutionScale;
            const uint32_t downsampleMultiplier = ext.userConfig->downsampleMultiplier;
            ext.sharedQueueResources->configurationMutex.unlock();
            RenderViewport viewport;
            RenderRect scissor;
            hlslpp::float2 fbHdRegion;
            VIRenderer::getViewportAndScissor(ext.swapChain, lastScreenVI, resolutionScale, downsampleMultiplier, viewport, scissor, fbHdRegion);

            // Convert the mouse coordinates to native coordinates.
            // FIXME: This needs a lot more work to be compatible with games with less standard VI modes.
            hlslpp::float2 screenCursorPos;
            ImVec2 nativeMousePos = ImGui::GetMousePos();
            const float aspectRatio = resolutionScale.x / resolutionScale.y;
            screenCursorPos.x = ((nativeMousePos.x - (viewport.x + viewport.width / 2)) / viewport.width) * aspectRatio;
            screenCursorPos.y = (nativeMousePos.y - (viewport.y + viewport.height / 2)) / viewport.height;
            screenCursorPos.x = (VI::Width / 4) + screenCursorPos.x * (VI::Width / 2);
            screenCursorPos.y = (VI::Height / 4) + screenCursorPos.y * (VI::Height / 2);
            debuggerInspector.rightClick(workload, screenCursorPos);
        }

        // Check the debugger popup regardless of the active inspector mode.
        bool debuggerSelected = debuggerInspector.checkPopup(workload);

        if (ImGui::Begin("Game editor")) {
            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Configuration")) {
                    // General configuration.
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::Text("User Configuration (persistent)");
                    ImGui::Separator();

                    genConfigChanged = ImGui::Combo("Graphics API", reinterpret_cast<int *>(&userConfig.graphicsAPI), "D3D12\0Vulkan\0Metal\0Automatic\0") || genConfigChanged;

                    if (!UserConfiguration::isGraphicsAPISupported(userConfig.graphicsAPI)) {
                        ImGui::Text("This API is not available on this platform!");
                    }
                    else if (UserConfiguration::resolveGraphicsAPI(userConfig.graphicsAPI) != inspector->graphicsAPI) {
                        ImGui::Text("You must restart the application for this change to be applied.");
                    }

                    resConfigChanged = ImGui::Combo("Resolution Mode", reinterpret_cast<int *>(&userConfig.resolution), "Original\0Window Integer Scale\0Manual\0") || resConfigChanged;
                    const bool manualResolution = (userConfig.resolution == UserConfiguration::Resolution::Manual);
                    if (manualResolution) {
                        resConfigChanged = ImGui::InputDouble("Resolution Multiplier", &userConfig.resolutionMultiplier) || resConfigChanged;
                    }
                    
                    genConfigChanged = ImGui::InputInt("Downsample Multiplier", &userConfig.downsampleMultiplier) || genConfigChanged;
                    
                    ImGui::BeginDisabled(!ext.device->getCapabilities().sampleLocations);
                    const bool usesHDR = ext.shaderLibrary->usesHDR;
                    const RenderSampleCounts sampleCountsSupported = ext.device->getSampleCountsSupported(RenderTarget::colorBufferFormat(usesHDR)) & ext.device->getSampleCountsSupported(RenderTarget::depthBufferFormat());
                    const uint32_t antialiasingOptionCount = uint32_t(UserConfiguration::Antialiasing::OptionCount);
                    const char *antialiasingNames[antialiasingOptionCount] = { "None", "MSAA 2X", "MSAA 4X", "MSAA 8X" };
                    if (ImGui::BeginCombo("Antialiasing", antialiasingNames[uint32_t(userConfig.antialiasing)])) {
                        for (uint32_t i = 0; i < antialiasingOptionCount; i++) {
                            UserConfiguration::Antialiasing antialiasingOption = UserConfiguration::Antialiasing(i);
                            uint32_t sampleCount = UserConfiguration::msaaSampleCount(antialiasingOption);
                            if ((sampleCountsSupported & sampleCount) != 0) {
                                const bool isSelected = (userConfig.antialiasing == antialiasingOption);
                                if (ImGui::Selectable(antialiasingNames[i], isSelected)) {
                                    userConfig.antialiasing = antialiasingOption;
                                    genConfigChanged = true;
                                }

                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::EndDisabled();

                    genConfigChanged = ImGui::Combo("Filtering", reinterpret_cast<int *>(&userConfig.filtering), "Nearest\0Linear\0Pixel Scaling\0") || genConfigChanged;

                    resConfigChanged = ImGui::Combo("Aspect Ratio Mode", reinterpret_cast<int *>(&userConfig.aspectRatio), "Original\0Expand\0Manual\0") || resConfigChanged;
                    const bool manualAspectRatio = (userConfig.aspectRatio == UserConfiguration::AspectRatio::Manual);
                    if (manualAspectRatio) {
                        resConfigChanged = ImGui::InputDouble("Aspect Ratio", &userConfig.aspectTarget) || resConfigChanged;
                    }

                    genConfigChanged = ImGui::Combo("Extended GBI Aspect Ratio Mode", reinterpret_cast<int *>(&userConfig.extAspectRatio), "Original\0Expand\0Manual\0") || genConfigChanged;
                    const bool manualExtAspectRatio = (userConfig.extAspectRatio == UserConfiguration::AspectRatio::Manual);
                    if (manualExtAspectRatio) {
                        genConfigChanged = ImGui::InputDouble("Extended GBI Aspect Ratio", &userConfig.extAspectTarget) || genConfigChanged;
                    }

                    genConfigChanged = ImGui::Combo("Upscale 2D Mode", reinterpret_cast<int *>(&userConfig.upscale2D), "Original\0Scaled Only\0All\0") || genConfigChanged;
                    genConfigChanged = ImGui::Combo("Refresh Rate Mode", reinterpret_cast<int *>(&userConfig.refreshRate), "Original\0Display\0Manual\0") || genConfigChanged;
                    const bool manualRefreshRate = (userConfig.refreshRate == UserConfiguration::RefreshRate::Manual);
                    if (manualRefreshRate) {
                        genConfigChanged = ImGui::InputInt("Refresh Rate Target", &userConfig.refreshRateTarget) || genConfigChanged;
                    }

                    // Store the user configuration that was used during initialization the first time we check this.
                    static UserConfiguration::InternalColorFormat configColorFormat = UserConfiguration::InternalColorFormat::OptionCount;
                    if (configColorFormat == UserConfiguration::InternalColorFormat::OptionCount) {
                        configColorFormat = userConfig.internalColorFormat;
                    }

                    genConfigChanged = ImGui::Combo("Color Format", reinterpret_cast<int *>(&userConfig.internalColorFormat), "Standard\0High\0Automatic\0") || genConfigChanged;
                    if (userConfig.internalColorFormat != configColorFormat) {
                        ImGui::Text("You must restart the application for this change to be applied.");
                    }

                    if (ImGui::Combo("Hardware Resolve", reinterpret_cast<int *>(&userConfig.hardwareResolve), "Disabled\0Enabled\0Automatic\0")) {
                        // Update shader library to automatically control all logic around hardware resolve.
                        ext.shaderLibrary->usesHardwareResolve = (userConfig.hardwareResolve != UserConfiguration::HardwareResolve::Disabled);
                        genConfigChanged = true;
                    }

                    genConfigChanged = ImGui::Checkbox("Three-Point Filtering", &userConfig.threePointFiltering) || genConfigChanged;
                    genConfigChanged = ImGui::Checkbox("High Performance State", &userConfig.idleWorkActive) || genConfigChanged;
                    
                    // Emulator configuration.
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::Text("Emulator Configuration (not persistent)");
                    ImGui::Separator();
                    ImGui::Text("Dither");
                    ImGui::Indent();
                    emulatorConfigChanged = ImGui::Checkbox("Post Blend Noise", &emulatorConfig.dither.postBlendNoise) || emulatorConfigChanged;
                    emulatorConfigChanged = ImGui::Checkbox("Post Blend Noise Negative", &emulatorConfig.dither.postBlendNoiseNegative) || emulatorConfigChanged;
                    ImGui::Unindent();
                    ImGui::Text("Framebuffer");
                    ImGui::Indent();
                    emulatorConfigChanged = ImGui::Checkbox("Render to RAM", &emulatorConfig.framebuffer.renderToRAM) || emulatorConfigChanged;
                    emulatorConfigChanged = ImGui::Checkbox("Copy with GPU", &emulatorConfig.framebuffer.copyWithGPU) || emulatorConfigChanged;
                    ImGui::Unindent();

                    // Enhancement configuration.
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::Text("Enhancement Configuration (not persistent)");
                    ImGui::Separator();
                    ImGui::Text("Framebuffer");
                    ImGui::Indent();
                    ImGui::Checkbox("Reinterpretation Fix ULS", &enhancementConfig.framebuffer.reinterpretFixULS);
                    ImGui::Unindent();
                    ImGui::Text("Presentation");
                    ImGui::Indent();
                    enhanceConfigChanged = ImGui::Combo("Mode##Presentation", reinterpret_cast<int *>(&enhancementConfig.presentation.mode), "Console\0Skip Buffering\0Present Early\0") || enhanceConfigChanged;
                    ImGui::Unindent();
                    ImGui::Text("Rect");
                    ImGui::Indent();
                    enhanceConfigChanged = ImGui::Checkbox("Fix LR with Scissor", &enhancementConfig.rect.fixRectLR) || enhanceConfigChanged;
                    ImGui::Unindent();
                    ImGui::Text("F3DEX");
                    ImGui::Indent();
                    ImGui::Checkbox("Force Branch", &enhancementConfig.f3dex.forceBranch);
                    ImGui::Unindent();
                    ImGui::Text("S2DEX");
                    ImGui::Indent();
                    ImGui::Checkbox("Fix Bilerp Mismatch", &enhancementConfig.s2dex.fixBilerpMismatch);
                    ImGui::Checkbox("Framebuffer Fast Path", &enhancementConfig.s2dex.framebufferFastPath);
                    ImGui::Unindent();
                    ImGui::Text("Textures");
                    ImGui::Indent();
                    enhanceConfigChanged = ImGui::Checkbox("Scale LOD", &enhancementConfig.textureLOD.scale) || enhanceConfigChanged;
                    ImGui::Unindent();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Textures")) {
                    for (const ReplacementDirectory &replacementDirectory : ext.textureCache->textureMap.replacementMap.replacementDirectories) {
                        const std::string replacementPath = replacementDirectory.dirOrZipPath.u8string();
                        ImGui::Text("Texture replacement path: %s", replacementPath.c_str());
                    }

                    ImGui::BeginChild("##textureReplacements", ImVec2(0, -64));
                    ImGui::EndChild();

                    const bool loadPack = ImGui::Button("Load pack");
                    ImGui::SameLine();
                    const bool loadDirectory = ImGui::Button("Load directory");
                    ImGui::SameLine();
                    const bool saveDirectory = ImGui::Button("Save directory");
                    ImGui::SameLine();
                    const bool dumpTextures = ImGui::Button(dumpingTexturesDirectory.empty() ? "Start dumping textures" : "Stop dumping textures");
                    if (loadPack) {
                        std::filesystem::path newPack = FileDialog::getOpenFilename({ FileFilter("RTZ Files", "rtz") });
                        if (!newPack.empty()) {
                            ext.textureCache->loadReplacementDirectory(ReplacementDirectory(newPack));
                        }
                    }
                    else if (loadDirectory) {
                        std::filesystem::path newDirectory = FileDialog::getDirectoryPath();
                        if (!newDirectory.empty()) {
                            ext.textureCache->loadReplacementDirectory(ReplacementDirectory(newDirectory));
                        }
                    }
                    else if (saveDirectory) {
                        ext.textureCache->saveReplacementDatabase();
                    }
                    else if (dumpTextures) {
                        if (dumpingTexturesDirectory.empty()) {
                            dumpingTexturesDirectory = FileDialog::getDirectoryPath();
                            textureManager.dumpedSet.clear();
                        }
                        else {
                            dumpingTexturesDirectory.clear();
                        }
                    }

                    if (ext.textureCache->textureMap.replacementMap.fileSystemIsDirectory) {
                        ImGui::SameLine();
                        const bool removeUnused = ImGui::Button("Remove unused entries");
                        if (removeUnused) {
                            ext.textureCache->removeUnusedEntriesFromDatabase();
                        }
                    }

                    if (!ext.textureCache->textureMap.replacementMap.replacementDirectories.empty()) {
                        ImGui::SameLine();
                        const bool unloadTextures = ImGui::Button("Unload textures");
                        if (unloadTextures) {
                            ext.textureCache->clearReplacementDirectories();
                        }
                    }

                    const bool loadPacks = ImGui::Button("Load packs");
                    ImGui::SameLine();
                    const bool loadDirectories = ImGui::Button("Load directories");
                    ImGui::SameLine();
                    if (loadPacks) {
                        // Ask for packs until the user cancels it.
                        std::vector<ReplacementDirectory> replacementDirectories;
                        std::filesystem::path newPack;
                        do {
                            newPack = FileDialog::getOpenFilename({ FileFilter("RTZ Files", "rtz") });
                            if (!newPack.empty()) {
                                replacementDirectories.emplace_back(ReplacementDirectory(newPack));
                            }
                        } while (!newPack.empty());

                        if (!replacementDirectories.empty()) {
                            ext.textureCache->loadReplacementDirectories(replacementDirectories);
                        }
                    }
                    else if (loadDirectories) {
                        // Ask for directories until the user cancels it.
                        std::vector<ReplacementDirectory> replacementDirectories;
                        std::filesystem::path newDirectory;
                        do {
                            newDirectory = FileDialog::getDirectoryPath();
                            if (!newDirectory.empty()) {
                                replacementDirectories.emplace_back(ReplacementDirectory(newDirectory));
                            }
                        } while (!newDirectory.empty());

                        if (!replacementDirectories.empty()) {
                            ext.textureCache->loadReplacementDirectories(replacementDirectories);
                        }
                    }

                    ImGui::EndTabItem();
                }

#           if SCRIPT_ENABLED
                if (ImGui::BeginTabItem("Script")) {
                    ImGui::BeginChild("##script");
                    ImGui::Text("Script log");
                    ImGui::Indent();
                    ImGui::BeginChild("##scriptConsole");
                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(scriptLog.str().c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndChild();
                    ImGui::Unindent();
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
#           endif

#           if RT_ENABLED
                if (ImGui::BeginTabItem("Scenes")) {
                    sceneLibraryInspector.inspectLibrary(sceneLibrary, ext.appWindow->windowHandle);
                    inspectorMode = !sceneLibraryInspector.selectedPresetName.empty() ? InspectorMode::Scene : InspectorMode::None;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Lights")) {
                    lightsLibraryInspector.inspectLibrary(lightsLibrary, ext.appWindow->windowHandle);
                    inspectorMode = !lightsLibraryInspector.selectedPresetName.empty() ? InspectorMode::Light : InspectorMode::None;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Materials")) {
                    materialLibraryInspector.inspectLibrary(materialLibrary, ext.appWindow->windowHandle);
                    inspectorMode = !materialLibraryInspector.selectedPresetName.empty() ? InspectorMode::Material : InspectorMode::None;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Draw calls", nullptr, drawCallLibraryInspector.newPresetRequested ? ImGuiTabItemFlags_SetSelected : 0)) {
                    drawCallLibraryInspector.inspectLibrary(drawCallLibrary, ext.appWindow->windowHandle, *ext.textureCache);
                    inspectorMode = !drawCallLibraryInspector.selectedPresetName.empty() ? InspectorMode::DrawCall : InspectorMode::None;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Game")) {
                    ImGui::Checkbox("Estimate Sun", &gameConfig.estimateSunLight);
                    ImGui::BeginDisabled(!gameConfig.estimateSunLight);
                    ImGui::DragFloat("Sun Intensity", &gameConfig.sunLightIntensity, 0.01f, 0.0f, FLT_MAX);
                    ImGui::DragFloat("Sun Distance", &gameConfig.sunLightDistance, 1.0f, 0.0f, FLT_MAX);
                    ImGui::EndDisabled();
                    ImGui::Checkbox("RSP Lights as Diffuse", &gameConfig.rspLightAsDiffuse);
                    ImGui::BeginDisabled(!gameConfig.rspLightAsDiffuse);
                    ImGui::DragFloat("RSP Lights Intensity", &gameConfig.rspLightIntensity, 0.01f, 0.0f, FLT_MAX);
                    ImGui::EndDisabled();

                    ImGui::EndTabItem();
                }
#           endif

                if (ImGui::BeginTabItem("Debugger", nullptr, debuggerSelected ? ImGuiTabItemFlags_SetSelected : 0)) {
                    DrawCallKey callKey;
                    bool createCallMod;
                    debuggerInspector.inspect(ext.framebufferGraphicsWorker, lastScreenVI, workload, framebufferManager, *ext.textureCache, callKey, createCallMod, ext.appWindow->windowHandle);
                    if (createCallMod) {
                        drawCallLibraryInspector.promptForNew(callKey);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Render")) {
                    if (ImPlot::BeginPlot("Frametimes")) {
                        const double FrametimeLimit = 20.0;
                        const int Stride = static_cast<int>(sizeof(double));
                        const auto &presentProfiler = ext.presentQueue->presentProfiler;
                        const auto &rendererProfiler = ext.workloadQueue->rendererProfiler;
                        const auto &matchingProfiler = ext.workloadQueue->matchingProfiler;
                        const auto &workloadProfiler = ext.workloadQueue->workloadProfiler;
                        const auto &dlApiProfiler = *ext.dlApiProfiler;
                        const auto &screenApiProfiler = *ext.screenApiProfiler;
                        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, FrametimeLimit);
                        ImPlot::SetupAxis(ImAxis_Y1, "ms", ImPlotAxisFlags_AutoFit);
                        ImPlot::PlotLine<double>("Present", presentProfiler.data(), static_cast<int>(presentProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, presentProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Renderer", rendererProfiler.data(), static_cast<int>(rendererProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, rendererProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Matching", matchingProfiler.data(), static_cast<int>(matchingProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, matchingProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Workload", workloadProfiler.data(), static_cast<int>(workloadProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, workloadProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Display List (API)", dlApiProfiler.data(), static_cast<int>(dlApiProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, dlApiProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Display List (CPU)", dlCpuProfiler.data(), static_cast<int>(dlCpuProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, dlCpuProfiler.index(), Stride);
                        ImPlot::HideNextItem();
                        ImPlot::PlotLine<double>("Update Screen (API)", screenApiProfiler.data(), static_cast<int>(screenApiProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, screenApiProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Update Screen (VI Changed)", viChangedProfiler.data(), static_cast<int>(viChangedProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, viChangedProfiler.index(), Stride);
                        ImPlot::PlotLine<double>("Update Screen (CPU)", screenCpuProfiler.data(), static_cast<int>(screenCpuProfiler.size()), 1.0, 0.0, ImPlotLineFlags_None, screenCpuProfiler.index(), Stride);
                        ImPlot::EndPlot();
                        
                        const double averagePresent = presentProfiler.average();
                        const double averageRenderer = rendererProfiler.average();
                        const double averageMatching = matchingProfiler.average();
                        const double averageWorkload = workloadProfiler.average();
                        const double dlApiProfilerAverage = dlApiProfiler.average();
                        const double dlCpuProfilerAverage = dlCpuProfiler.average();
                        const double screenApiProfilerAverage = screenApiProfiler.average();
                        const double viChangedProfilerAverage = viChangedProfiler.average();
                        const double screenCpuProfilerAverage = screenCpuProfiler.average();
                        const double textureStreamAverage = ext.textureCache->getAverageStreamLoadTime() / 1000.0;
                        ImGui::Text("Average Present (OS): %fms (%.1f FPS)\n", averagePresent, 1000.0 / averagePresent);
                        ImGui::Text("Average Renderer: %fms (%.1f FPS)\n", averageRenderer, 1000.0 / averageRenderer);
                        ImGui::Text("Average Matching (CPU): %fms (%.1f FPS)\n", averageMatching, 1000.0 / averageMatching);
                        ImGui::Text("Average Workload: %fms (%.1f FPS)\n", averageWorkload, 1000.0 / averageWorkload);
                        ImGui::Text("Average Display List (API): %fms (%.1f FPS)\n", dlApiProfilerAverage, 1000.0 / dlApiProfilerAverage);
                        ImGui::Text("Average Display List (CPU): %fms (%.1f FPS)\n", dlCpuProfilerAverage, 1000.0 / dlCpuProfilerAverage);
                        ImGui::Text("Average Update Screen (API): %fms (%.1f FPS)\n", screenApiProfilerAverage, 1000.0 / screenApiProfilerAverage);
                        ImGui::Text("Average Update Screen (VI Changed): %fms (%.1f FPS)\n", viChangedProfilerAverage, 1000.0 / viChangedProfilerAverage);
                        ImGui::Text("Average Update Screen (CPU): %fms (%.1f FPS)\n", screenCpuProfilerAverage, 1000.0 / screenCpuProfilerAverage);

                        // Show texture replacement statistics.
                        uint64_t poolUsed, poolCached, poolLimit;
                        double megabyteSize = 1024.0 * 1024.0;
                        ext.textureCache->getReplacementPoolStats(poolUsed, poolCached, poolLimit);
                        ImGui::NewLine();
                        ImGui::Text("Texture Pool Used: %.1f MB\n", double(poolUsed) / megabyteSize);
                        ImGui::Text("Texture Pool Cached: %.1f MB\n", double(poolCached) / megabyteSize);
                        ImGui::Text("Texture Pool Limit: %1.f MB\n", double(poolLimit) / megabyteSize);
                        ImGui::Text("Average Texture Stream: %fms\n", textureStreamAverage);
                    }

                    bool changed = false;
#               if RT_ENABLED
                    RaytracingConfiguration &rtConfig = *ext.rtConfig;
                    changed = ImGui::DragInt("DI samples", &rtConfig.diSamples, 0.1f, 0, 32) || changed;
                    changed = ImGui::DragInt("GI samples", &rtConfig.giSamples, 0.1f, 0, 32) || changed;
                    changed = ImGui::DragInt("Max lights", &rtConfig.maxLights, 0.1f, 0, 16) || changed;
                    changed = ImGui::DragInt("Max reflections", &rtConfig.maxReflections, 0.1f, 0, 32) || changed;
                    changed = ImGui::DragFloat("Motion blur strength", &rtConfig.motionBlurStrength, 0.1f, 0.0f, 10.0f) || changed;
                    changed = ImGui::DragInt("Motion blur samples", &rtConfig.motionBlurSamples, 0.1f, 0, 256) || changed;

                    int visualizationMode = static_cast<int>(rtConfig.visualizationMode);
                    changed = ImGui::Combo("Visualization Mode", &visualizationMode, "Final\0Shading position\0Shading normal\0Shading specular\0"
                        "Color\0Instance ID\0Direct light raw\0Direct light filtered\0Indirect light raw\0Indirect light filtered\0"
                        "Reflection\0Refraction\0Transparent\0Motion vectors\0Reactive mask\0Lock mask\0Depth\0") || changed;

                    rtConfig.visualizationMode = static_cast<interop::VisualizationMode>(visualizationMode);

                    int upscaleMode = static_cast<int>(rtConfig.upscalerMode);
                    changed = ImGui::Combo("Upscale Mode", &upscaleMode, "Bilinear\0AMD FSR 2\0NVIDIA DLSS\0Intel XeSS\0") || changed;
                    rtConfig.upscalerMode = static_cast<UpscaleMode>(upscaleMode);

                    // TODO: Retrieve this by interacting with the game renderer thread somehow.
                    //if (getUpscalerInitialized(rtConfig.upscalerMode))
                    if (true) {
                        if (rtConfig.upscalerMode != RT64::UpscaleMode::Bilinear) {
                            int upscalerQualityMode = static_cast<int>(rtConfig.upscalerQualityMode);
                            changed = ImGui::Combo("Quality", &upscalerQualityMode, "Ultra Performance\0Performance\0Balanced\0Quality\0Ultra Quality\0Native\0Auto\0") || changed;
                            rtConfig.upscalerQualityMode = static_cast<Upscaler::QualityMode>(upscalerQualityMode);

                            if (rtConfig.upscalerMode != RT64::UpscaleMode::XeSS) {
                                changed = ImGui::DragFloat("Sharpness", &rtConfig.upscalerSharpness, 0.01f, -1.0f, 1.0f) || changed;
                            }

                            changed = ImGui::Checkbox("Resolution Override", &rtConfig.upscalerResolutionOverride) || changed;
                            if (rtConfig.upscalerResolutionOverride) {
                                ImGui::SameLine();
                                changed = ImGui::DragFloat("Resolution Multiplier", &rtConfig.resolutionScale, 0.01f, 0.01f, 1.0f) || changed;
                            }

                            if (rtConfig.upscalerMode == RT64::UpscaleMode::FSR) {
                                changed = ImGui::Checkbox("Reactive Mask", &rtConfig.upscalerReactiveMask) || changed;
                            }

                            changed = ImGui::Checkbox("Lock Mask", &rtConfig.upscalerLockMask) || changed;

                        }
                        else {
                            changed = ImGui::DragFloat("Resolution Multiplier", &rtConfig.resolutionScale, 0.01f, 0.01f, 2.0f) || changed;
                        }
                    }
                    else {
                        changed = ImGui::DragFloat("Resolution Multiplier", &rtConfig.resolutionScale, 0.01f, 0.01f, 2.0f) || changed;
                    }

                    changed = ImGui::Checkbox("Denoiser", &rtConfig.denoiserEnabled) || changed;

                    if (changed) {
                        ext.sharedQueueResources->setRtConfig(rtConfig);
                    }
#               endif
                    ImGui::NewLine();
                    ImGui::Text("Specialized Shaders: %u", ext.rasterShaderCache->shaderCount());
                    ImGui::NewLine();

                    bool ubershadersOnly = ext.workloadQueue->ubershadersOnly;
                    ImGui::Checkbox("Ubershaders Only", &ubershadersOnly);
                    ext.workloadQueue->ubershadersOnly = ubershadersOnly;

                    bool ubershadersVisible = ext.workloadQueue->ubershadersVisible;
                    ImGui::Checkbox("Ubershaders Visible", &ubershadersVisible);
                    ext.workloadQueue->ubershadersVisible = ubershadersVisible;

                    ImGui::NewLine();

                    // Show the current graphics API in use.
                    switch (inspector->graphicsAPI) {
                    case UserConfiguration::GraphicsAPI::D3D12:
                        ImGui::Text("Graphics API: D3D12");
                        break;
                    case UserConfiguration::GraphicsAPI::Vulkan:
                        ImGui::Text("Graphics API: Vulkan");
                        break;
                    case UserConfiguration::GraphicsAPI::Metal:
                        ImGui::Text("Graphics API: Metal");
                        break;
                    default:
                        ImGui::Text("Graphics API: Unknown");
                        break;
                    }

                    const RenderDeviceDescription &description = ext.device->getDescription();
                    const RenderDeviceCapabilities &capabilities = ext.device->getCapabilities();
                    ImGui::Text("Display Refresh Rate (OS): %d\n", ext.appWindow->getRefreshRate());
                    if (capabilities.displayTiming) {
                        ImGui::Text("Display Refresh Rate (RHI): %d\n", ext.swapChain->getRefreshRate());
                    }

                    ImGui::Text("Vendor ID: 0x%08x", uint32_t(description.vendor));
                    ImGui::Text("Driver Version: 0x%016" PRIx64, description.driverVersion);
                    ImGui::Text("Raytracing: %d", capabilities.raytracing);
                    ImGui::Text("Raytracing State Update: %d", capabilities.raytracingStateUpdate);
                    ImGui::Text("Sample Locations: %d", capabilities.sampleLocations);
                    ImGui::Text("Descriptor Indexing: %d", capabilities.descriptorIndexing);
                    ImGui::Text("Scalar Block Layout: %d", capabilities.scalarBlockLayout);
                    ImGui::Text("Present Wait: %d", capabilities.presentWait);
                    ImGui::Text("Display Timing: %d", capabilities.displayTiming);
                    ImGui::Text("Prefer HDR: %d", capabilities.preferHDR);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::End();

        if (inspectorMode != InspectorMode::None) {
            if (ImGui::Begin("Inspector")) {
                switch (inspectorMode) {
                case InspectorMode::Light:
                    lightsLibraryInspector.inspectSelection(lightsLibrary);
                    break;
                case InspectorMode::Material:
                    if (materialLibraryInspector.inspectSelection(materialLibrary, lightsLibrary)) {
                        const std::string materialName = materialLibraryInspector.selectedPresetName;
                        auto it = materialLibrary.presetMap.find(materialName);
                        if (it != materialLibrary.presetMap.end()) {
                            drawCallLibrary.updateMaterialInCache(it->first, it->second);
                        }
                    }

                    break;
                case InspectorMode::DrawCall:
                    drawCallLibraryInspector.inspectSelection(drawCallLibrary, materialLibrary);
                    break;
                case InspectorMode::Scene:
                    sceneLibraryInspector.inspectSelection(sceneLibrary);
                    break;
                default:
                    ImGui::Text("Nothing selected");
                    break;
                }
            }

            ImGui::End();
        }

        inspector->endFrame();
        ext.presentQueue->inspectorMutex.unlock();

        // Everything must be recreated to support the new multisampling configuration, so we delegate this to the application instead.
        if (userConfig.msaaSampleCount() != previousMSAACount) {
            ext.app->updateMultisampling();
        }
        else if (shaderCacheChanged) {
            ext.app->destroyShaderCache();
        }

        // Update renderer configuration.
        const bool configChanged = genConfigChanged || resConfigChanged;
        if (configChanged) {
            configurationSaveQueued = true;
            userConfig.validate();
            ext.sharedQueueResources->setUserConfig(userConfig, resConfigChanged);
        }

        if (enhanceConfigChanged) {
            ext.sharedQueueResources->setEnhancementConfig(enhancementConfig);
        }

        if (emulatorConfigChanged) {
            ext.sharedQueueResources->setEmulatorConfig(emulatorConfig);
        }
    }

    void State::advanceWorkload(Workload &workload, bool paused) {
        workload.workloadId = ++workloadId;
        workload.presentId = presentId;
        workload.debuggerCamera = debuggerInspector.camera;
        workload.debuggerRenderer = debuggerInspector.renderer;
        workload.paused = paused;
    }

    void State::advancePresent(Present &present, bool paused) {
        present.presentId = ++presentId;
        present.workloadId = workloadId;
        present.paused = paused;
        present.debuggerFramebuffer.address = debuggerInspector.renderer.framebufferAddress;
        present.debuggerFramebuffer.view = (debuggerInspector.renderer.framebufferIndex >= 0);
    }

    void State::dpInterrupt() {
        *MI_INTR_REG |= MI_INTR_DP;
        checkInterrupts();
    }

    void State::spInterrupt() {
        *MI_INTR_REG |= MI_INTR_SP;
        checkInterrupts();
    }

    void State::advanceFramebufferRenderer() {
        framebufferRenderer->advanceFrame(false);
    }

    void State::flushFramebufferOperations(FramebufferPair &fbPair) {
        if (!drawFbOperations.empty()) {
            fbPair.startFbOperations.insert(fbPair.startFbOperations.end(), drawFbOperations.begin(), drawFbOperations.end());
            drawFbOperations.clear();
        }

        if (!drawFbDiscards.empty()) {
            fbPair.startFbDiscards.insert(fbPair.startFbDiscards.end(), drawFbDiscards.begin(), drawFbDiscards.end());
            drawFbDiscards.clear();
        }
    }

    bool State::hasFramebufferOperationsPending() const {
        return !drawFbOperations.empty() || !drawFbDiscards.empty();
    }

    void State::pushReturnAddress(DisplayList *dl) {
        assert(dl != nullptr);
        returnAddressStack.push_back(dl);
    }

    DisplayList *State::popReturnAddress() {
        if (returnAddressStack.empty()) {
            return nullptr;
        }
        else {
            DisplayList *dl = returnAddressStack.back();
            returnAddressStack.pop_back();
            return dl;
        }
    }

    void State::setRefreshRate(uint16_t refreshRate) {
        extended.refreshRate = refreshRate;
    }

    void State::setRenderToRAM(uint8_t renderToRAM) {
        extended.renderToRAM = renderToRAM;
    }

    void State::setDitherNoiseStrength(float noiseStrength) {
        extended.ditherNoiseStrength = noiseStrength;
    }
    
    void State::setExtendedRDRAM(bool isExtended) {
        extended.extendRDRAM = isExtended;
    }

    void State::startSpriteCommand(uint64_t replacementHash) {
        if (replacementHash != activeSpriteCommand.replacementHash) {
            flush();
            activeSpriteCommand.callCount = 0;
            activeSpriteCommand.replacementHash = replacementHash;
        }
    }

    void State::endSpriteCommand() {
        if (activeSpriteCommand.replacementHash != 0) {
            flush();

            if (activeSpriteCommand.callCount > 0) {
                const int workloadCursor = ext.workloadQueue->writeCursor;
                Workload &workload = ext.workloadQueue->workloads[workloadCursor];
                workload.spriteCommands.emplace_back(activeSpriteCommand);
            }

            activeSpriteCommand.replacementHash = 0;
        }
    }

    uint8_t *State::fromRDRAM(uint32_t rdramAddress) const {
        return &RDRAM[rdramAddress];
    }

    void State::dumpRDRAM(const std::string &path) {
        FILE *fp = fopen(path.c_str(), "wb");
        fwrite(RDRAM, RDRAMSize, 1, fp);
        fclose(fp);
    }

    void State::enableExtendedGBI(uint8_t opCode) {
        ext.interpreter->extendedOpCode = opCode;
    }

    void State::disableExtendedGBI() {
        ext.interpreter->extendedOpCode = 0;
    }

    void State::clearExtended() {
        extended = Extended();
        rsp->clearExtended();
        rdp->clearExtended();
    }

    void State::updateRenderFlagSampleCount() {
        switch (renderTargetManager.multisampling.sampleCount) {
        case RenderSampleCount::COUNT_1:
            renderFlagSampleCount = 0;
            break;
        case RenderSampleCount::COUNT_2:
            renderFlagSampleCount = 1;
            break;
        case RenderSampleCount::COUNT_4:
            renderFlagSampleCount = 2;
            break;
        case RenderSampleCount::COUNT_8:
            renderFlagSampleCount = 3;
            break;
        default:
            assert(false && "Unknown sample count.");
            break;
        }
    }
};