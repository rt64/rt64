//
// RT64
//

#include "rt64_framebuffer_manager.h"

#include <cassert>
#include <algorithm>

#include "xxHash/xxh3.h"

#include "common/rt64_common.h"
#include "gbi/rt64_f3d.h"
#include "hle/rt64_rdp.h"
#include "render/rt64_render_target.h"
#include "render/rt64_render_worker.h"

namespace RT64 {
    static void fixSizeToMultiple(uint32_t &width, uint32_t &height) {
        const uint32_t SizeMultiple = 32;
        width = ((width + SizeMultiple - 1) / SizeMultiple) * SizeMultiple;
        height = ((height + SizeMultiple - 1) / SizeMultiple) * SizeMultiple;
    }

    // FramebufferManager
    
    FramebufferManager::FramebufferManager() {
        writeTimestamp = 0;
    }

    FramebufferManager::~FramebufferManager() { }
    
    Framebuffer &FramebufferManager::get(uint32_t address, uint8_t siz, uint32_t width, uint32_t height) {
        auto &fb = framebuffers[address];
        fb.widthChanged = (fb.width != width);
        fb.sizChanged = (fb.siz != siz);
        
        if (fb.widthChanged || fb.sizChanged) {
            fb.maxHeight = height;
            fb.readHeight = 0;
        }
        else {
            fb.maxHeight = std::max(fb.maxHeight, height);
        }

        fb.height = fb.maxHeight;
        fb.siz = siz;
        fb.width = width;
        fb.addressStart = address;
        fb.addressEnd = fb.addressStart + fb.imageRowBytes(width) * fb.height;
        return fb;
    }

    Framebuffer *FramebufferManager::find(uint32_t address) const {
        auto it = framebuffers.find(address);
        if (it != framebuffers.end()) {
            return const_cast<Framebuffer *>(&it->second);
        }
        else {
            return nullptr;
        }
    }
    
    Framebuffer *FramebufferManager::findMostRecentContaining(uint32_t addressStart, uint32_t addressEnd) {
        Framebuffer *mostRecent = nullptr;
        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            if (it->second.overlaps(addressStart, addressEnd)) {
                if (mostRecent != nullptr) {
                    // Prioritize FBs with newer timestamps.
                    if (it->second.lastWriteTimestamp >= mostRecent->lastWriteTimestamp) {
                        mostRecent = &it->second;
                    }
                    // Prioritize FBs that fully contain the range.
                    else if ((it->second.lastWriteTimestamp == mostRecent->lastWriteTimestamp) && (it->second.contains(addressStart, addressEnd) && !mostRecent->contains(addressStart, addressEnd))) {
                        mostRecent = &it->second;
                    }
                }
                else {
                    mostRecent = &it->second;
                }
            }

            it++;
        }

        return mostRecent;
    }
    
    void FramebufferManager::writeChanges(RenderWorker *renderWorker, const FramebufferChangePool &fbChangePool, const FramebufferOperation &op, 
        RenderTargetManager &targetManager, const ShaderLibrary *shaderLibrary)
    {
        auto it = framebuffers.find(op.writeChanges.address);
        if ((it != framebuffers.end()) && (it->second.lastWriteType != Framebuffer::Type::None)) {
            const FramebufferChange *fbChange = fbChangePool.get(op.writeChanges.id);
            assert(fbChange != nullptr);
            RenderTargetKey targetKey(it->second.addressStart, it->second.width, it->second.siz, it->second.lastWriteType);
            RenderTarget &target = targetManager.get(targetKey);
            if (!target.isEmpty()) {
                target.copyFromChanges(renderWorker, *fbChange, it->second.width, it->second.height, 0, shaderLibrary);
            }
        }
    }

    void FramebufferManager::createTileCopySetup(RenderWorker *renderWorker, const FramebufferOperation &op, hlslpp::float2 resolutionScale, RenderTargetManager &targetManager, std::unordered_set<RenderTarget *> *resizedTargets) {
        TileCopy &tileCopy = tileCopies[op.createTileCopy.id];
        auto fbIt = framebuffers.find(op.createTileCopy.address);
        if (fbIt == framebuffers.end()) {
            tileCopy.ignore = true;
            return;
        }
        
        const FramebufferTile &fbTile = op.createTileCopy.fbTile;
        const uint32_t tileWidth = std::clamp<long>(lround((fbTile.right - fbTile.left) * resolutionScale.x), 1L, RenderTarget::MaxDimension);
        const uint32_t tileHeight = std::clamp<long>(lround((fbTile.bottom - fbTile.top) * resolutionScale.y), 1L, RenderTarget::MaxDimension);
        tileCopy.id = op.createTileCopy.id;
        tileCopy.address = op.createTileCopy.address;
        tileCopy.usedWidth = tileWidth;
        tileCopy.usedHeight = tileHeight;
        tileCopy.left = std::clamp<long>(lround(fbTile.left * resolutionScale.x), 0, RenderTarget::MaxDimension);
        tileCopy.top = std::clamp<long>(lround(fbTile.top * resolutionScale.y), 0, RenderTarget::MaxDimension);
        tileCopy.ulScaleS = true;
        tileCopy.ulScaleS = true;
        tileCopy.texelShift = { 0, 0 };
        tileCopy.texelMask = { UINT_MAX, UINT_MAX };
        tileCopy.ditherOffset = { tileCopy.left, tileCopy.top };
        tileCopy.ditherPattern = op.createTileCopy.fbTile.ditherPattern;
        tileCopy.readColorFromStorage = false;
        tileCopy.readDepthFromStorage = false;
        tileCopy.ignore = false;

        const bool insufficientSize = (tileCopy.textureWidth < tileWidth) || (tileCopy.textureHeight < tileHeight);
        if (insufficientSize) {
            tileCopy.framebuffer.reset();
            tileCopy.texture.reset();
        }

        if (tileCopy.texture == nullptr) {
            tileCopy.textureWidth = tileWidth;
            tileCopy.textureHeight = tileHeight;
            fixSizeToMultiple(tileCopy.textureWidth, tileCopy.textureHeight);

            RenderTextureFlags textureFlags = RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS;
            textureFlags |= RenderTextureFlag::RENDER_TARGET;

            const RenderTextureDesc textureDesc = RenderTextureDesc::Texture2D(tileCopy.textureWidth, tileCopy.textureHeight, 1, RenderTarget::colorBufferFormat(targetManager.usesHDR), textureFlags);
            tileCopy.texture = renderWorker->device->createTexture(textureDesc);
        }

        if (tileCopy.framebuffer == nullptr) {
            const RenderTexture *framebufferTexture = tileCopy.texture.get();
            tileCopy.framebuffer = renderWorker->device->createFramebuffer(RenderFramebufferDesc(&framebufferTexture, 1));
        }
        
        RenderTargetKey colorTargetKey(fbIt->second.addressStart, fbIt->second.width, fbIt->second.siz, Framebuffer::Type::Color);
        RenderTarget &colorTarget = targetManager.get(colorTargetKey);
        uint32_t rtWidth, rtHeight, rtMisalignX;
        RenderTarget::computeScaledSize(fbIt->second.width, fbIt->second.height, resolutionScale, rtWidth, rtHeight, rtMisalignX);
        if (colorTarget.resize(renderWorker, rtWidth, rtHeight)) {
            assert(resizedTargets != nullptr);
            colorTarget.resolutionScale = resolutionScale;
            resizedTargets->emplace(&colorTarget);
            tileCopy.readColorFromStorage = true;
        }

        if (fbIt->second.everUsedAsDepth) {
            RenderTargetKey depthTargetKey(fbIt->second.addressStart, fbIt->second.width, fbIt->second.siz, Framebuffer::Type::Depth);
            RenderTarget &depthTarget = targetManager.get(depthTargetKey);
            if (depthTarget.resize(renderWorker, rtWidth, rtHeight)) {
                assert(resizedTargets != nullptr);
                resizedTargets->emplace(&depthTarget);
                depthTarget.resolutionScale = resolutionScale;
                tileCopy.readDepthFromStorage = true;
            }
        }
    }

    void FramebufferManager::createTileCopyRecord(RenderWorker *renderWorker, const FramebufferOperation &op, const FramebufferStorage &fbStorage, 
        RenderTargetManager &targetManager, const hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex, CommandListCopies &cmdListCopies,
        const ShaderLibrary *shaderLibrary)
    {
        // Texture for tile copy must exist.
        TileCopy &tileCopy = tileCopies[op.createTileCopy.id];
        if (tileCopy.ignore) {
            return;
        }

        auto fbIt = framebuffers.find(op.createTileCopy.address);
        assert(fbIt != framebuffers.end());
        RenderTargetKey colorTargetKey(fbIt->second.addressStart, fbIt->second.width, fbIt->second.siz, Framebuffer::Type::Color);
        RenderTarget &colorTarget = targetManager.get(colorTargetKey);
        if (tileCopy.readColorFromStorage) {
            colorTarget.clearColorTarget(renderWorker);
            FramebufferChange *fbChange = fbIt->second.readChangeFromStorage(renderWorker, fbStorage, scratchChangePool, Framebuffer::Type::Color, fbIt->second.lastWriteFmt, maxFbPairIndex, 0, fbIt->second.height, shaderLibrary);
            if (fbChange != nullptr) {
                colorTarget.copyFromChanges(renderWorker, *fbChange, fbIt->second.width, fbIt->second.height, 0, shaderLibrary);
            }
        }
        
        // Copy from depth target if the last write was a depth buffer.
        if (fbIt->second.lastWriteType == Framebuffer::Type::Depth) {
            RenderTargetKey depthTargetKey(fbIt->second.addressStart, fbIt->second.width, fbIt->second.siz, Framebuffer::Type::Depth);
            RenderTarget &depthTarget = targetManager.get(depthTargetKey);
            if (tileCopy.readDepthFromStorage) {
                depthTarget.clearDepthTarget(renderWorker);
                FramebufferChange *fbChange = fbIt->second.readChangeFromStorage(renderWorker, fbStorage, scratchChangePool, Framebuffer::Type::Depth, fbIt->second.lastWriteFmt, maxFbPairIndex, 0, fbIt->second.height, shaderLibrary);
                if (fbChange != nullptr) {
                    depthTarget.copyFromChanges(renderWorker, *fbChange, fbIt->second.width, fbIt->second.height, 0, shaderLibrary);
                }
            }

            const FixedRect &r = fbIt->second.lastWriteRect;
            colorTarget.copyFromTarget(renderWorker, &depthTarget, r.left(false), r.top(false), r.width(false, true), r.height(false, true), shaderLibrary);
            fbIt->second.discardLastWrite();
        }

        // Resolve the target if necessary.
        colorTarget.resolveTarget(renderWorker, shaderLibrary);

        cmdListCopies.copyRegionTargets.insert(&colorTarget);

        const uint32_t srcRight = std::min(tileCopy.left + tileCopy.usedWidth, static_cast<uint32_t>(colorTarget.width));
        const uint32_t srcBottom = std::min(tileCopy.top + tileCopy.usedHeight, static_cast<uint32_t>(colorTarget.height));
        CommandListCopyRegion copyRegion = {};
        copyRegion.srcTexture = colorTarget.getResolvedTexture();
        copyRegion.dstTexture = tileCopy.texture.get();

        if (colorTarget.textureCopyDescSet == nullptr) {
            colorTarget.textureCopyDescSet = std::make_unique<TextureCopyDescriptorSet>(renderWorker->device);
            colorTarget.textureCopyDescSet->setTexture(colorTarget.textureCopyDescSet->gInput, colorTarget.getResolvedTexture(), RenderTextureLayout::SHADER_READ, colorTarget.getResolvedTextureView());
        }

        assert(tileCopy.framebuffer != nullptr);
        copyRegion.dstFramebuffer = tileCopy.framebuffer.get();
        copyRegion.descriptorSet = colorTarget.textureCopyDescSet->get();
        copyRegion.pushConstants.uvScroll = { float(tileCopy.left), float(tileCopy.top) };
        copyRegion.pushConstants.uvScale = { float(srcRight - tileCopy.left), float(srcBottom - tileCopy.top) };
        cmdListCopies.cmdListCopyRegions.push_back(copyRegion);
    }
    
    void FramebufferManager::reinterpretTileSetup(RenderWorker *renderWorker, const FramebufferOperation &op, hlslpp::float2 resolutionScale, bool usesHDR) {
        assert(tileCopies.find(op.reinterpretTile.srcId) != tileCopies.end());

        // Source tile must exist.
        TileCopy &srcTile = tileCopies[op.reinterpretTile.srcId];
        if (srcTile.texture == nullptr) {
            srcTile.ignore = true;
            return;
        }

        uint32_t dstTileWidth = srcTile.usedWidth;
        uint32_t dstTileHeight = srcTile.usedHeight;

        // Shrink the tile's width.
        float sampleScale = 1.0f;
        if (op.reinterpretTile.dstSiz > op.reinterpretTile.srcSiz) {
            uint8_t sizDifference = op.reinterpretTile.dstSiz - op.reinterpretTile.srcSiz;
            dstTileWidth >>= sizDifference;
            sampleScale = 1.0f * (1 << sizDifference);
        }
        // Expand the tile's width.
        else if (op.reinterpretTile.dstSiz < op.reinterpretTile.srcSiz) {
            uint8_t sizDifference = op.reinterpretTile.srcSiz - op.reinterpretTile.dstSiz;
            dstTileWidth <<= sizDifference;
            sampleScale = 1.0f / (1 << sizDifference);
        }

        dstTileWidth = std::clamp<long>(dstTileWidth, 1L, RenderTarget::MaxDimension);
        dstTileHeight = std::clamp<long>(dstTileHeight, 1L, RenderTarget::MaxDimension);

        TileCopy &dstTile = tileCopies[op.reinterpretTile.dstId];
        dstTile.id = op.reinterpretTile.dstId;
        dstTile.ulScaleS = op.reinterpretTile.ulScaleS;
        dstTile.ulScaleS = op.reinterpretTile.ulScaleS;
        dstTile.texelShift = op.reinterpretTile.texelShift;
        dstTile.texelMask = op.reinterpretTile.texelMask;
        dstTile.usedWidth = dstTileWidth;
        dstTile.usedHeight = dstTileHeight;
        dstTile.sampleScale = sampleScale;
        dstTile.ditherOffset = srcTile.ditherOffset;
        dstTile.ditherPattern = srcTile.ditherPattern;
        srcTile.ignore = false;

        const bool insufficientSize = (dstTile.textureWidth < dstTileWidth) || (dstTile.textureHeight < dstTileHeight);
        if (insufficientSize) {
            dstTile.framebuffer.reset();
            dstTile.texture.reset();
        }

        if (dstTile.texture == nullptr) {
            dstTile.textureWidth = dstTileWidth;
            dstTile.textureHeight = dstTileHeight;
            fixSizeToMultiple(dstTile.textureWidth, dstTile.textureHeight);

            RenderTextureFlags textureFlags = RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS;
            textureFlags |= RenderTextureFlag::RENDER_TARGET;

            const RenderTextureDesc textureDesc = RenderTextureDesc::Texture2D(dstTile.textureWidth, dstTile.textureHeight, 1, RenderTarget::colorBufferFormat(usesHDR), textureFlags);
            dstTile.texture = renderWorker->device->createTexture(textureDesc);
        }
    }

    void FramebufferManager::reinterpretTileRecord(RenderWorker *renderWorker, const FramebufferOperation &op, TextureCache &textureCache, hlslpp::float2 resolutionScale,
        uint64_t submissionFrame, bool usesHDR, CommandListReinterpretations &cmdListReinterpretations)
    {
        assert(tileCopies.find(op.reinterpretTile.srcId) != tileCopies.end());

        TileCopy &srcTile = tileCopies[op.reinterpretTile.srcId];
        if (srcTile.ignore) {
            return;
        }

        TileCopy &dstTile = tileCopies[op.reinterpretTile.dstId];
        const uint32_t index = uint32_t(cmdListReinterpretations.cmdListDispatches.size());
        CommandListReinterpretDispatch dispatch;
        auto &c = dispatch.reinterpretCB;
        c.resolution.x = dstTile.usedWidth;
        c.resolution.y = dstTile.usedHeight;
        c.sampleScale = dstTile.sampleScale;
        c.srcSiz = op.reinterpretTile.srcSiz;
        c.srcFmt = op.reinterpretTile.srcFmt;
        c.dstSiz = op.reinterpretTile.dstSiz;
        c.dstFmt = op.reinterpretTile.dstFmt;
        c.tlutFormat = (op.reinterpretTile.tlutHash != 0) ? (op.reinterpretTile.tlutFormat + 1) : 0;
        c.ditherOffset = dstTile.ditherOffset;
        c.ditherPattern = dstTile.ditherPattern;
        c.ditherRandomSeed = uint32_t(writeTimestamp) + op.reinterpretTile.dstId;
        c.usesHDR = usesHDR;
        dispatch.srcTexture = srcTile.texture.get();
        dispatch.dstTexture = dstTile.texture.get();

        // Assert for known reinterpretation cases only that are currently supported by the shader.
        assert("Unimplemented reinterpretation logic." && (
            ((c.srcFmt == G_IM_FMT_RGBA) && (c.srcSiz == G_IM_SIZ_16b) && (c.dstSiz == G_IM_SIZ_8b) && (c.tlutFormat > 0)) ||
            ((c.srcSiz == G_IM_SIZ_8b) && ((c.dstFmt == G_IM_FMT_CI) || (c.dstFmt == G_IM_FMT_I) || (c.dstFmt == G_IM_FMT_IA)) && (c.dstSiz == G_IM_SIZ_8b) && (c.tlutFormat == 0)) ||
            ((c.srcFmt == G_IM_FMT_RGBA) && (c.srcSiz == G_IM_SIZ_16b) && (c.dstFmt == G_IM_FMT_IA) && (c.dstSiz == G_IM_SIZ_16b))
        ));

        while (descriptorReinterpretSetsCount >= descriptorReinterpretSets.size()) {
            descriptorReinterpretSets.emplace_back();
        }

        std::unique_ptr<ReinterpretDescriptorSet> &reinterpretSet = descriptorReinterpretSets[descriptorReinterpretSetsCount];
        descriptorReinterpretSetsCount++;

        if (reinterpretSet == nullptr) {
            reinterpretSet = std::make_unique<ReinterpretDescriptorSet>(renderWorker->device);
        }

        reinterpretSet->setTexture(reinterpretSet->gInputColor, dispatch.srcTexture, RenderTextureLayout::SHADER_READ);
        reinterpretSet->setTexture(reinterpretSet->gOutput, dispatch.dstTexture, RenderTextureLayout::GENERAL);

        // Search the texture cache for the TLUT if it's required.
        if (op.reinterpretTile.tlutHash > 0) {
            uint32_t textureIndex;
            if (textureCache.useTexture(op.reinterpretTile.tlutHash, submissionFrame, textureIndex)) {
                const Texture *cacheTexture = textureCache.getTexture(textureIndex);
                assert(cacheTexture != nullptr);
                reinterpretSet->setTexture(reinterpretSet->gInputTLUT, cacheTexture->tmem.get(), RenderTextureLayout::SHADER_READ);
            }
            else {
                assert(false && "Unable to find TLUT required for reintepretation on the texture cache.");
            }
        }
        else {
            if (dummyTLUTTexture == nullptr) {
                dummyTLUTTexture = renderWorker->device->createTexture(RenderTextureDesc::Texture1D(4, 1, RenderFormat::R8_UINT));
                renderWorker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(dummyTLUTTexture.get(), RenderTextureLayout::SHADER_READ));
            }

            reinterpretSet->setTexture(reinterpretSet->gInputTLUT, dummyTLUTTexture.get(), RenderTextureLayout::SHADER_READ);
        }

        dispatch.descriptorSet = reinterpretSet->get();
        cmdListReinterpretations.cmdListDispatches.emplace_back(dispatch);
    }

    bool FramebufferManager::makeFramebufferTile(Framebuffer *fb, uint32_t addressStart, uint32_t addressEnd, uint32_t lineWidth, uint32_t tileHeight, FramebufferTile &outTile, bool RGBA32) {
        assert(fb != nullptr);

        // We need to figure out the best fitting tile from the address range specified and the TMEM Regions this tile must be stored on.
        // The tile width and height parameters won't be 0 on load tile operations. They will however be 0 on load block operations.
        
        // If the starting address is lower than the framebuffer address, we move a row one by one according to the stride specified of the original image width.
        uint32_t tileRowStart = 0;
        uint32_t fbStride = fb->imageRowBytes(fb->width);
        while (addressStart < fb->addressStart) {
            addressStart += fbStride;
            tileRowStart++;
        }
        
        // We went over the allowed address range, a tile copy is impossible.
        if (addressStart >= fb->addressEnd) {
            return false;
        }

        // Disallow the tile copy if the end address ended up below the starting address.
        const uint32_t minEndAddress = std::min(addressEnd, fb->addressEnd);
        if (minEndAddress <= addressStart) {
            return false;
        }
        
        // Figure out how many rows we could possibly given the current address range.
        const uint32_t fbBytes = minEndAddress - fb->addressStart;
        const uint32_t fbMinRow = (addressStart - fb->addressStart) / fbStride;
        const uint32_t fbMaxRow = (fbBytes / fbStride) + (((fbBytes % fbStride) > 0) ? 1 : 0);

        // Relative offset of the image start to the framebuffer start.
        const uint32_t offset = addressStart - fb->addressStart;

        // This will be the same size for 4 and 8 byte formats.
        const uint32_t pixelSize = 1 << fb->siz >> 1;

        // The offset is not aligned to the pixel size. It's not possible to make a direct copy.
        if ((offset % pixelSize) != 0) {
            return false;
        }

        // Figure out where the upper left coordinate of the tile is inside the framebuffer.
        const uint32_t rowBytes = fb->imageRowBytes(fb->width);
        const uint32_t row = offset / rowBytes;
        const uint32_t rowOffset = offset % rowBytes;
        const uint32_t pixelShift = (fb->siz == G_IM_SIZ_4b) ? 1 : 0;
        outTile.left = (rowOffset / pixelSize) << pixelShift;
        outTile.top = row;

        // Line width is defined.
        if (lineWidth > 0) {
            outTile.right = outTile.left + lineWidth;
        }
        // Figure it out from the framebuffer instead.
        else {
            const uint32_t rowRightPixels = ((rowBytes - rowOffset) / pixelSize) << pixelShift;
            outTile.right = outTile.left + rowRightPixels;
        }

        // Tile height is defined.
        if (tileHeight > 0) {
            outTile.bottom = outTile.top + tileHeight;
        }
        else {
            const uint32_t rowEnd = std::max((addressEnd - addressStart) / rowBytes, 1U);
            outTile.bottom = outTile.top + rowEnd;

            // Invalidate the tile if this is a loadBlock operation, more than one row is being loaded
            // and the offset is not perfectly aligned with a row.
            const bool fromLoadBlock = (tileHeight == 0);
            const bool multipleRows = (rowEnd > 1);
            const bool misalignedRow = (rowOffset > 0);
            if (fromLoadBlock && multipleRows && misalignedRow) {
                return false;
            }
        }

        // Clamp the tile to the framebuffer's dimensions and the image row ranges found.
        outTile.top = std::max(outTile.top, fbMinRow);
        outTile.right = std::min(outTile.right, fb->width);
        outTile.bottom = std::min(outTile.bottom, fb->height);
        outTile.bottom = std::min(outTile.bottom, fbMaxRow);

        // Invalid tile.
        if ((outTile.bottom <= outTile.top) || (outTile.right <= outTile.left)) {
            return false;
        }

        // Define the tile.
        outTile.lineWidth = (lineWidth > 0) ? lineWidth : (outTile.right - outTile.left);
        outTile.address = fb->addressStart;
        outTile.siz = fb->siz;
        outTile.fmt = fb->lastWriteFmt;
        outTile.ditherPattern = fb->bestDitherPattern();

        return true;
    }

    FramebufferOperation FramebufferManager::makeTileCopyTMEM(uint64_t dstTileId, const FramebufferTile &fbTile) {
        FramebufferOperation op;
        op.type = FramebufferOperation::Type::CreateTileCopy;
        op.createTileCopy.id = dstTileId;
        op.createTileCopy.address = fbTile.address;
        op.createTileCopy.fbTile = fbTile;
        return op;
    }

    FramebufferOperation FramebufferManager::makeTileReintepretation(uint64_t srcTileId, uint8_t srcSiz, uint8_t srcFmt, uint64_t dstTileId, uint8_t dstSiz, uint8_t dstFmt,
        bool ulScaleS, bool ulScaleT, interop::uint2 texelShift, interop::uint2 texelMask, uint64_t tlutHash, uint32_t tlutFormat)
    {
        FramebufferOperation op;
        op.type = FramebufferOperation::Type::ReinterpretTile;
        op.reinterpretTile.srcId = srcTileId;
        op.reinterpretTile.srcSiz = srcSiz;
        op.reinterpretTile.srcFmt = srcFmt;
        op.reinterpretTile.dstId = dstTileId;
        op.reinterpretTile.dstSiz = dstSiz;
        op.reinterpretTile.dstFmt = dstFmt;
        op.reinterpretTile.ulScaleS = ulScaleS;
        op.reinterpretTile.ulScaleT = ulScaleT;
        op.reinterpretTile.texelShift = texelShift;
        op.reinterpretTile.texelMask = texelMask;
        op.reinterpretTile.tlutHash = tlutHash;
        op.reinterpretTile.tlutFormat = tlutFormat;
        return op;
    }

    void FramebufferManager::insertRegionsTMEM(uint32_t addressStart, uint32_t tmemStart, uint32_t tmemWords, uint32_t tmemMask, bool RGBA32, bool syncRequired, std::vector<RegionIterator> *resultRegions) {
        if (resultRegions != nullptr) {
            resultRegions->clear();
        }

        auto insertRegions = [&](bool upperTMEM) {
            RegionTMEM newRegion = { };
            newRegion.fbTile.address = addressStart;
            newRegion.syncRequired = syncRequired;

            const uint32_t tmemAdd = upperTMEM ? (RDP_TMEM_WORDS >> 1) : 0;
            const uint32_t byteShift = RGBA32 ? 4 : 3;
            uint32_t tmemEnd = (tmemStart & tmemMask) + tmemWords;
            const uint32_t tmemBarrier = tmemEnd;
            uint32_t tmemCursor = tmemEnd;
            uint32_t wordsLeft = tmemWords;
            while (wordsLeft > 0) {
                if ((tmemCursor > tmemBarrier) && ((tmemCursor - tmemBarrier) > wordsLeft)) {
                    wordsLeft -= (tmemCursor - tmemBarrier);
                    newRegion.tmemStart = tmemBarrier;
                    newRegion.tmemEnd = tmemCursor + tmemAdd;
                    wordsLeft = 0;
                }
                else if (wordsLeft > tmemCursor) {
                    wordsLeft -= tmemCursor;
                    newRegion.tmemStart = tmemAdd;
                    newRegion.tmemEnd = tmemCursor + tmemAdd;
                    tmemCursor = (tmemMask + 1);
                }
                else {
                    tmemCursor -= wordsLeft;
                    newRegion.tmemStart = tmemCursor + tmemAdd;
                    newRegion.tmemEnd = tmemCursor + wordsLeft + tmemAdd;
                    wordsLeft = 0;
                }

                activeRegionsTMEM.push_front(newRegion);

                if (resultRegions != nullptr) {
                    resultRegions->push_back(activeRegionsTMEM.begin());
                }
            }
        };

        insertRegions(false);

        if (RGBA32) {
            insertRegions(true);
        }
    }

    void FramebufferManager::discardRegionsTMEM(uint32_t tmemStart, uint32_t tmemWords, uint32_t tmemMask) {
        tmemStart = tmemStart & tmemMask;

        const uint32_t wordLimit = (tmemMask + 1);
        if ((tmemStart + tmemWords) > wordLimit) {
            const uint32_t leftWords = wordLimit - tmemStart;
            discardRegionsTMEM(tmemStart, leftWords, tmemMask);

            const uint32_t rightWords = tmemWords - leftWords;
            discardRegionsTMEM(0, std::min(tmemStart, rightWords), tmemMask);
        }
        else {
            auto it = activeRegionsTMEM.begin();
            const uint32_t tmemEnd = tmemStart + tmemWords;
            while (it != activeRegionsTMEM.end()) {
                if ((it->tmemStart < tmemEnd) && (it->tmemEnd > tmemStart)) {
                    // Region is fully contained within the discard region. Erase the region.
                    if ((it->tmemStart >= tmemStart) && (it->tmemEnd <= tmemEnd)) {
                        it->tmemEnd = it->tmemStart;
                    }
                    // Only the right side of the region is contained withing the discard region. Shrink the region.
                    else if ((it->tmemStart <= tmemStart) && (it->tmemEnd < tmemEnd)) {
                        it->fbTile = {};
                        it->tmemEnd = tmemStart;
                    }
                    // Only the left side of the region is contained withing the discard region. Move the start of the region.
                    else if ((it->tmemStart > tmemStart) && (it->tmemEnd >= tmemEnd)) {
                        it->fbTile = {};
                        it->tmemStart = tmemEnd;
                    }
                    // The discard region is fully contained inside the region but doesn't cover each side. Must shrink the
                    // region to the left side and insert a new one for the right side.
                    else {
                        it->fbTile = {};

                        // Don't add the new region if it'd end up being empty.
                        if (it->tmemEnd != tmemEnd) {
                            RegionTMEM newRegion = *it;
                            newRegion.tmemStart = tmemEnd;
                            activeRegionsTMEM.push_back(newRegion);
                        }

                        it->tmemEnd = tmemStart;
                    }

                    // Region is empty, erase it.
                    if (it->tmemStart == it->tmemEnd) {
                        it = activeRegionsTMEM.erase(it);
                    }
                    else {
                        it++;
                    }
                }
                else {
                    it++;
                }
            }
        }
    }

    FramebufferManager::CheckCopyResult FramebufferManager::checkTileCopyTMEM(uint32_t tmem, uint32_t lineWidth, uint8_t siz, uint8_t fmt, uint16_t uls) {
        const bool RGBA32 = (siz == G_IM_SIZ_32b) && (fmt == G_IM_FMT_RGBA);
        if (RGBA32) {
            tmem = tmem & RDP_TMEM_MASK128;
        }

        CheckCopyResult result;
        auto it = activeRegionsTMEM.begin();
        while (it != activeRegionsTMEM.end()) {
            if ((tmem >= it->tmemStart) && (tmem < it->tmemEnd)) {
                if (it->syncRequired) {
                    result.syncRequired = true;
                }

                if (it->fbTile.valid()) {
                    bool validCopy = false;
                    bool reinterpret = false;
                    uint32_t tileWidth = (it->fbTile.right - it->fbTile.left);
                    uint32_t tileLineWidth = it->fbTile.lineWidth;

                    // Tile reinterpreation is not required when using RGBA16 and Depth tile copies. Allow
                    // the framebuffer manager to perform the conversion instead that preserves the precision.
                    if ((it->fbTile.siz == G_IM_SIZ_16b) && (siz == G_IM_SIZ_16b) &&
                        (
                            ((it->fbTile.fmt == G_IM_FMT_RGBA) && (fmt == G_IM_FMT_DEPTH)) ||
                            ((it->fbTile.fmt == G_IM_FMT_DEPTH) && (fmt == G_IM_FMT_RGBA))
                            )
                        )
                    {
                        // Do nothing.
                    }
                    // Tile reinterpretation is always enabled if the source is an 8-bit FB, since special 
                    // sampling and decoding are required.
                    else if (it->fbTile.siz == G_IM_SIZ_8b) {
                        reinterpret = true;
                    }
                    // Tile reinterpretation is also required if the formats are different. A strict format
                    // difference here is not necessarily true in the case of certain formats that are actually
                    // compatible and behave the same way. This logic can be improved in that regard to detect
                    // less false positives.
                    else if (it->fbTile.fmt != fmt) {
                        reinterpret = true;
                    }

                    // Detect whether the actual width and pixel size matches, and if it doesn't, if the sizes
                    // are compatible.
                    if ((tileLineWidth == lineWidth) && (it->fbTile.siz == siz)) {
                        validCopy = true;
                    }
                    else if ((tileLineWidth < lineWidth) && (it->fbTile.siz > siz)) {
                        uint8_t sizDifference = (it->fbTile.siz - siz);
                        uint32_t sizMultiplier = (1 << sizDifference);
                        if ((tileLineWidth * sizMultiplier) == lineWidth) {
                            tileWidth *= sizMultiplier;
                            tileLineWidth *= sizMultiplier;
                            validCopy = true;
                            reinterpret = true;
                        }
                    }
                    else if ((tileLineWidth > lineWidth) && (it->fbTile.siz < siz)) {
                        uint8_t sizDifference = (siz - it->fbTile.siz);
                        uint32_t sizMultiplier = (1 << sizDifference);
                        if ((lineWidth * sizMultiplier) == tileLineWidth) {
                            tileWidth /= sizMultiplier;
                            tileLineWidth /= sizMultiplier;
                            validCopy = true;
                            reinterpret = true;
                        }
                    }

                    // Special condition for RGBA32. Verify if an equivalent region exists in the upper half of TMEM.
                    if (validCopy && RGBA32) {
                        const uint32_t TMEMUpper = RDP_TMEM_WORDS >> 1;
                        auto searchIt = activeRegionsTMEM.begin();
                        while (searchIt != activeRegionsTMEM.end()) {
                            if ((searchIt != it) &&
                                (searchIt->tileCopyId == it->tileCopyId) &&
                                (searchIt->tmemStart == (it->tmemStart + TMEMUpper)) &&
                                (searchIt->tmemEnd == (it->tmemEnd + TMEMUpper)))
                            {
                                break;
                            }

                            searchIt++;
                        }

                        if (searchIt == activeRegionsTMEM.end()) {
                            validCopy = false;
                        }
                    }

                    if (validCopy) {
                        result.tileId = it->tileCopyId;
                        result.tileWidth = tileWidth;
                        result.lineWidth = tileLineWidth;
                        result.tileHeight = it->fbTile.bottom - it->fbTile.top;
                        result.fmt = it->fbTile.fmt;
                        result.siz = it->fbTile.siz;
                        result.reinterpret = reinterpret;
                    }

                    return result;
                }
            }

            it++;
        }

        return result;
    }

    void FramebufferManager::clearUsedTileCopies() {
        scratchChangePool.reset();
        usedTimestamp++;
    }

    uint64_t FramebufferManager::findTileCopyId(uint32_t width, uint32_t height) {
        assert(width > 0);
        assert(height > 0);

        uint64_t newId = 0;
        uint32_t textureWidth = width;
        uint32_t textureHeight = height;
        fixSizeToMultiple(textureWidth, textureHeight);

        for (auto &it : tileCopies) {
            newId = std::max(it.first, newId);

            TileCopy &tileCopy = it.second;
            if (tileCopy.usedTimestamp == usedTimestamp) {
                continue;
            }

            // Only reuse exact matches.
            if ((tileCopy.textureWidth == textureWidth) && (tileCopy.textureHeight == textureHeight)) {
                tileCopy.usedWidth = width;
                tileCopy.usedHeight = height;
                tileCopy.usedTimestamp = usedTimestamp;
                return it.first;
            }
        }

        // Make a new tile with a new Id.
        TileCopy &tileCopy = tileCopies[++newId];
        tileCopy.id = newId;
        tileCopy.usedWidth = width;
        tileCopy.usedHeight = height;
        tileCopy.usedTimestamp = usedTimestamp;

        return newId;
    }

    void FramebufferManager::storeRAM(FramebufferStorage &fbStorage, const uint8_t *RDRAM, uint32_t fbPairIndex) {
        assert(RDRAM != nullptr);

        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            const uint8_t *fbRAM = &RDRAM[it->first];
            fbStorage.store(fbPairIndex, it->first, fbRAM, it->second.RAMBytes);
            it++;
        }
    }

    void FramebufferManager::checkRAM(const uint8_t *RDRAM, std::vector<Framebuffer *> &differentFbs, bool updateHashes) {
        assert(RDRAM != nullptr);

        differentFbs.clear();
        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            const uint8_t *fbRAM = &RDRAM[it->first];
            uint64_t currentHash = XXH3_64bits(fbRAM, it->second.RAMBytes);
            if (currentHash != it->second.RAMHash) {
                differentFbs.push_back(&it->second);

                if (updateHashes) {
                    it->second.RAMHash = currentHash;
                }
            }

            it++;
        }
    }

    void FramebufferManager::uploadRAM(RenderWorker *renderWorker, Framebuffer **differentFbs, size_t differentFbsCount, FramebufferChangePool &fbChangePool,
        const uint8_t *RDRAM, bool canDiscard, std::vector<FramebufferOperation> &fbOps, std::vector<uint32_t> &fbDiscards, const ShaderLibrary *shaderLibrary)
    {
        assert(renderWorker != nullptr);
        assert(RDRAM != nullptr);

        fbOps.clear();

        size_t fbIndex = 0;
        for (size_t i = 0; i < differentFbsCount; i++) {
            Framebuffer *fb = differentFbs[i];
            const uint8_t *fbRAM = &RDRAM[fb->addressStart];
            const FramebufferChange::Type fbChangeType = (fb->lastWriteFmt == G_IM_FMT_DEPTH) ? FramebufferChange::Type::Depth : FramebufferChange::Type::Color;
            FramebufferChange &fbChange = fbChangePool.use(renderWorker, fbChangeType, fb->width, fb->height, shaderLibrary->usesHDR);
            const uint32_t DifferenceFractionNum = 1;
            const uint32_t DifferenceFractionDiv = 4;
            const uint32_t differentPixels = fb->copyRAMToNativeAndChanges(renderWorker, fbChange, fbRAM, 0, fb->height, fb->lastWriteFmt, false, shaderLibrary);
            const uint32_t differentBytes = differentPixels << fb->siz >> 1;
            fb->modifiedBytes += differentBytes;

            const uint32_t differentBytesLimit = (fb->RAMBytes * DifferenceFractionNum) / DifferenceFractionDiv;
            const bool discardFb = canDiscard && (fb->modifiedBytes >= differentBytesLimit);
            if (discardFb) {
                fbDiscards.emplace_back(fb->addressStart);
            }
            else {
                FramebufferOperation changesOp;
                changesOp.type = FramebufferOperation::Type::WriteChanges;
                changesOp.writeChanges.address = fb->addressStart;
                changesOp.writeChanges.id = fbChange.id;
                fbOps.emplace_back(changesOp);
            }

            fbIndex++;
        }
    }
    
    void FramebufferManager::resetTracking() {
        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            it->second.maxHeight = 0;
            it->second.ditherPatterns.fill(0);
            it++;
        }
    }
    
    void FramebufferManager::hashTracking(const uint8_t *RDRAM) {
        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            if ((it->second.maxHeight > 0) && (it->second.RAMBytes > 0)) {
                it->second.RAMHash = XXH3_64bits(&RDRAM[it->first], it->second.RAMBytes);
            }

            it++;
        }
    }

    void FramebufferManager::changeRAM(Framebuffer *changedFb, uint32_t addressStart, uint32_t addressEnd) {
        assert(changedFb != nullptr);

        auto it = framebuffers.begin();
        while (it != framebuffers.end()) {
            if ((&it->second != changedFb) && (it->second.overlaps(addressStart, addressEnd))) {
                it->second.rdramChanged = true;
            }

            it++;
        }
    }

    void FramebufferManager::resetOperations() {
        descriptorReinterpretSetsCount = 0;
    }

    void FramebufferManager::setupOperations(RenderWorker *renderWorker, const std::vector<FramebufferOperation> &operations, hlslpp::float2 resolutionScale, RenderTargetManager &targetManager, std::unordered_set<RenderTarget *> *resizedTargets) {
        assert(renderWorker != nullptr);

        for (const FramebufferOperation &op : operations) {
            switch (op.type) {
            case FramebufferOperation::Type::WriteChanges: {
                // No setup required.
                break;
            }
            case FramebufferOperation::Type::CreateTileCopy: {
                createTileCopySetup(renderWorker, op, resolutionScale, targetManager, resizedTargets);
                break;
            }
            case FramebufferOperation::Type::ReinterpretTile: {
                reinterpretTileSetup(renderWorker, op, resolutionScale, targetManager.usesHDR);
                break;
            }
            default:
                assert(false && "Unknown operation type");
                break;
            }
        }
    }

    void FramebufferManager::recordOperations(RenderWorker *renderWorker, const FramebufferChangePool *fbChangePool, const FramebufferStorage *fbStorage, const ShaderLibrary *shaderLibrary, TextureCache *textureCache,
        const std::vector<FramebufferOperation> &operations, RenderTargetManager &targetManager, hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex, uint64_t submissionFrame)
    {
        assert(renderWorker != nullptr);

        thread_local CommandListCopies cmdListCopies;
        thread_local CommandListReinterpretations cmdListReinterpretations;
        cmdListCopies.clear();
        cmdListReinterpretations.clear();

        for (const FramebufferOperation &op : operations) {
            switch (op.type) {
            case FramebufferOperation::Type::WriteChanges: {
                assert(fbChangePool != nullptr);
                writeChanges(renderWorker, *fbChangePool, op, targetManager, shaderLibrary);
                break;
            }
            case FramebufferOperation::Type::CreateTileCopy: {
                assert(fbStorage != nullptr);
                createTileCopyRecord(renderWorker, op, *fbStorage, targetManager, resolutionScale, maxFbPairIndex, cmdListCopies, shaderLibrary);
                break;
            }
            case FramebufferOperation::Type::ReinterpretTile: {
                assert(textureCache != nullptr);
                reinterpretTileRecord(renderWorker, op, *textureCache, resolutionScale, submissionFrame, shaderLibrary->usesHDR, cmdListReinterpretations);
                break;
            }
            default:
                assert(false && "Unknown operation type");
                break;
            }
        }

        thread_local std::vector<RenderTextureBarrier> tileCopyBeforeBarriers;
        thread_local std::vector<RenderTextureBarrier> tileCopyAfterBarriers;
        const bool copyRegions = !cmdListCopies.cmdListCopyRegions.empty();
        if (copyRegions) {
            tileCopyBeforeBarriers.clear();
            tileCopyAfterBarriers.clear();

            for (const CommandListCopyRegion &copy : cmdListCopies.cmdListCopyRegions) {
                tileCopyBeforeBarriers.push_back(RenderTextureBarrier(copy.dstTexture, RenderTextureLayout::COLOR_WRITE));
                tileCopyAfterBarriers.push_back(RenderTextureBarrier(copy.dstTexture, RenderTextureLayout::SHADER_READ));
            }

            for (RenderTarget *renderTarget : cmdListCopies.copyRegionTargets) {
                tileCopyBeforeBarriers.emplace_back(RenderTextureBarrier(renderTarget->getResolvedTexture(), RenderTextureLayout::SHADER_READ));
            }

            renderWorker->commandList->barriers(RenderBarrierStage::GRAPHICS, tileCopyBeforeBarriers);
            
            // Use the rasterization pipeline to copy the texture regions. Using the dedicated transfer commands does not behave consistently
            // enough across hardware to use it directly when it comes to synchronization due to unknown reasons found during testing (probably
            // due to transfer granularity or synchronization exceptions on legacy barriers when it comes to copy operations).
            const ShaderRecord &textureCopy = shaderLibrary->textureCopy;
            renderWorker->commandList->setPipeline(textureCopy.pipeline.get());
            renderWorker->commandList->setGraphicsPipelineLayout(textureCopy.pipelineLayout.get());
            renderWorker->commandList->setVertexBuffers(0, nullptr, 0, nullptr);

            RenderDescriptorSet *lastDescriptorSet = nullptr;
            for (const CommandListCopyRegion &copy : cmdListCopies.cmdListCopyRegions) {
                renderWorker->commandList->setFramebuffer(copy.dstFramebuffer);
                renderWorker->commandList->setViewports(RenderViewport(0.0f, 0.0f, copy.pushConstants.uvScale.x, copy.pushConstants.uvScale.y));
                renderWorker->commandList->setScissors(RenderRect(0, 0, std::lround(copy.pushConstants.uvScale.x), std::lround(copy.pushConstants.uvScale.y)));
                if (copy.descriptorSet != lastDescriptorSet) {
                    renderWorker->commandList->setGraphicsDescriptorSet(copy.descriptorSet, 0);
                    lastDescriptorSet = copy.descriptorSet;
                }

                renderWorker->commandList->setGraphicsPushConstants(0, &copy.pushConstants);
                renderWorker->commandList->drawInstanced(3, 1, 0, 0);
            }

            renderWorker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, tileCopyAfterBarriers);
        }

        const bool reinterpretTiles = !cmdListReinterpretations.cmdListDispatches.empty();
        if (reinterpretTiles) {
            tileCopyBeforeBarriers.clear();
            tileCopyAfterBarriers.clear();

            for (const CommandListReinterpretDispatch &dispatch : cmdListReinterpretations.cmdListDispatches) {
                tileCopyBeforeBarriers.push_back(RenderTextureBarrier(dispatch.dstTexture, RenderTextureLayout::GENERAL));
                tileCopyAfterBarriers.push_back(RenderTextureBarrier(dispatch.dstTexture, RenderTextureLayout::SHADER_READ));
            }

            renderWorker->commandList->barriers(RenderBarrierStage::COMPUTE, tileCopyBeforeBarriers);

            const ShaderRecord &shaderRecord = shaderLibrary->fbReinterpret;
            renderWorker->commandList->setPipeline(shaderRecord.pipeline.get());
            renderWorker->commandList->setComputePipelineLayout(shaderRecord.pipelineLayout.get());
            for (const CommandListReinterpretDispatch &dispatch : cmdListReinterpretations.cmdListDispatches) {
                const interop::uint2 &res = dispatch.reinterpretCB.resolution;
                const uint32_t dispatchX = (res.x + FB_COMMON_WORKGROUP_SIZE - 1) / FB_COMMON_WORKGROUP_SIZE;
                const uint32_t dispatchY = (res.y + FB_COMMON_WORKGROUP_SIZE - 1) / FB_COMMON_WORKGROUP_SIZE;
                renderWorker->commandList->setComputeDescriptorSet(dispatch.descriptorSet, 0);
                renderWorker->commandList->setComputePushConstants(0, &dispatch.reinterpretCB);
                renderWorker->commandList->dispatch(dispatchX, dispatchY, 1);
            }

            renderWorker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, tileCopyAfterBarriers);
        }
    }

    void FramebufferManager::performOperations(RenderWorker *renderWorker, const FramebufferChangePool *fbChangePool, const FramebufferStorage *fbStorage, const ShaderLibrary *shaderLibrary,
        TextureCache *textureCache, const std::vector<FramebufferOperation> &operations, RenderTargetManager &targetManager, hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex,
        uint64_t submissionFrame, std::unordered_set<RenderTarget *> *resizedTargets)
    {
        resetOperations();
        setupOperations(renderWorker, operations, resolutionScale, targetManager, resizedTargets);
        recordOperations(renderWorker, fbChangePool, fbStorage, shaderLibrary, textureCache, operations, targetManager, resolutionScale, maxFbPairIndex, submissionFrame);
    }

    void FramebufferManager::performDiscards(const std::vector<uint32_t> &discards) {
        for (uint32_t address : discards) {
            auto it = framebuffers.find(address);
            if (it != framebuffers.end()) {
                framebuffers.erase(it);
            }
        }
    }

    void FramebufferManager::destroyAllTileCopies() {
        tileCopies.clear();
    }

    uint64_t FramebufferManager::nextWriteTimestamp() {
        return ++writeTimestamp;
    }
};