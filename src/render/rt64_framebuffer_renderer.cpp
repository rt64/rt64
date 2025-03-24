//
// RT64
//

#include "rt64_framebuffer_renderer.h"

#include "../include/rt64_extended_gbi.h"

#include "common/rt64_elapsed_timer.h"
#include "common/rt64_math.h"
#include "hle/rt64_color_converter.h"
#include "gbi/rt64_f3d.h"
#include "shared/rt64_framebuffer_params.h"
#include "shared/rt64_raster_params.h"

#include "rt64_descriptor_sets.h"
#include "rt64_render_worker.h"

// TODO: Move to shared.

namespace interop {
    struct BicubicCB {
        uint2 InputResolution;
        uint2 OutputResolution;
    };

    struct HistogramAverageCB {
        uint pixelCount;
        float minLuminance;
        float luminanceRange;
        float timeDelta;
        float tau;
    };

    struct HistogramSetCB {
        float luminanceValue;
    };

    struct LuminanceHistogramCB {
        uint inputWidth;
        uint inputHeight;
        float minLuminance;
        float oneOverLuminanceRange;
    };

    struct TextureCB {
        uint2 TextureSize;
        float2 TexelSize;
    };
};

namespace RT64 {
    // Helper functions.
    
    RenderRect convertFixedRect(FixedRect rect, hlslpp::float2 resScale, int32_t fbWidth, float aspectRatioScale, float extOriginPercentage, int32_t horizontalMisalignment, uint16_t leftOrigin, uint16_t rightOrigin) {
        if (!rect.isNull()) {
            auto computeOrigin = [=](uint16_t origin) {
                if (origin < G_EX_ORIGIN_NONE) {
                    return std::lround(((fbWidth * origin) / G_EX_ORIGIN_RIGHT) * extOriginPercentage + (fbWidth / 2) * (1.0f - extOriginPercentage));
                }
                else {
                    return fbWidth / 2L;
                }
            };

            auto correctMisalignment = [=](int32_t coord, uint16_t origin) {
                if (origin < G_EX_ORIGIN_NONE) {
                    return int32_t(coord - (coord % std::lround(resScale[1]))) - horizontalMisalignment;
                }
                else {
                    return coord;
                }
            };

            int32_t left = static_cast<int32_t>(std::floor((computeOrigin(leftOrigin) + (rect.left(true) - computeOrigin(leftOrigin)) * aspectRatioScale) * resScale.x));
            int32_t right = static_cast<int32_t>(std::ceil((computeOrigin(rightOrigin) + (rect.right(true) - computeOrigin(rightOrigin)) * aspectRatioScale) * resScale.x));
            int32_t top = lround(rect.top(true) * resScale.y);
            int32_t bottom = lround(rect.bottom(true) * resScale.y);
            left = correctMisalignment(left, leftOrigin);
            right = correctMisalignment(right, rightOrigin);
            return RenderRect(left, top, right, bottom);
        }
        else {
            return RenderRect(0, 0, 0, 0);
        }
    }
    
    RenderViewport convertViewportRect(FixedRect rect, hlslpp::float2 resScale, int32_t fbWidth, float aspectRatioScale, float extOriginPercentage, float horizontalMisalignment, uint16_t leftOrigin, uint16_t rightOrigin) {
        auto computeOrigin = [=](uint16_t origin) {
            if (origin < G_EX_ORIGIN_NONE) {
                return ((fbWidth * origin) / G_EX_ORIGIN_RIGHT) * extOriginPercentage + (fbWidth / 2) * (1.0f - extOriginPercentage);
            }
            else {
                return float(fbWidth) / 2;
            }
        };
        
        auto correctMisalignment = [=](float coord, uint16_t origin) {
            if (origin < G_EX_ORIGIN_NONE) {
                return (coord - std::fmod(coord, resScale[1])) - horizontalMisalignment;
            }
            else {
                return coord;
            }
        };
        
        float left = std::round((computeOrigin(leftOrigin) + (rect.left(true) - computeOrigin(leftOrigin)) * aspectRatioScale) * resScale.x);
        float right = std::round((computeOrigin(rightOrigin) + (rect.right(true) - computeOrigin(rightOrigin)) * aspectRatioScale) * resScale.x);
        float top = std::round(rect.top(true) * resScale.y);
        float bottom = std::round(rect.bottom(true) * resScale.y);
        left = correctMisalignment(left, leftOrigin);
        right = correctMisalignment(right, rightOrigin);
        return RenderViewport(left, top, right - left, bottom - top);
    }
    
    void moveViewportRect(RenderViewport &viewport, hlslpp::float2 resScale, float middleViewport, float extOriginPercentage, float horizontalMisalignment, uint16_t origin) {
        if (origin < G_EX_ORIGIN_NONE) {
            viewport.x += ((middleViewport * origin) / G_EX_ORIGIN_CENTER) * extOriginPercentage + middleViewport * (1.0f - extOriginPercentage);
            viewport.x -= horizontalMisalignment;
        }
        else {
            viewport.x += middleViewport;
        }
    }

    FixedRect fixRect(FixedRect rect, const FixedRect &scissorRect, bool fixLR) {
        // There's a very common error in many games where the fill rectangles are incorrectly configured
        // with one less coordinate than what's required to fill out the screen because other rectangle methods
        // under different modes require adding an extra coordinate. Since this behavior often breaks detection
        // for widescreen hacks, an enhancement option to fix them is available given they're within the tolerance
        // of one pixel from the scissor coordinates.
        if (fixLR) {
            if ((abs(scissorRect.lrx - rect.lrx) <= 4) && (rect.ulx < scissorRect.lrx)) {
                rect.lrx = scissorRect.lrx;
            }

            if ((abs(scissorRect.lry - rect.lry) <= 4) && (rect.uly < scissorRect.lry)) {
                rect.lry = scissorRect.lry;
            }
        }

        return rect;
    }
    
    hlslpp::float3 viewPositionFrom(hlslpp::float4x4 viewI) {
        return viewI[3].xyz;
    }

    hlslpp::float3 viewDirectionFrom(hlslpp::float4x4 viewI) {
        return hlslpp::normalize(viewI[2].xyz);
    }

    RenderColor toRenderColor(hlslpp::float4 v) {
        return { v.x, v.y, v.z, v.w };
    }

    // RasterScene

    RasterScene::RasterScene() { }

    // FramebufferRenderer
    
    FramebufferRenderer::FramebufferRenderer(RenderWorker *worker, bool rtSupport, UserConfiguration::GraphicsAPI graphicsAPI, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);

        this->shaderLibrary = shaderLibrary;

        frameParams.frameCount = 0;
        frameParams.viewUbershaders = false;
        frameParams.ditherNoiseStrength = 1.0f;

        shaderUploader = std::make_unique<BufferUploader>(worker->device);
        descCommonSet = std::make_unique<FramebufferRendererDescriptorCommonSet>(shaderLibrary->samplerLibrary, worker->device->getCapabilities().raytracing, worker->device);

#   if RT_ENABLED
        if (rtSupport) {
            this->rtSupport = rtSupport;
            rtResources = std::make_unique<RaytracingResources>(worker, graphicsAPI);
    }
#   endif
    }

    FramebufferRenderer::~FramebufferRenderer() {
        dummyColorTargetView.reset();
        dummyDepthTargetView.reset();
        dummyColorTarget.reset();
        dummyDepthTarget.reset();
    }
    
    void FramebufferRenderer::resetFramebuffers(RenderWorker *worker, bool ubershadersVisible, float ditherNoiseStrength, const RenderMultisampling &multisampling) {
        instanceDrawCallVector.clear();
        hitGroupVector.clear();
        renderIndicesVector.clear();
        rspSmoothNormalVector.clear();
        frameParams.viewUbershaders = ubershadersVisible;
        frameParams.ditherNoiseStrength = ditherNoiseStrength;
        framebufferCount = 0;

        // Create dummy color target if it hasn't been created yet.
        if (dummyColorTarget == nullptr) {
            RenderTextureDesc dummyColorDesc = RenderTextureDesc::ColorTarget(4, 4, RenderTarget::colorBufferFormat(shaderLibrary->usesHDR), multisampling);
            dummyColorTarget = worker->device->createTexture(dummyColorDesc);
            dummyColorTarget->setName("Framebuffer Renderer Color Dummy");
            dummyColorTargetView = dummyColorTarget->createTextureView(RenderTextureViewDesc::Texture2D(dummyColorDesc.format));
            dummyColorTargetTransitioned = false;
        }

        // Create dummy depth target if it hasn't been created yet.
        if (dummyDepthTarget == nullptr) {
            RenderTextureDesc dummyDepthDesc = RenderTextureDesc::DepthTarget(4, 4, RenderFormat::D32_FLOAT, multisampling);
            dummyDepthTarget = worker->device->createTexture(dummyDepthDesc);
            dummyDepthTarget->setName("Framebuffer Renderer Depth Dummy");
            dummyDepthTargetView = dummyDepthTarget->createTextureView(RenderTextureViewDesc::Texture2D(dummyDepthDesc.format));
            dummyDepthTargetTransitioned = false;
        }
    }

#if RT_ENABLED
    void FramebufferRenderer::resetRaytracing(RaytracingShaderCache *rtShaderCache, const RenderTexture *blueNoiseTexture) {
        assert(rtResources != nullptr);
        assert(rtShaderCache != nullptr);
        assert(blueNoiseTexture != nullptr);

        const int32_t stateIndex = rtShaderCache->getActiveState();
        rtState = &rtShaderCache->states[stateIndex];
        rtPipelineLayout = rtShaderCache->pipelineLayout.get();
        rtResources->resetBottomLevelAS();

        this->blueNoiseTexture = blueNoiseTexture;
    }
#endif

    void FramebufferRenderer::updateTextureCache(TextureCache *textureCache) {
        const std::unique_lock<std::mutex> textureMapLock(textureCache->textureMapMutex);
        textureCacheVersions = textureCache->textureMap.versions;
        textureCacheTextures = textureCache->textureMap.textures;
        textureCacheTextureReplacements = textureCache->textureMap.textureReplacements;
        textureCacheFreeSpaces = textureCache->textureMap.freeSpaces;
        textureCacheSize = static_cast<uint32_t>(textureCacheTextures.size());
        textureCacheGlobalVersion = textureCache->textureMap.globalVersion;
        textureCacheReplacementMapEnabled = textureCache->textureMap.replacementMapEnabled;
        dynamicTextureViewVector.clear();
        dynamicTextureBarrierVector.clear();
    }

    void FramebufferRenderer::createGPUTiles(const DrawCallTile *callTiles, uint32_t callTileCount, interop::GPUTile *dstGPUTiles, const FramebufferManager *fbManager, 
        TextureCache *textureCache, uint64_t submissionFrame)
    {
        for (uint32_t i = 0; i < callTileCount; i++) {
            const DrawCallTile &callTile = callTiles[i];
            if (!callTile.valid) {
                continue;
            }
            
            interop::GPUTile &gpuTile = dstGPUTiles[i];
            if (callTile.tileCopyUsed) {
                const auto &it = fbManager->tileCopies.find(callTile.tmemHashOrID);
                if (it != fbManager->tileCopies.end()) {
                    const FramebufferManager::TileCopy &tileCopy = it->second;
                    gpuTile.tcScale.x = static_cast<float>(tileCopy.usedWidth) / static_cast<float>(callTile.tileCopyWidth);
                    gpuTile.tcScale.y = static_cast<float>(tileCopy.usedHeight) / static_cast<float>(callTile.tileCopyHeight);
                    gpuTile.ulScale.x = tileCopy.ulScaleS ? gpuTile.tcScale.x : 1.0f;
                    gpuTile.ulScale.y = tileCopy.ulScaleT ? gpuTile.tcScale.y : 1.0f;
                    gpuTile.texelShift = tileCopy.texelShift;
                    gpuTile.texelMask = tileCopy.texelMask;
                    gpuTile.textureIndex = getTextureIndex(tileCopy);
                    gpuTile.textureDimensions = interop::float3(float(tileCopy.textureWidth), float(tileCopy.textureHeight), 1.0f);
                    gpuTile.flags.alphaIsCvg = !callTile.reinterpretTile;
                    gpuTile.flags.highRes = true;
                    gpuTile.flags.fromCopy = true;
                    gpuTile.flags.rawTMEM = false;
                    gpuTile.flags.hasMipmaps = false;
                }
            }
            else {
                // Retrieve the texture from the cache or use a blank texture if not found.
                uint32_t textureIndex = 0;
                bool textureReplaced = false;
                bool hasMipmaps = false;
                textureCache->useTexture(callTile.tmemHashOrID, submissionFrame, textureIndex, gpuTile.tcScale, gpuTile.textureDimensions, textureReplaced, hasMipmaps);

                // Describe the GPU tile for a regular texture.
                gpuTile.ulScale.x = gpuTile.tcScale.x;
                gpuTile.ulScale.y = gpuTile.tcScale.y;
                gpuTile.texelShift = { 0, 0 };
                gpuTile.texelMask = { UINT_MAX, UINT_MAX };
                gpuTile.textureIndex = textureIndex;
                gpuTile.flags.alphaIsCvg = false;
                gpuTile.flags.highRes = textureReplaced;
                gpuTile.flags.fromCopy = false;
                gpuTile.flags.rawTMEM = !textureReplaced && callTile.rawTMEM;
                gpuTile.flags.hasMipmaps = hasMipmaps;
            }
        }
    }

    uint32_t FramebufferRenderer::getDestinationIndex() {
        uint32_t dstIndex;
        if (textureCacheFreeSpaces.empty()) {
            dstIndex = textureCacheSize++;
        }
        else {
            dstIndex = textureCacheFreeSpaces.back();
            textureCacheFreeSpaces.pop_back();
        }

        return dstIndex;
    }
    
    uint32_t FramebufferRenderer::getTextureIndex(RenderTarget *renderTarget) {
        assert(renderTarget != nullptr);

        uint32_t dstIndex = getDestinationIndex();
        dynamicTextureViewVector.emplace_back(DynamicTextureView{ renderTarget->getResolvedTexture(), dstIndex, renderTarget->getResolvedTextureView()});
        return dstIndex;
    }
    
    uint32_t FramebufferRenderer::getTextureIndex(const FramebufferManager::TileCopy &tileCopy) {
        assert(tileCopy.texture != nullptr);

        uint32_t dstIndex = getDestinationIndex();
        dynamicTextureViewVector.emplace_back(DynamicTextureView{ tileCopy.texture.get(), dstIndex, nullptr });
        dynamicTextureBarrierVector.emplace_back(RenderTextureBarrier(tileCopy.texture.get(), RenderTextureLayout::SHADER_READ));
        return dstIndex;
    }
    
    void FramebufferRenderer::updateShaderDescriptorSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, const bool raytracingEnabled) {
        assert(worker != nullptr);
        assert(drawBuffers != nullptr);
        
        const bool createSet = (descTextureSet == nullptr) || (descTextureSet->textureCacheSize < (textureCacheSize + 1));
        if (createSet) {
            descTextureSet = std::make_unique<FramebufferRendererDescriptorTextureSet>(worker->device, ((textureCacheSize + 1) * 3) / 2);
        }

        if (createSet || (descriptorTextureReplacementMapEnabled != textureCacheReplacementMapEnabled)) {
            descriptorTextureVersions.clear();
            descriptorTextureGlobalVersion = 0;
            descriptorTextureReplacementMapEnabled = textureCacheReplacementMapEnabled;
        }

#   if RT_ENABLED
        if (raytracingEnabled) {
            descCommonSet->setBuffer(descCommonSet->posBuffer, outputBuffers->worldPosBuffer.buffer.get(), outputBuffers->worldPosBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->normBuffer, outputBuffers->worldNormBuffer.buffer.get(), outputBuffers->worldNormBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->velBuffer, outputBuffers->worldVelBuffer.buffer.get(), outputBuffers->worldVelBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->genTexCoordBuffer, outputBuffers->genTexCoordBuffer.buffer.get(), outputBuffers->genTexCoordBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->shadedColBuffer, outputBuffers->shadedColBuffer.buffer.get(), outputBuffers->shadedColBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->srcFogIndices, drawBuffers->fogIndicesBuffer.get(), drawBuffers->fogIndicesBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->srcLightIndices, drawBuffers->lightIndicesBuffer.get(), drawBuffers->lightIndicesBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->srcLightCounts, drawBuffers->lightCountsBuffer.get(), drawBuffers->lightCountsBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->indexBuffer, drawBuffers->faceIndicesBuffer.get(), drawBuffers->faceIndicesBuffer.allocatedSize);
            descCommonSet->setBuffer(descCommonSet->RSPFogVector, drawBuffers->rspFogBuffer.get(), drawBuffers->rspFogBuffer.allocatedSize, RenderBufferStructuredView(sizeof(interop::RSPFog)));
            descCommonSet->setBuffer(descCommonSet->RSPLightVector, drawBuffers->rspLightsBuffer.get(), drawBuffers->rspLightsBuffer.allocatedSize, RenderBufferStructuredView(sizeof(interop::RSPLight)));
            descCommonSet->setTexture(descCommonSet->gViewDirection, rtResources->viewDirectionTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gShadingPosition, rtResources->shadingPositionTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gShadingNormal, rtResources->shadingNormalTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gShadingSpecular, rtResources->shadingSpecularTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gDiffuse, rtResources->diffuseTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gInstanceId, rtResources->instanceIdTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gDirectLightAccum, rtResources->directLightTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gIndirectLightAccum, rtResources->indirectLightTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gReflection, rtResources->reflectionTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gRefraction, rtResources->refractionTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gTransparent, rtResources->transparentTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gFlow, rtResources->flowTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gReactiveMask, rtResources->reactiveMaskTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gLockMask, rtResources->lockMaskTexture.get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gNormalRoughness, rtResources->normalRoughnessTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gDepth, rtResources->depthTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gPrevNormalRoughness, rtResources->normalRoughnessTexture[rtResources->swapBuffers ? 0 : 1].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gPrevDepth, rtResources->depthTexture[rtResources->swapBuffers ? 0 : 1].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gPrevDirectLightAccum, rtResources->directLightTexture[rtResources->swapBuffers ? 0 : 1].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gPrevIndirectLightAccum, rtResources->indirectLightTexture[rtResources->swapBuffers ? 0 : 1].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gFilteredDirectLight, rtResources->filteredDirectLightTexture[1].get(), RenderTextureLayout::GENERAL);
            descCommonSet->setTexture(descCommonSet->gFilteredIndirectLight, rtResources->filteredIndirectLightTexture[1].get(), RenderTextureLayout::GENERAL);

            const uint32_t hitBufferPixelCount = rtResources->textureWidth * rtResources->textureHeight * RaytracingResources::MaxHitQueries;
            descCommonSet->setBuffer(descCommonSet->gHitVelocityDistance, rtResources->hitVelocityDistanceBuffer.get(), hitBufferPixelCount * 16, rtResources->hitVelocityDistanceBufferView.get());
            descCommonSet->setBuffer(descCommonSet->gHitColor, rtResources->hitColorBuffer.get(), hitBufferPixelCount * 4, rtResources->hitColorBufferView.get());
            descCommonSet->setBuffer(descCommonSet->gHitNormalFog, rtResources->hitNormalFogBuffer.get(), hitBufferPixelCount * 8, rtResources->hitNormalFogBufferView.get());
            descCommonSet->setBuffer(descCommonSet->gHitInstanceId, rtResources->hitInstanceIdBuffer.get(), hitBufferPixelCount * 2, rtResources->hitInstanceIdBufferView.get());
            descCommonSet->setAccelerationStructure(descCommonSet->SceneBVH, rtResources->topLevelAS.get());
            descCommonSet->setBuffer(descCommonSet->SceneLights, rtResources->lightsBuffer.get(), sizeof(interop::PointLight) * std::max(rtResources->rtParams.lightsCount, 1U), RenderBufferStructuredView(sizeof(interop::PointLight)));
            descCommonSet->setBuffer(descCommonSet->interleavedRasters, interleavedRastersBuffer.get(), sizeof(interop::InterleavedRaster) * std::max(interleavedRastersCount, 1U), RenderBufferStructuredView(sizeof(interop::InterleavedRaster)));
            descCommonSet->setTexture(descCommonSet->gBlueNoise, blueNoiseTexture, RenderTextureLayout::SHADER_READ);
            descCommonSet->setBuffer(descCommonSet->instanceExtraParams, drawBuffers->extraParamsBuffer.get(), RenderBufferStructuredView(sizeof(interop::ExtraParams)));
            descCommonSet->setBuffer(descCommonSet->RtParams, rtResources->rtParamsBuffer.get(), sizeof(interop::RaytracingParams));
        }
#   endif

        descCommonSet->setBuffer(descCommonSet->FrParams, frameParamsBuffer.get(), sizeof(interop::FrameParams));
        descCommonSet->setBuffer(descCommonSet->instanceRenderIndices, renderIndicesBuffer.get(), RenderBufferStructuredView(sizeof(interop::RenderIndices)));
        descCommonSet->setBuffer(descCommonSet->instanceRDPParams, drawBuffers->rdpParamsBuffer.get(), RenderBufferStructuredView(sizeof(interop::RDPParams)));
        descCommonSet->setBuffer(descCommonSet->RDPTiles, drawBuffers->rdpTilesBuffer.get(), RenderBufferStructuredView(sizeof(interop::RDPTile)));
        descCommonSet->setBuffer(descCommonSet->GPUTiles, drawBuffers->gpuTilesBuffer.get(), RenderBufferStructuredView(sizeof(interop::GPUTile)));
        descCommonSet->setBuffer(descCommonSet->DynamicRenderParams, drawBuffers->renderParamsBuffer.get(), RenderBufferStructuredView(sizeof(interop::RenderParams)));

        // Make sure the versions vector matches the texture cache size.
        descriptorTextureVersions.resize(textureCacheSize, 0);

        // Update texture vector with static textures from the cache and dynamic resource views.
        if (descriptorTextureGlobalVersion != textureCacheGlobalVersion) {
            const uint32_t textureVersionSize = static_cast<uint32_t>(textureCacheVersions.size());
            for (uint32_t i = 0; i < textureVersionSize; i++) {
                if (textureCacheVersions[i] == descriptorTextureVersions[i]) {
                    continue;
                }

                descriptorTextureVersions[i] = textureCacheVersions[i];
                if (textureCacheTextures[i] == nullptr) {
                    continue;
                }

                if (textureCacheReplacementMapEnabled && (textureCacheTextureReplacements[i] != nullptr)) {
                    descTextureSet->setTexture(i, textureCacheTextureReplacements[i]->texture.get(), RenderTextureLayout::SHADER_READ);
                }
                else if (textureCacheTextures[i]->texture != nullptr) {
                    descTextureSet->setTexture(i, textureCacheTextures[i]->texture.get(), RenderTextureLayout::SHADER_READ);
                }
                else {
                    descTextureSet->setTexture(i, textureCacheTextures[i]->tmem.get(), RenderTextureLayout::SHADER_READ);
                }
            }

            descriptorTextureGlobalVersion = textureCacheGlobalVersion;
        }

        for (const DynamicTextureView &dynamicView : dynamicTextureViewVector) {
            descTextureSet->setTexture(dynamicView.dstIndex, dynamicView.texture, RenderTextureLayout::SHADER_READ, dynamicView.textureView);
            descriptorTextureVersions[dynamicView.dstIndex] = 0;
        }
    }

    void FramebufferRenderer::updateRSPSmoothNormalSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers) {
        assert(worker != nullptr);

        if (smoothDescSet == nullptr) {
            smoothDescSet = std::make_unique<RSPSmoothNormalDescriptorSet>(worker->device);
        }

        smoothDescSet->setBuffer(smoothDescSet->srcWorldPos, outputBuffers->worldPosBuffer.buffer.get(), outputBuffers->worldPosBuffer.allocatedSize, RenderBufferStructuredView(sizeof(float) * 4));
        smoothDescSet->setBuffer(smoothDescSet->srcCol, drawBuffers->normalColorBuffer.get(), drawBuffers->normalColorBuffer.allocatedSize, RenderBufferStructuredView(sizeof(uint8_t) * 4));
        smoothDescSet->setBuffer(smoothDescSet->srcFaceIndices, drawBuffers->faceIndicesBuffer.get(), drawBuffers->faceIndicesBuffer.allocatedSize, RenderBufferStructuredView(sizeof(uint32_t)));
        smoothDescSet->setBuffer(smoothDescSet->dstWorldNorm, outputBuffers->worldNormBuffer.buffer.get(), outputBuffers->worldNormBuffer.allocatedSize, RenderBufferStructuredView(sizeof(float) * 4));
    }

    void FramebufferRenderer::updateRSPVertexTestZSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers) {
        assert(worker != nullptr);

        if (vertexTestZSet == nullptr) {
            vertexTestZSet = std::make_unique<RSPVertexTestZDescriptorSet>(worker->device);
        }
        
        vertexTestZSet->setBuffer(vertexTestZSet->screenPos, outputBuffers->screenPosBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
        vertexTestZSet->setBuffer(vertexTestZSet->srcFaceIndices, drawBuffers->faceIndicesBuffer.get(), drawBuffers->faceIndicesBuffer.allocatedSize, RenderBufferStructuredView(sizeof(uint32_t)));
        vertexTestZSet->setBuffer(vertexTestZSet->dstFaceIndices, outputBuffers->testZIndexBuffer.buffer.get(), outputBuffers->testZIndexBuffer.allocatedSize, RenderBufferStructuredView(sizeof(uint32_t)));
    }

    void FramebufferRenderer::updateShaderViews(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, const bool raytracingEnabled) {
        updateShaderDescriptorSet(worker, drawBuffers, outputBuffers, raytracingEnabled);
        updateRSPVertexTestZSet(worker, drawBuffers, outputBuffers);

#   if RT_ENABLED
        if (raytracingEnabled) {
            updateRSPSmoothNormalSet(worker, drawBuffers, outputBuffers);
            rtResources->updateShaderSets(worker, shaderLibrary);
        }
#   endif
    }

    bool FramebufferRenderer::submitDepthAccess(RenderWorker *worker, RenderFramebufferStorage *fbStorage, bool readOnly, bool &depthState) {
        if (depthState == readOnly) {
            return false;
        }
        
        RenderFramebuffer *renderFramebuffer = readOnly ? fbStorage->colorWriteDepthRead.get() : fbStorage->colorDepthWrite.get();
        const RenderTextureLayout depthReadState = RenderTextureLayout::DEPTH_READ;
        const RenderTextureLayout depthWriteState = RenderTextureLayout::DEPTH_WRITE;
        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(fbStorage->depthTarget->texture.get(), readOnly ? depthReadState : RenderTextureLayout::DEPTH_WRITE));
        worker->commandList->setFramebuffer(renderFramebuffer);
        depthState = readOnly;
        return true;
    }
    
    void FramebufferRenderer::submitRasterScene(RenderWorker *worker, const Framebuffer &framebuffer, RenderFramebufferStorage *fbStorage, const RasterScene &rasterScene, bool &depthState) {
        InstanceDrawCall::Type previousCallType = InstanceDrawCall::Type::Unknown;
        bool previousVertexTestZ = false;
        const RenderPipeline *previousPipeline = nullptr;
        RenderViewport previousViewport;
        RenderRect previousScissor;
        interop::RasterParams rasterParams;
        RenderDescriptorSet *descRealFbSet = framebuffer.descRealFbSet->get();
        RenderDescriptorSet *descDummyFbSet = framebuffer.descDummyFbSet->get();

        auto switchToGraphicsPipeline = [&]() {
            previousCallType = InstanceDrawCall::Type::Unknown;
            previousVertexTestZ = false;
            previousPipeline = nullptr;
            previousViewport = RenderViewport();
            previousScissor = RenderRect();
            worker->commandList->setGraphicsPipelineLayout(rendererPipelineLayout);
            worker->commandList->setGraphicsDescriptorSet(descCommonSet->get(), 0);
            worker->commandList->setGraphicsDescriptorSet(descTextureSet->get(), 1);
            worker->commandList->setGraphicsDescriptorSet(descTextureSet->get(), 2);
            worker->commandList->setGraphicsDescriptorSet(depthState ? descRealFbSet : descDummyFbSet, 3);
        };

        auto switchToDepthRead = [&]() {
            if (submitDepthAccess(worker, fbStorage, true, depthState)) {
                worker->commandList->setGraphicsDescriptorSet(descRealFbSet, 3);
            }
        };

        auto switchToDepthWrite = [&]() {
            if (submitDepthAccess(worker, fbStorage, false, depthState)) {
                worker->commandList->setGraphicsDescriptorSet(descDummyFbSet, 3);
            }
        };

        auto drawCallTriangles = [&](const InstanceDrawCall &drawCall) {
            if (drawCall.type == InstanceDrawCall::Type::IndexedTriangles) {
                worker->commandList->drawIndexedInstanced(drawCall.triangles.faceCount * 3, 1, drawCall.triangles.indexStart, 0, 0);
            }
            else {
                worker->commandList->drawInstanced(drawCall.triangles.faceCount * 3, 1, drawCall.triangles.indexStart, 0);
            }
        };

        if (fbStorage->colorTarget != nullptr) {
            switchToGraphicsPipeline();
        }
        
        for (uint32_t i : rasterScene.instanceIndices) {
            const InstanceDrawCall &drawCall = instanceDrawCallVector[i];
            switch (drawCall.type) {
            case InstanceDrawCall::Type::IndexedTriangles: 
            case InstanceDrawCall::Type::RawTriangles:
            case InstanceDrawCall::Type::RegularRect: {
                assert(fbStorage->colorTarget != nullptr);

                const bool typeDifferent = (drawCall.type != previousCallType);
                const bool testZDifferent = (drawCall.type == InstanceDrawCall::Type::IndexedTriangles) && (drawCall.triangles.vertexTestZ != previousVertexTestZ);
                if (typeDifferent || testZDifferent) {
                    switch (drawCall.type) {
                    case InstanceDrawCall::Type::IndexedTriangles:
                        worker->commandList->setVertexBuffers(0, indexedVertexViews.data(), uint32_t(indexedVertexViews.size()), vertexInputSlots.data());
                        worker->commandList->setIndexBuffer(drawCall.triangles.vertexTestZ ? &testZIndexBufferView : &indexBufferView);
                        previousVertexTestZ = drawCall.triangles.vertexTestZ;
                        break;
                    case InstanceDrawCall::Type::RawTriangles:
                    case InstanceDrawCall::Type::RegularRect:
                        worker->commandList->setVertexBuffers(0, rawVertexViews.data(), uint32_t(rawVertexViews.size()), vertexInputSlots.data());
                        worker->commandList->setIndexBuffer(nullptr);
                        break;
                    default:
                        assert(false && "Unknown draw call type.");
                        break;
                    };

                    previousCallType = drawCall.type;
                }

                const auto &triangles = drawCall.triangles;
                assert(triangles.pipeline != nullptr);

                // Draw calls can sometimes end up with empty viewports or scissors and cause validation errors. We just skip them.
                if (triangles.viewport.isEmpty() || triangles.scissor.isEmpty()) {
                    continue;
                }

                // A new pass must be started if decals are required and something wrote to the depth buffer before this call.
                const interop::OtherMode otherMode = triangles.shaderDesc.otherMode;
                bool depthDecal = (otherMode.zMode() == ZMODE_DEC);
                bool depthWrite = otherMode.zUpd();
                if (depthDecal) {
                    switchToDepthRead();
                }
                else if (!depthDecal && depthWrite) {
                    switchToDepthWrite();
                }
                
                if (previousViewport != triangles.viewport) {
                    rasterParams.halfPixelOffset = { 1.0f / triangles.viewport.width, -1.0f / triangles.viewport.height};
                    worker->commandList->setViewports(triangles.viewport);
                    previousViewport = triangles.viewport;
                }

                if (previousScissor != triangles.scissor) {
                    worker->commandList->setScissors(triangles.scissor);
                    previousScissor = triangles.scissor;
                }

                if (previousPipeline != triangles.pipeline) {
                    worker->commandList->setPipeline(triangles.pipeline);
                    previousPipeline = triangles.pipeline;
                }
                
                rasterParams.renderIndex = i;
                worker->commandList->setGraphicsPushConstants(0, &rasterParams);
                drawCallTriangles(drawCall);

                // Simulate dither noise.
                if (triangles.postBlendDitherNoise) {
                    if (triangles.postBlendDitherNoiseNegative) {
                        worker->commandList->setPipeline(postBlendDitherNoiseSubNegativePipeline);
                    }
                    else {
                        worker->commandList->setPipeline(postBlendDitherNoiseAddPipeline);
                        drawCallTriangles(drawCall);

                        worker->commandList->setPipeline(postBlendDitherNoiseSubPipeline);
                    }

                    drawCallTriangles(drawCall);
                    previousPipeline = nullptr;
                }

                break;
            };
            case InstanceDrawCall::Type::FillRect: {
                const auto &clearRect = drawCall.clearRect;
                RenderTarget *chosenTarget = (fbStorage->colorTarget != nullptr) ? fbStorage->colorTarget : fbStorage->depthTarget;
                bool rectCoversWholeTarget = (clearRect.rect.left == 0) && (clearRect.rect.top == 0) && (uint32_t(clearRect.rect.right) == chosenTarget->width) && (uint32_t(clearRect.rect.bottom) == chosenTarget->height);
                const RenderRect *clearRects = rectCoversWholeTarget ? nullptr : &clearRect.rect;
                uint32_t clearRectCount = rectCoversWholeTarget ? 0 : 1;
                if (fbStorage->colorTarget != nullptr) {
                    worker->commandList->clearColor(0, clearRect.color, clearRects, clearRectCount);
                }
                else {
                    worker->commandList->clearDepth(true, clearRect.depth, clearRects, clearRectCount);
                }

                break;
            };
            case InstanceDrawCall::Type::VertexTestZ: {
                assert(testZIndexBuffer != nullptr);
                assert(fbStorage->colorTarget != nullptr);

                const interop::RSPVertexTestZCB &testZCB = drawCall.vertexTestZ;
                switchToDepthRead();

                const bool useMSAA = (fbStorage->colorTarget->multisampling.sampleCount > 0);
                const auto &rspVertexTestZ = useMSAA ? shaderLibrary->rspVertexTestZMS : shaderLibrary->rspVertexTestZ;
                worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderBufferBarrier(testZIndexBuffer, RenderBufferAccess::WRITE));
                worker->commandList->setPipeline(rspVertexTestZ.pipeline.get());
                worker->commandList->setComputePipelineLayout(rspVertexTestZ.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &testZCB);
                worker->commandList->setComputeDescriptorSet(vertexTestZSet->get(), 0);
                worker->commandList->setComputeDescriptorSet(descRealFbSet, 1);
                worker->commandList->dispatch(1, 1, 1);
                worker->commandList->barriers(RenderBarrierStage::GRAPHICS, RenderBufferBarrier(testZIndexBuffer, RenderBufferAccess::READ));

                switchToGraphicsPipeline();
                break;
            };
            default:
                // Do nothing.
                break;
            }
        }

        // Mark targets for resolve.
        if (fbStorage->colorTarget != nullptr) {
            fbStorage->colorTarget->markForResolve();
        }

        if (fbStorage->depthTarget != nullptr) {
            fbStorage->depthTarget->markForResolve();
        }
    }

    void FramebufferRenderer::updateMultisampling() {
        dummyColorTargetView.reset();
        dummyDepthTargetView.reset();
        dummyColorTarget.reset();
        dummyDepthTarget.reset();
#   if RT_ENABLED
        if (rtResources != nullptr) {
            rtResources->updateMultisampling();
        }
#   endif
    }

#if RT_ENABLED
    void FramebufferRenderer::updateRaytracingScene(RenderWorker *worker, const RaytracingScene &rtScene) {
        auto &rtParams = rtResources->rtParams;
        const bool lumaActive = rtScene.presetScene.luminanceRange > 0.0f;

        // Only use jitter when an upscaler is active.
        Upscaler *upscaler = rtResources->getUpscaler(rtResources->upscalerMode);
        bool jitterActive = rtResources->upscaleActive && (upscaler != nullptr);
        if (jitterActive) {
            const int phaseCount = upscaler->getJitterPhaseCount(rtResources->textureWidth, rtScene.screenWidth);
            rtParams.pixelJitter = HaltonJitter(frameParams.frameCount, phaseCount);
        }
        else {
            rtParams.pixelJitter = { 0.0f, 0.0f };
        }

        rtParams.viewport.x = rtScene.viewport.x;
        rtParams.viewport.y = rtScene.viewport.y;
        rtParams.viewport.z = rtScene.viewport.width;
        rtParams.viewport.w = rtScene.viewport.height;

        const auto &preset = rtScene.presetScene;
        rtParams.ambientBaseColor = hlslpp::float4(preset.ambientBaseColor, 0.0f);
        rtParams.ambientNoGIColor = hlslpp::float4(preset.ambientNoGIColor, 0.0f);
        rtParams.eyeLightDiffuseColor = hlslpp::float4(preset.eyeLightDiffuseColor, 0.0f);
        rtParams.eyeLightSpecularColor = hlslpp::float4(preset.eyeLightSpecularColor, 0.0f);
        rtParams.giDiffuseStrength = preset.giDiffuseStrength;
        rtParams.giBackgroundStrength = preset.giBackgroundStrength;
        rtParams.tonemapExposure = preset.tonemapExposure;
        rtParams.tonemapWhite = preset.tonemapWhite;
        rtParams.tonemapBlack = preset.tonemapBlack;

        const auto &proj = rtScene.curProjMatrix;
        rtParams.fovRadians = fovFromProj(proj);
        rtParams.nearDist = nearPlaneFromProj(proj);
        rtParams.farDist = farPlaneFromProj(proj);

        if (isnan(rtParams.fovRadians)) {
            rtParams.fovRadians = 0.75f;
        }

        if (isnan(rtParams.nearDist)) {
            rtParams.nearDist = 1.0f;
        }

        if (isnan(rtParams.farDist)) {
            rtParams.farDist = 1000.0f;
        }

        rtParams.view = rtScene.curViewMatrix;
        rtParams.projection = rtScene.curProjMatrix;

        rtParams.viewI = hlslpp::inverse(rtParams.view);
        rtParams.projectionI = hlslpp::inverse(rtParams.projection);
        rtParams.viewProj = hlslpp::mul(rtParams.view, rtParams.projection);
        rtParams.prevViewProj = hlslpp::mul(rtScene.prevViewMatrix, rtScene.prevProjMatrix);

        // TODO: There's probably a way to compute this without calculating the FOV/Near/Far values.
        // Pinhole camera vectors to generate non-normalized ray direction.
        // TODO: Make a fake target and focal distance at the midpoint of the near/far planes
        // until the game sends that data in some way in the future.
        const float FocalDistance = (rtParams.nearDist + rtParams.farDist) / 2.0f;
        const hlslpp::float3 Up(0.0f, 1.0f, 0.0f);
        const hlslpp::float3 Pos = viewPositionFrom(rtParams.viewI);
        const hlslpp::float3 Target = Pos + viewDirectionFrom(rtParams.viewI) * FocalDistance;
        hlslpp::float3 cameraW = hlslpp::normalize(Target - Pos) * FocalDistance;
        hlslpp::float3 cameraU = hlslpp::normalize(hlslpp::cross(cameraW, Up));
        hlslpp::float3 cameraV = hlslpp::normalize(hlslpp::cross(cameraU, cameraW));
        const float ulen = FocalDistance * std::tan(rtParams.fovRadians * 0.5f);// * rtParams.aspectRatio;
        const float vlen = FocalDistance * std::tan(rtParams.fovRadians * 0.5f);
        cameraU = cameraU * ulen;
        cameraV = cameraV * vlen;
        rtParams.cameraU = hlslpp::float4(cameraU, 0.0f);
        rtParams.cameraV = hlslpp::float4(cameraV, 0.0f);
        rtParams.cameraW = hlslpp::float4(cameraW, 0.0f);

        // Enable light reprojection if denoising is enabled.
#   ifdef DI_REPROJECTION_SUPPORT
        globalParamsBufferData.diReproject = !rtResources->skipReprojection && denoiserEnabled && (globalParamsBufferData.diSamples > 0) && (rtResources->upscalerMode != UpscaleMode::DLSS) ? 1 : 0;
#   else
        rtParams.diReproject = 0;
#   endif

        rtParams.giReproject = !rtResources->skipReprojection && rtResources->denoiserEnabled && (rtParams.giSamples > 0) && (rtResources->upscalerMode != UpscaleMode::DLSS) ? 1 : 0;
        rtParams.binaryLockMask = (rtResources->upscalerMode != UpscaleMode::FSR);
        rtParams.interleavedRastersCount = interleavedRastersCount;
        
        Framebuffer &framebuffer = framebufferVector[framebufferCount - 1];
        RenderDescriptorSet *descRealDepthSet = framebuffer.descRealFbSet->get();
        RenderDescriptorSet *descriptorSets[] = { descCommonSet->get(), descTextureSet->get(), descTextureSet->get(), descRealDepthSet };
        rtResources->updateTopLevelASResources(worker, instanceDrawCallVector, rtScene.instanceIndices);
        rtResources->createShaderBindingTable(worker, rtState, descriptorSets, uint32_t(std::size(descriptorSets)), hitGroupVector);
        rtResources->updateLightsBuffer(worker, rtScene);
    }
    
    void FramebufferRenderer::submitRaytracingScene(RenderWorker *worker, RenderTarget *colorTarget, const RaytracingScene &rtScene) {
        // Unbind any render targets.
        worker->commandList->setFramebuffer(nullptr);

        // Resolve the color target if necessary before using it as the RT scene background.
        colorTarget->resolveTarget(worker, shaderLibrary);
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(colorTarget->getResolvedTexture(), RenderTextureLayout::SHADER_READ));

        if (rtResources->transitionOutputBuffers) {
            RenderTextureBarrier afterCreationBarriers[] = {
                RenderTextureBarrier(rtResources->viewDirectionTexture.get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->shadingPositionTexture.get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->shadingNormalTexture.get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->shadingSpecularTexture.get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->instanceIdTexture.get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->directLightTexture[0].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->directLightTexture[1].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->indirectLightTexture[0].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->indirectLightTexture[1].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->normalRoughnessTexture[0].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->normalRoughnessTexture[1].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->filteredDirectLightTexture[0].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->filteredDirectLightTexture[1].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->filteredIndirectLightTexture[0].get(), RenderTextureLayout::GENERAL),
                RenderTextureBarrier(rtResources->filteredIndirectLightTexture[1].get(), RenderTextureLayout::GENERAL)
            };

            worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterCreationBarriers, uint32_t(std::size(afterCreationBarriers)));
            rtResources->transitionOutputBuffers = false;
        }

        // Make sure all these buffers are usable as UAVs.
        RenderTextureBarrier preDispatchBarriers[] = {
            RenderTextureBarrier(rtResources->diffuseTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->reflectionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->refractionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->transparentTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->flowTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->reactiveMaskTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->lockMaskTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->depthTexture[0].get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->depthTexture[1].get(), RenderTextureLayout::GENERAL)
        };

        worker->commandList->barriers(RenderBarrierStage::COMPUTE, preDispatchBarriers, uint32_t(std::size(preDispatchBarriers)));
        
        // Bind pipeline and dispatch primary rays.
        RT64_LOG_PRINTF("Dispatching primary rays");
        Framebuffer &framebuffer = framebufferVector[framebufferCount - 1];
        RenderDescriptorSet *descRealFbSet = framebuffer.descRealFbSet->get();
        rtResources->shaderBindingTableInfo.groups.rayGen.startIndex = 0;
        worker->commandList->setPipeline(rtState->pipeline.get());
        worker->commandList->setRaytracingPipelineLayout(rtPipelineLayout);
        worker->commandList->setRaytracingDescriptorSet(descCommonSet->get(), 0);
        worker->commandList->setRaytracingDescriptorSet(descTextureSet->get(), 1);
        worker->commandList->setRaytracingDescriptorSet(descTextureSet->get(), 2);
        worker->commandList->setRaytracingDescriptorSet(descRealFbSet, 3);
        worker->commandList->traceRays(rtResources->textureWidth, rtResources->textureHeight, 1, rtResources->shaderBindingTableBuffer->at(0), rtResources->shaderBindingTableInfo.groups);

        // Barriers for shading buffers before dispatching secondary rays.
        RenderTextureBarrier shadingBarriers[] = {
            RenderTextureBarrier(rtResources->instanceIdTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->shadingPositionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->viewDirectionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->shadingNormalTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->shadingSpecularTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->reflectionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->refractionTexture.get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->normalRoughnessTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL),
        };

        worker->commandList->barriers(RenderBarrierStage::COMPUTE, shadingBarriers, uint32_t(std::size(shadingBarriers)));

        // Dispatch rays for direct light.
        RT64_LOG_PRINTF("Dispatching direct light rays");
        rtResources->shaderBindingTableInfo.groups.rayGen.startIndex = 1;
        worker->commandList->traceRays(rtResources->textureWidth, rtResources->textureHeight, 1, rtResources->shaderBindingTableBuffer->at(0), rtResources->shaderBindingTableInfo.groups);

        // Dispatch rays for indirect light.
        RT64_LOG_PRINTF("Dispatching indirect light rays");
        rtResources->shaderBindingTableInfo.groups.rayGen.startIndex = 2;
        worker->commandList->traceRays(rtResources->textureWidth, rtResources->textureHeight, 1, rtResources->shaderBindingTableBuffer->at(0), rtResources->shaderBindingTableInfo.groups);

        // Wait until indirect light is done before dispatching reflection or refraction rays.
        // TODO: This is only required to prevent simultaneous usage of the anyhit buffers.
        // This barrier can be removed if this no longer happens, resulting in less serialization of the commands.
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->indirectLightTexture[rtResources->swapBuffers ? 1 : 0].get(), RenderTextureLayout::GENERAL));

        // Dispatch rays for refraction.
        RT64_LOG_PRINTF("Dispatching refraction rays");
        rtResources->shaderBindingTableInfo.groups.rayGen.startIndex = 4;
        worker->commandList->traceRays(rtResources->textureWidth, rtResources->textureHeight, 1, rtResources->shaderBindingTableBuffer->at(0), rtResources->shaderBindingTableInfo.groups);

        // Wait until refraction is done before dispatching reflection rays.
        // TODO: This is only required to prevent simultaneous usage of the anyhit buffers.
        // This barrier can be removed if this no longer happens, resulting in less serialization of the commands.
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->refractionTexture.get(), RenderTextureLayout::GENERAL));

        // Reflection passes.
        int reflections = rtResources->maxReflections;
        while (reflections > 0) {
            // Dispatch rays for reflection.
            RT64_LOG_PRINTF("Dispatching reflection rays");
            rtResources->shaderBindingTableInfo.groups.rayGen.startIndex = 3;
            worker->commandList->traceRays(rtResources->textureWidth, rtResources->textureHeight, 1, rtResources->shaderBindingTableBuffer->at(0), rtResources->shaderBindingTableInfo.groups);
            reflections--;

            // Add a barrier to wait for the input UAVs to be finished if there's more passes left to be done.
            if (reflections > 0) {
                RenderTextureBarrier newInputBarriers[] = {
                    RenderTextureBarrier(rtResources->viewDirectionTexture.get(), RenderTextureLayout::GENERAL),
                    RenderTextureBarrier(rtResources->shadingNormalTexture.get(), RenderTextureLayout::GENERAL),
                    RenderTextureBarrier(rtResources->instanceIdTexture.get(), RenderTextureLayout::GENERAL),
                    RenderTextureBarrier(rtResources->reflectionTexture.get(), RenderTextureLayout::GENERAL)
                };

                worker->commandList->barriers(RenderBarrierStage::COMPUTE, newInputBarriers, uint32_t(std::size(newInputBarriers)));
            }
        }

        // Copy direct light raw buffer to the first direct filtered buffer.
        {
            RenderTexture *source = rtResources->directLightTexture[rtResources->swapBuffers ? 1 : 0].get();
            RenderTexture *dest = rtResources->filteredDirectLightTexture[1].get();

            RenderTextureBarrier beforeCopyBarriers[] = {
                RenderTextureBarrier(source, RenderTextureLayout::COPY_SOURCE),
                RenderTextureBarrier(dest, RenderTextureLayout::COPY_DEST)
            };

            worker->commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers, uint32_t(std::size(beforeCopyBarriers)));
            worker->commandList->copyTexture(dest, source);

            RenderTextureBarrier afterCopyBarriers[] = {
                RenderTextureBarrier(source, RenderTextureLayout::GENERAL),
                RenderTextureBarrier(dest, RenderTextureLayout::SHADER_READ)
            };

            worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterCopyBarriers, uint32_t(std::size(afterCopyBarriers)));
        }

        // Copy indirect light raw buffer to the first indirect filtered buffer.
        bool denoiseGI = rtResources->denoiserEnabled && (rtResources->rtParams.giSamples > 0) && (rtResources->upscalerMode != UpscaleMode::DLSS);
        {
            RenderTexture *source = rtResources->indirectLightTexture[rtResources->swapBuffers ? 1 : 0].get();
            RenderTexture *dest = rtResources->filteredIndirectLightTexture[denoiseGI ? 0 : 1].get();

            RenderTextureBarrier beforeCopyBarriers[] = {
                RenderTextureBarrier(source, RenderTextureLayout::COPY_SOURCE),
                RenderTextureBarrier(dest, RenderTextureLayout::COPY_DEST)
            };

            worker->commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers, uint32_t(std::size(beforeCopyBarriers)));
            worker->commandList->copyTexture(dest, source);

            RenderTextureBarrier afterCopyBarriers[] = {
                RenderTextureBarrier(source, RenderTextureLayout::GENERAL),
                RenderTextureBarrier(dest, RenderTextureLayout::SHADER_READ)
            };

            worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterCopyBarriers, uint32_t(std::size(afterCopyBarriers)));
        }

        // Apply a gaussian filter to the indirect light with a compute shader.
        if (denoiseGI) {
            for (int i = 0; i < 5; i++) {
                const uint32_t ThreadGroupWorkCount = 8;
                uint32_t dispatchX = (rtResources->textureWidth + ThreadGroupWorkCount - 1) / ThreadGroupWorkCount;
                uint32_t dispatchY = (rtResources->textureHeight + ThreadGroupWorkCount - 1) / ThreadGroupWorkCount;
                const ShaderRecord &gaussianFilter = shaderLibrary->gaussianFilterRGB3x3;
                interop::TextureCB textureCB;
                textureCB.TextureSize = { rtResources->textureWidth, rtResources->textureHeight };
                textureCB.TexelSize = { 1.0f / rtResources->textureWidth, 1.0f / rtResources->textureHeight };

                worker->commandList->setPipeline(gaussianFilter.pipeline.get());
                worker->commandList->setComputePipelineLayout(gaussianFilter.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &textureCB);
                worker->commandList->setComputeDescriptorSet(rtResources->indirectFilterSets[i % 2]->get(), 0);
                worker->commandList->dispatch(dispatchX, dispatchY, 1);

                RenderTextureBarrier afterBlurBarriers[] = {
                    RenderTextureBarrier(rtResources->filteredIndirectLightTexture[(i % 2) ? 1 : 0].get(), RenderTextureLayout::GENERAL),
                    RenderTextureBarrier(rtResources->filteredIndirectLightTexture[(i % 2) ? 0 : 1].get(), RenderTextureLayout::SHADER_READ)
                };

                worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterBlurBarriers, uint32_t(std::size(afterBlurBarriers)));
            }
        }

        // Compose the output buffer.
        RenderTexture *rtOutputCur = rtResources->outputTexture[rtResources->swapBuffers ? 1 : 0].get();

        // Barriers for shading buffers after rays are finished.
        RenderTextureBarrier afterDispatchBarriers[] = {
            RenderTextureBarrier(rtOutputCur, RenderTextureLayout::COLOR_WRITE),
            RenderTextureBarrier(colorTarget->texture.get(), RenderTextureLayout::COLOR_WRITE),
            RenderTextureBarrier(rtResources->diffuseTexture.get(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->reflectionTexture.get(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->refractionTexture.get(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->transparentTexture.get(), RenderTextureLayout::SHADER_READ)
        };

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, afterDispatchBarriers, uint32_t(std::size(afterDispatchBarriers)));

        // Set the output as the current render target.
        worker->commandList->setFramebuffer(rtResources->outputFramebuffer[rtResources->swapBuffers ? 1 : 0].get());

        // Apply the scissor and viewport to the size of the output texture.
        worker->commandList->setViewports(RenderViewport(0.0f, 0.0f, float(rtResources->textureWidth), float(rtResources->textureHeight)));
        worker->commandList->setScissors(RenderRect(0, 0, rtResources->textureWidth, rtResources->textureHeight));

        // Draw the raytracing output.
        RT64_LOG_PRINTF("Composing the raytracing output");
        const ShaderRecord &composeShader = shaderLibrary->compose;
        worker->commandList->setVertexBuffers(0, nullptr, 0, nullptr);
        worker->commandList->setPipeline(composeShader.pipeline.get());
        worker->commandList->setGraphicsPipelineLayout(composeShader.pipelineLayout.get());
        worker->commandList->setGraphicsDescriptorSet(rtResources->composeSet->get(), 0);
        worker->commandList->drawInstanced(3, 1, 0, 0);

        // Switch resources to the correct states after composing the image
        RenderTextureBarrier afterComposeBarriers[] = {
            RenderTextureBarrier(rtOutputCur, RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->filteredDirectLightTexture[1].get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->filteredIndirectLightTexture[1].get(), RenderTextureLayout::GENERAL),
            RenderTextureBarrier(rtResources->flowTexture.get(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->reactiveMaskTexture.get(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(rtResources->lockMaskTexture.get(), RenderTextureLayout::SHADER_READ)
        };

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, afterComposeBarriers, uint32_t(std::size(afterComposeBarriers)));

        const bool lumaActive = rtScene.presetScene.luminanceRange > 0.0f;
        if (lumaActive) {
            const uint32_t ThreadGroupWorkRegionDim = 8;
            worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->downscaledOutputTexture.get(), RenderTextureLayout::GENERAL));

            RT64_LOG_PRINTF("Do the downscaling shader");
            {
                // Execute the compute shader for downscaling the image.
                const ShaderRecord &bicubicScaling = shaderLibrary->bicubicScaling;
                uint32_t dispatchX = ((rtResources->textureWidth / 8) + ThreadGroupWorkRegionDim - 1) / ThreadGroupWorkRegionDim;
                uint32_t dispatchY = ((rtResources->textureHeight / 8) + ThreadGroupWorkRegionDim - 1) / ThreadGroupWorkRegionDim;
                interop::BicubicCB bicubicCB;
                bicubicCB.InputResolution = { rtResources->textureWidth, rtResources->textureHeight };
                bicubicCB.OutputResolution = { rtResources->textureWidth / 8, rtResources->textureHeight / 8 };
                worker->commandList->setPipeline(bicubicScaling.pipeline.get());
                worker->commandList->setComputePipelineLayout(bicubicScaling.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &bicubicCB);
                worker->commandList->setComputeDescriptorSet(rtResources->downscaleSet->get(), 0);
                worker->commandList->dispatch(dispatchX, dispatchY, 1);
            }


            worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->downscaledOutputTexture.get(), RenderTextureLayout::GENERAL));

            RT64_LOG_PRINTF("Do the luminance histogram shader");
            {
                // Execute the compute shader for the luminance histogram.
                const ShaderRecord &luminanceHistogram = shaderLibrary->luminanceHistogram;
                uint32_t dispatchX = ((rtResources->textureWidth / 8) + ThreadGroupWorkRegionDim - 1) / ThreadGroupWorkRegionDim;
                uint32_t dispatchY = ((rtResources->textureHeight / 8) + ThreadGroupWorkRegionDim - 1) / ThreadGroupWorkRegionDim;
                interop::LuminanceHistogramCB histogramCB;
                histogramCB.inputWidth = rtResources->textureWidth / 8;
                histogramCB.inputHeight = rtResources->textureHeight / 8;
                histogramCB.minLuminance = rtScene.presetScene.minLuminance;
                histogramCB.oneOverLuminanceRange = 1.0f / rtScene.presetScene.luminanceRange;
                worker->commandList->setPipeline(luminanceHistogram.pipeline.get());
                worker->commandList->setComputePipelineLayout(luminanceHistogram.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &histogramCB);
                worker->commandList->setComputeDescriptorSet(rtResources->lumaSet->get(), 0);
                worker->commandList->dispatch(dispatchX, dispatchY, 1);
            }

            worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->downscaledOutputTexture.get(), RenderTextureLayout::SHADER_READ));

            RT64_LOG_PRINTF("Do the luminance average shader");
            {
                // Execute the compute shader for the luminance histogram average.
                const ShaderRecord &histogramAverage = shaderLibrary->histogramAverage;
                interop::HistogramAverageCB averageCB;
                averageCB.pixelCount = (rtResources->textureWidth / 8) * (rtResources->textureHeight / 8);
                averageCB.minLuminance = rtScene.presetScene.minLuminance;
                averageCB.luminanceRange = rtScene.presetScene.luminanceRange;
                averageCB.timeDelta = rtScene.deltaTime;
                averageCB.tau = rtScene.presetScene.lumaUpdateTime;
                worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->lumaAverageTexture.get(), RenderTextureLayout::GENERAL));
                worker->commandList->setPipeline(histogramAverage.pipeline.get());
                worker->commandList->setComputePipelineLayout(histogramAverage.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &averageCB);
                worker->commandList->setComputeDescriptorSet(rtResources->lumaAvgSet->get(), 0);
                worker->commandList->dispatch(ThreadGroupWorkRegionDim, ThreadGroupWorkRegionDim, 1);
                worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->lumaAverageTexture.get(), RenderTextureLayout::SHADER_READ));
            }

            RT64_LOG_PRINTF("Do the histogram clear shader");
            {
                // Execute the compute shader for clearing the luminance histogram.
                const ShaderRecord &histogramClear = shaderLibrary->histogramClear;
                worker->commandList->setPipeline(histogramClear.pipeline.get());
                worker->commandList->setComputePipelineLayout(histogramClear.pipelineLayout.get());
                worker->commandList->setComputeDescriptorSet(rtResources->lumaClearSet->get(), 0);
                worker->commandList->dispatch(ThreadGroupWorkRegionDim, ThreadGroupWorkRegionDim, 1);
            }
        }
        else {
            RT64_LOG_PRINTF("Do the histogram set shader");
            {
                // Execute the compute shader for setting the luminance value.
                const ShaderRecord &histogramSet = shaderLibrary->histogramSet;
                interop::HistogramSetCB setCB;
                setCB.luminanceValue = rtScene.presetScene.minLuminance;
                worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(rtResources->lumaAverageTexture.get(), RenderTextureLayout::GENERAL));
                worker->commandList->setPipeline(histogramSet.pipeline.get());
                worker->commandList->setComputePipelineLayout(histogramSet.pipelineLayout.get());
                worker->commandList->setComputePushConstants(0, &setCB);
                worker->commandList->setComputeDescriptorSet(rtResources->lumaSetSet->get(), 0);
                worker->commandList->dispatch(1, 1, 1);
                worker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, RenderTextureBarrier(rtResources->lumaAverageTexture.get(), RenderTextureLayout::SHADER_READ));
            }
        }

        Upscaler *upscaler = rtResources->getUpscaler(rtResources->upscalerMode);
        const bool upscalerActive = rtResources->upscaleActive && (upscaler != nullptr);
        if (upscalerActive) {
            thread_local std::vector<RenderTextureBarrier> beforeBarriers;
            thread_local std::vector<RenderTextureBarrier> afterBarriers;
            beforeBarriers.clear();
            afterBarriers.clear();

            beforeBarriers.push_back(RenderTextureBarrier(rtResources->upscaledOutputTexture.get(), RenderTextureLayout::GENERAL));
            afterBarriers.push_back(RenderTextureBarrier(rtResources->upscaledOutputTexture.get(), RenderTextureLayout::SHADER_READ));
            RenderTexture *rtDepthCur = rtResources->depthTexture[rtResources->swapBuffers ? 1 : 0].get();
            if (upscaler->requiresNonShaderResourceInputs()) {
                for (RenderTexture *res : { rtOutputCur, rtResources->flowTexture.get(), rtResources->reactiveMaskTexture.get(), rtResources->lockMaskTexture.get(), rtDepthCur }) {
                    beforeBarriers.push_back(RenderTextureBarrier(res, RenderTextureLayout::SHADER_READ));
                    afterBarriers.push_back(RenderTextureBarrier(res, RenderTextureLayout::SHADER_READ));
                }
            }

            worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeBarriers);

            Upscaler::UpscaleParameters params;
            params.inRect = { 0, 0, static_cast<int>(rtResources->textureWidth), static_cast<int>(rtResources->textureHeight) };
            params.inDiffuseAlbedo = rtResources->diffuseTexture.get();
            params.inSpecularAlbedo = rtResources->shadingSpecularTexture.get();
            params.inNormalRoughness = rtResources->normalRoughnessTexture[rtResources->swapBuffers ? 1 : 0].get();
            params.inColor = rtOutputCur;
            params.inFlow = rtResources->flowTexture.get();
            params.inReactiveMask = rtResources->upscalerReactiveMask ? rtResources->reactiveMaskTexture.get() : nullptr;
            params.inLockMask = rtResources->upscalerLockMask ? rtResources->lockMaskTexture.get() : nullptr;
            params.inDepth = rtDepthCur;
            params.outColor = rtResources->upscaledOutputTexture.get();
            params.jitterX = -rtResources->rtParams.pixelJitter.x;
            params.jitterY = -rtResources->rtParams.pixelJitter.y;
            params.deltaTime = rtScene.deltaTime * 1000.0f;
            params.nearPlane = rtResources->rtParams.nearDist;
            params.farPlane = rtResources->rtParams.farDist;
            params.fovY = rtResources->rtParams.fovRadians;
            params.resetAccumulation = false; // TODO: Make this configurable via the API.
            upscaler->upscale(worker, params);

            worker->commandList->barriers(RenderBarrierStage::GRAPHICS, afterBarriers);
        }

        // Set the final render target. Apply the same scissor and viewport that was determined for the raytracing step.
        worker->commandList->setFramebuffer(colorTarget->textureFramebuffer.get());
        worker->commandList->setViewports(rtScene.viewport);
        worker->commandList->setScissors(rtScene.scissor);
        worker->commandList->setVertexBuffers(0, nullptr, 0, nullptr);

        // Draw final output.
        const ShaderRecord &postProcess = shaderLibrary->postProcess;
        worker->commandList->setPipeline(postProcess.pipeline.get());
        worker->commandList->setGraphicsPipelineLayout(postProcess.pipelineLayout.get());
        worker->commandList->setGraphicsDescriptorSet(rtResources->postProcessSet->get(), 0);
        worker->commandList->drawInstanced(3, 1, 0, 0);

        // Draw debug view on top.
        if (rtResources->rtParams.visualizationMode != interop::VisualizationMode::Final) {
            const ShaderRecord &debugShader = shaderLibrary->debug;
            worker->commandList->setPipeline(debugShader.pipeline.get());
            worker->commandList->setGraphicsPipelineLayout(debugShader.pipelineLayout.get());
            worker->commandList->setGraphicsDescriptorSet(descCommonSet->get(), 0);
            worker->commandList->setGraphicsDescriptorSet(descTextureSet->get(), 1);
            worker->commandList->setGraphicsDescriptorSet(descTextureSet->get(), 2);
            worker->commandList->setGraphicsDescriptorSet(descRealFbSet, 3);
            worker->commandList->drawInstanced(3, 1, 0, 0);
        }

        // Mark targets for resolve.
        colorTarget->markForResolve();
    }
#endif

    void FramebufferRenderer::submitRSPSmoothNormalCompute(RenderWorker *worker, const OutputBuffers *outputBuffers) {
        if (rspSmoothNormalVector.empty()) {
            return;
        }
        
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderBufferBarrier(outputBuffers->worldNormBuffer.buffer.get(), RenderBufferAccess::WRITE));

        const int ThreadGroupSize = 64;
        const auto &rspSmoothNormal = shaderLibrary->rspSmoothNormal;
        for (const RSPSmoothNormalGenerationCB &cb : rspSmoothNormalVector) {
            const uint32_t triangleCount = cb.indexCount / 3;
            const uint32_t dispatchCount = (triangleCount + ThreadGroupSize - 1) / ThreadGroupSize;
            worker->commandList->setPipeline(rspSmoothNormal.pipeline.get());
            worker->commandList->setComputePipelineLayout(rspSmoothNormal.pipelineLayout.get());
            worker->commandList->setComputePushConstants(0, &cb);
            worker->commandList->setComputeDescriptorSet(smoothDescSet->get(), 0);
            worker->commandList->dispatch(dispatchCount, 1, 1);
        }
    }

    void FramebufferRenderer::recordSetup(RenderWorker *worker, std::vector<BufferUploader *> bufferUploaders, RSPProcessor *rspProcessor,
        VertexProcessor *vertexProcessor, const OutputBuffers *outputBuffers, bool rtEnabled) {
        if (!dummyColorTargetTransitioned) {
            worker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, RenderTextureBarrier(dummyColorTarget.get(), RenderTextureLayout::SHADER_READ));
            dummyColorTargetTransitioned = true;
        }

        if (!dummyDepthTargetTransitioned) {
            worker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, RenderTextureBarrier(dummyDepthTarget.get(), RenderTextureLayout::DEPTH_READ));
            dummyDepthTargetTransitioned = true;
        }

        for (BufferUploader *uploader : bufferUploaders) {
            uploader->commandListBeforeBarriers(worker);
        }

        shaderUploader->commandListBeforeBarriers(worker);

        for (BufferUploader *uploader : bufferUploaders) {
            uploader->commandListCopyResources(worker);
        }

        shaderUploader->commandListCopyResources(worker);

        for (BufferUploader *uploader : bufferUploaders) {
            uploader->commandListAfterBarriers(worker);
        }

        if (rspProcessor != nullptr) {
            rspProcessor->recordCommandList(worker, shaderLibrary, outputBuffers);
        }

        if (vertexProcessor != nullptr) {
            vertexProcessor->recordCommandList(worker, shaderLibrary, outputBuffers);
        }

#   if RT_ENABLED
        if (rtEnabled) {
            assert(rtResources != nullptr);
            if (!rtResources->bottomLevelASVector.empty()) {
                if (!rspSmoothNormalVector.empty()) {
                    submitRSPSmoothNormalCompute(worker, outputBuffers);
                }

                rtResources->submitBottomLevelASCreation(worker);
                rtResources->submitTopLevelASCreation(worker);
            }
        }
#   endif

        shaderUploader->commandListAfterBarriers(worker);
        pendingUploaders = bufferUploaders;

        if (!dynamicTextureBarrierVector.empty()) {
            worker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, dynamicTextureBarrierVector);
        }
    }

    void FramebufferRenderer::recordFramebuffer(RenderWorker *worker, uint32_t framebufferIndex) {
        // Submit all transition barriers first.
        thread_local std::vector<RenderTextureBarrier> startBarriers;
        startBarriers.clear();

        const Framebuffer &framebuffer = framebufferVector[framebufferIndex];
        for (RenderTarget *target : framebuffer.transitionRenderTargetSet) {
            startBarriers.emplace_back(RenderTextureBarrier(target->getResolvedTexture(), RenderTextureLayout::SHADER_READ));
        }
        
        const RenderTargetDrawCall &targetDrawCall = framebuffer.renderTargetDrawCall;
        RenderTarget *colorTarget = targetDrawCall.fbStorage->colorTarget;
        RenderTarget *depthTarget = targetDrawCall.fbStorage->depthTarget;
        if (colorTarget != nullptr) {
            startBarriers.emplace_back(RenderTextureBarrier(colorTarget->texture.get(), RenderTextureLayout::COLOR_WRITE));
        }

        startBarriers.emplace_back(RenderTextureBarrier(depthTarget->texture.get(), RenderTextureLayout::DEPTH_WRITE));
        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, startBarriers);

        bool depthState = false;
        worker->commandList->setFramebuffer(targetDrawCall.fbStorage->colorDepthWrite.get());
        for (const auto &pair : targetDrawCall.sceneIndices) {
#       if RT_ENABLED
            if (pair.second) {
                const auto &rtScene = targetDrawCall.rtScenes[pair.first];

                // Draw all the interleaved rasterized buffers that will be used in the render target.
                thread_local std::vector<RenderTextureBarrier> interleavedBarriers;
                interleavedBarriers.clear();
                for (uint32_t i = 0; i < interleavedRastersCount; i++) {
                    bool interleavedDepthState = false;
                    const uint32_t sceneIndex = rtScene.interleavedRasters[i].rasterSceneIndex;
                    RenderTarget *colorRenderTarget = rtResources->interleavedColorTargetVector[i].get();
                    RenderTarget *depthRenderTarget = rtResources->interleavedDepthTargetVector[i].get();
                    worker->commandList->barriers(RenderBarrierStage::GRAPHICS, {
                        RenderTextureBarrier(colorRenderTarget->texture.get(), RenderTextureLayout::COLOR_WRITE),
                        RenderTextureBarrier(depthRenderTarget->texture.get(), RenderTextureLayout::DEPTH_WRITE)
                    });

                    RenderFramebufferStorage *fbStorage = rtResources->interleavedFramebufferStorageVector[i].get();
                    worker->commandList->setFramebuffer(fbStorage->colorDepthWrite.get());
                    worker->commandList->clearColor();
                    worker->commandList->clearDepth();

                    submitDepthAccess(worker, fbStorage, false, interleavedDepthState);
                    submitRasterScene(worker, framebuffer, fbStorage, targetDrawCall.rasterScenes[sceneIndex], interleavedDepthState);

                    // Resolve the interleaved scene.
                    // TODO: Depth textures need to be thrown into a separate view vector for multisampled textures.
                    fbStorage->colorTarget->resolveTarget(worker, shaderLibrary);

                    interleavedBarriers.emplace_back(RenderTextureBarrier(colorRenderTarget->texture.get(), RenderTextureLayout::SHADER_READ));
                    interleavedBarriers.emplace_back(RenderTextureBarrier(depthRenderTarget->texture.get(), RenderTextureLayout::DEPTH_READ));
                }

                if (!interleavedBarriers.empty()) {
                    worker->commandList->barriers(RenderBarrierStage::COMPUTE, interleavedBarriers);
                }

                submitDepthAccess(worker, targetDrawCall.fbStorage, true, depthState);
                submitRaytracingScene(worker, targetDrawCall.fbStorage->colorTarget, rtScene);
            }
            else
#       endif
            {
                const RasterScene &rasterScene = targetDrawCall.rasterScenes[pair.first];
                submitDepthAccess(worker, targetDrawCall.fbStorage, false, depthState);
                submitRasterScene(worker, framebuffer, targetDrawCall.fbStorage, rasterScene, depthState);
            }
        }
    }

    void FramebufferRenderer::waitForUploaders() {
        shaderUploader->wait();

        for (BufferUploader *uploader : pendingUploaders) {
            uploader->wait();
        }
    }

#if RT_ENABLED
    void FramebufferRenderer::setRaytracingConfig(const RaytracingConfiguration &rtConfig, bool resolutionChanged) {
        assert(rtResources != nullptr);
        rtResources->setRaytracingConfig(rtConfig, resolutionChanged);
    }
#endif

    void FramebufferRenderer::addFramebuffer(const DrawParams &p) {
        assert(p.fbStorage != nullptr);
        
        // Setup framebuffer pair data and descriptor set.
        const FramebufferPair &fbPair = p.curWorkload->fbPairs[p.fbPairIndex];
        interop::FramebufferParams fbParams;
        fbParams.resolution = { p.targetWidth / p.resolutionScale.x, p.targetHeight / p.resolutionScale.y };
        fbParams.resolutionScale = p.resolutionScale;
        fbParams.horizontalMisalignment = p.horizontalMisalignment;
        framebufferCount++;

        while (framebufferCount > framebufferVector.size()) {
            framebufferVector.emplace_back();
            framebufferVector.back().paramsBuffer = p.worker->device->createBuffer(RenderBufferDesc::UploadBuffer(256, RenderBufferFlag::CONSTANT));
            framebufferVector.back().descRealFbSet = std::make_unique<FramebufferRendererDescriptorFramebufferSet>(p.worker->device);
            framebufferVector.back().descDummyFbSet = std::make_unique<FramebufferRendererDescriptorFramebufferSet>(p.worker->device);
        }

        Framebuffer &framebuffer = framebufferVector[framebufferCount - 1];
        void *paramBufferBytes = framebuffer.paramsBuffer->map();
        memcpy(paramBufferBytes, &fbParams, sizeof(interop::FramebufferParams));
        framebuffer.paramsBuffer->unmap();

        RenderTexture *backgroundColorTexture = (p.fbStorage->colorTarget != nullptr) ? p.fbStorage->colorTarget->getResolvedTexture() : dummyColorTarget.get();
        RenderTextureView *backgroundColorTextureView = (p.fbStorage->colorTarget != nullptr) ? p.fbStorage->colorTarget->getResolvedTextureView() : dummyColorTargetView.get();
        framebuffer.descRealFbSet->setBuffer(framebuffer.descRealFbSet->FbParams, framebuffer.paramsBuffer.get(), sizeof(interop::FramebufferParams));
        framebuffer.descRealFbSet->setTexture(framebuffer.descRealFbSet->gBackgroundColor, backgroundColorTexture, RenderTextureLayout::SHADER_READ, backgroundColorTextureView);
        framebuffer.descRealFbSet->setTexture(framebuffer.descRealFbSet->gBackgroundDepth, p.fbStorage->depthTarget->texture.get(), RenderTextureLayout::DEPTH_READ, p.fbStorage->depthTarget->textureView.get());
        framebuffer.descDummyFbSet->setBuffer(framebuffer.descDummyFbSet->FbParams, framebuffer.paramsBuffer.get(), sizeof(interop::FramebufferParams));
        framebuffer.descDummyFbSet->setTexture(framebuffer.descDummyFbSet->gBackgroundColor, backgroundColorTexture, RenderTextureLayout::SHADER_READ, backgroundColorTextureView);
        framebuffer.descDummyFbSet->setTexture(framebuffer.descDummyFbSet->gBackgroundDepth, dummyDepthTarget.get(), RenderTextureLayout::DEPTH_READ, dummyDepthTargetView.get());

        // Store ubershader and other effect pipelines.
        const RasterShaderUber *rasterShaderUber = p.rasterShaderCache->getGPUShaderUber();
        rendererPipelineLayout = rasterShaderUber->pipelineLayout.get();
        postBlendDitherNoiseAddPipeline = rasterShaderUber->postBlendDitherNoiseAddPipeline.get();
        postBlendDitherNoiseSubPipeline = rasterShaderUber->postBlendDitherNoiseSubPipeline.get();
        postBlendDitherNoiseSubNegativePipeline = rasterShaderUber->postBlendDitherNoiseSubNegativePipeline.get();

        // Make a new render target draw call.
        RenderTargetDrawCall &targetDrawCall = framebuffer.renderTargetDrawCall;
        const DrawData &drawData = p.curWorkload->drawData;
        const DrawBuffers &drawBuffers = p.curWorkload->drawBuffers;
        const OutputBuffers &outputBuffers = p.curWorkload->outputBuffers;
        const RenderBuffer *screenPosRes = outputBuffers.screenPosBuffer.buffer.get();
        const RenderBuffer *tcRes = outputBuffers.genTexCoordBuffer.buffer.get();
        const RenderBuffer *indexRes = drawBuffers.faceIndicesBuffer.get();
        const RenderBuffer *shadedColRes = outputBuffers.shadedColBuffer.buffer.get();
        const RenderBuffer *worldPosRes = outputBuffers.worldPosBuffer.buffer.get();
        const RenderBuffer *worldNormRes = outputBuffers.worldNormBuffer.buffer.get();
        const RenderBuffer *worldVelRes = outputBuffers.worldVelBuffer.buffer.get();
        const RenderBuffer *triPosRes = drawBuffers.triPosBuffer.get();
        const RenderBuffer *triTcRes = drawBuffers.triTcBuffer.get();
        const RenderBuffer *triColRes = drawBuffers.triColorBuffer.get();
        const uint32_t PosStride = sizeof(float) * 4;
        const uint32_t ColStride = sizeof(float) * 4;
        const uint32_t WorldNormStride = sizeof(float) * 4;
        const uint32_t WorldVelStride = sizeof(float) * 4;
        const uint32_t TcStride = sizeof(float) * 2;
        const uint32_t IndexStride = sizeof(uint32_t);
        const uint32_t vertexCount = drawData.vertexCount();
        const uint32_t indexCount = uint32_t(drawData.faceIndices.size());
        const uint32_t rawTriVertexCount = drawData.rawTriVertexCount();
        vertexInputSlots[0] = RenderInputSlot(0, PosStride);
        vertexInputSlots[1] = RenderInputSlot(1, TcStride);
        vertexInputSlots[2] = RenderInputSlot(2, ColStride);
        indexedVertexViews[0] = RenderVertexBufferView(RenderBufferReference(screenPosRes), PosStride * vertexCount);
        indexedVertexViews[1] = RenderVertexBufferView(RenderBufferReference(tcRes), TcStride * vertexCount);
        indexedVertexViews[2] = RenderVertexBufferView(RenderBufferReference(shadedColRes), ColStride * vertexCount);
        indexBufferView = RenderIndexBufferView(RenderBufferReference(indexRes), IndexStride * indexCount, RenderFormat::R32_UINT);
        rawVertexViews[0] = RenderVertexBufferView(RenderBufferReference(triPosRes), PosStride * rawTriVertexCount);
        rawVertexViews[1] = RenderVertexBufferView(RenderBufferReference(triTcRes), TcStride * rawTriVertexCount);
        rawVertexViews[2] = RenderVertexBufferView(RenderBufferReference(triColRes), ColStride * rawTriVertexCount);
        testZIndexBuffer = outputBuffers.testZIndexBuffer.buffer.get();
        testZIndexBufferView = RenderIndexBufferView(testZIndexBuffer, uint32_t(outputBuffers.testZIndexBuffer.allocatedSize), RenderFormat::R32_UINT);

        RasterScene rasterScene;
        auto checkRasterScene = [&](RasterScene &rasterScene) {
            if (!rasterScene.instanceIndices.empty()) {
                uint32_t sceneIndex = static_cast<uint32_t>(targetDrawCall.rasterScenes.size());
                targetDrawCall.rasterScenes.push_back(rasterScene);
                targetDrawCall.sceneIndices.push_back({ sceneIndex, false });
                rasterScene.instanceIndices.clear();

                return true;
            }
            else {
                return false;
            }
        };

        targetDrawCall.rasterScenes.clear();

#   if RT_ENABLED
        RaytracingScene rtScene;
        auto checkRtScene = [&](RaytracingScene &rtScene) {
            if (!rtScene.instanceIndices.empty()) {
                uint32_t sceneIndex = static_cast<uint32_t>(targetDrawCall.rtScenes.size());
                targetDrawCall.rtScenes.push_back(rtScene);
                targetDrawCall.sceneIndices.push_back({ sceneIndex, true });
                rtScene.instanceIndices.clear();

                return true;
            }
            else {
                return false;
            }
        };

        targetDrawCall.rtScenes.clear();
#   endif

        targetDrawCall.fbStorage = p.fbStorage;
        targetDrawCall.sceneIndices.clear();

        const float SimilarityPercentage = 0.1f; // TODO: Make more strict once VI ratios are in.
        const float scissorRatio = static_cast<float>(fbPair.scissorRect.width(false, true)) / static_cast<float>(fbPair.scissorRect.height(false, true));
        const bool adjustRatio = (abs((scissorRatio / p.aspectRatioSource) - 1.0f) < SimilarityPercentage);
        const float aspectRatioScale = adjustRatio ? (p.aspectRatioTarget / p.aspectRatioSource) : 1.0f;
        InstanceDrawCall instanceDrawCall;
        interop::RenderIndices renderIndices;
        FixedRect fixedRect;
        uint32_t globalCallIndex = 0;
        const float wideWidth = p.fbWidth * p.resolutionScale.x;
        const float originalWidth = p.fbWidth * p.resolutionScale.y;
        const float commonHeight = float(p.targetHeight);
        const float middleViewport = (wideWidth / 2.0f) - (originalWidth / 2.0f);
        const float extOriginPercentage = p.extAspectPercentage;
        RenderViewport rawViewportWide(0.0f, 0.0f, wideWidth, commonHeight);
        RenderViewport rawViewportOriginal(0.0f, 0.0f, originalWidth, commonHeight);
        uint32_t vertexTestZFaceIndicesStart = 0;
        int32_t vertexTestZCallIndex = -1;
        for (uint32_t pr = 0; (pr < fbPair.projectionCount) && (globalCallIndex < p.maxGameCall); pr++) {
            const Projection &proj = fbPair.projections[pr];
            const bool perspProj = (proj.type == Projection::Type::Perspective);

#       if RT_ENABLED
            // TODO: Move heuristics of RT proj elsewhere?
            // TODO: Use detected scenes logic instead.
            bool rtProj = p.rtEnabled && perspProj && fbPair.depthWrite && targetDrawCall.rtScenes.empty(); // TODO: Remove the last condition once multiple heaps per RT scene are supported.

            // Make sure the matrices are compatible if we're switching to a new projection.
            bool rtProjCompatible = true;
            if (rtProj && (!rtScene.instanceIndices.empty())) {
                const float Threshold = 1e-6f;
                const float viewMatrixDiff = matrixDifference(drawData.modViewTransforms[proj.transformsIndex], rtScene.curViewMatrix);
                const float projMatrixDiff = matrixDifference(drawData.modProjTransforms[proj.transformsIndex], rtScene.curProjMatrix);
                rtProjCompatible = (viewMatrixDiff < Threshold) && (projMatrixDiff < Threshold);
            }

            // TODO: Remove this condition once multiple heaps per RT scene are supported.
            if (rtProj && !rtProjCompatible) {
                rtProj = false;
            }
#       endif
            
            const uint16_t viewportOrigin = drawData.viewportOrigins[proj.transformsIndex];
            for (uint32_t d = 0; (d < proj.gameCallCount) && (globalCallIndex < p.maxGameCall); d++) {
                const GameCall &call = proj.gameCalls[d];
                renderIndices.instanceIndex = call.callDesc.callIndex;
                renderIndices.faceIndicesStart = call.meshDesc.faceIndicesStart;
                renderIndices.rdpTileIndex = call.callDesc.tileIndex;
                renderIndices.rdpTileCount = call.callDesc.tileCount;
                renderIndices.highlightColor = call.debuggerDesc.highlightColor;
                renderIndicesVector.push_back(renderIndices);

                uint32_t cycleType = call.callDesc.otherMode.cycleType();
                if (cycleType == G_CYC_FILL) {
                    instanceDrawCall.type = InstanceDrawCall::Type::FillRect;

                    auto &clearRect = instanceDrawCall.clearRect;
                    if (call.debuggerDesc.highlightColor > 0) {
                        clearRect.color = toRenderColor(ColorConverter::RGBA32::toRGBAF(call.debuggerDesc.highlightColor));
                    }
                    else {
                        if (p.fbStorage->colorTarget == nullptr) {
                            clearRect.depth = ColorConverter::D16::toF(call.callDesc.fillColor & 0xFFFF);
                        }
                        else if (fbPair.colorImage.siz == G_IM_SIZ_32b) {
                            clearRect.color = toRenderColor(ColorConverter::RGBA32::toRGBAF(call.callDesc.fillColor));
                        }
                        else {
                            clearRect.color = toRenderColor(ColorConverter::RGBA16::toRGBAF(call.callDesc.fillColor & 0xFFFF));
                        }
                    }

                    float invRatioScale = 1.0f / aspectRatioScale;
                    int32_t horizontalMisalignment = 0;
                    fixedRect = fixRect(call.callDesc.rect, fbPair.scissorRect, p.fixRectLR);

                    // A rect that spans the whole width of the scissor.
                    if ((fixedRect.ulx <= fbPair.scissorRect.ulx) && (fixedRect.lrx >= fbPair.scissorRect.lrx)) {
                        invRatioScale = 1.0f;
                    }
                    // A regular rectangle that should correct its misalignment.
                    else {
                        horizontalMisalignment = int32_t(p.horizontalMisalignment);
                    }

                    clearRect.rect = convertFixedRect(fixedRect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, horizontalMisalignment, call.callDesc.rectLeftOrigin, call.callDesc.rectRightOrigin);
                }
                else if (call.callDesc.extendedType != DrawExtendedType::None) {
                    switch (call.callDesc.extendedType) {
                    case DrawExtendedType::VertexTestZ:
                        instanceDrawCall.type = InstanceDrawCall::Type::VertexTestZ;
                        instanceDrawCall.vertexTestZ.vertexIndex = call.callDesc.extendedData.vertexTestZ.vertexIndex;
                        instanceDrawCall.vertexTestZ.resolutionScale = p.resolutionScale;
                        instanceDrawCall.vertexTestZ.srcIndexStart = call.meshDesc.faceIndicesStart + 3;
                        instanceDrawCall.vertexTestZ.dstIndexStart = vertexTestZFaceIndicesStart;
                        instanceDrawCall.vertexTestZ.indexCount = 0;
                        vertexTestZCallIndex = int32_t(instanceDrawCallVector.size());
                        break;
                    case DrawExtendedType::EndVertexTestZ:
                        instanceDrawCall.type = InstanceDrawCall::Type::Unknown;
                        vertexTestZCallIndex = -1;
                        break;
                    default:
                        assert(false && "Unknown extended type.");
                        break;
                    }
                }
                else {
#               if RT_ENABLED
                    if (rtProj) {
                        instanceDrawCall.type = InstanceDrawCall::Type::Raytracing;

                        if (hitGroupVector.empty()) {
                            // TODO: Support specialized shaders.
                            //const RaytracingShaderPrograms &shaderPrograms = p.ubershadersOnly ? rtState->shaderProgramsMap.find(UberShaderHash)->second : rtState->getShaderPrograms(call.shaderDesc);
                            const RaytracingShaderPrograms &shaderPrograms = rtState->shaderProgramsMap.find(UberShaderHash)->second;
                            hitGroupVector.emplace_back(shaderPrograms.surface);
                            hitGroupVector.emplace_back(shaderPrograms.shadow);
                        }

                        auto &raytracing = instanceDrawCall.raytracing;
                        raytracing.hitGroupIndex = 0;
                        raytracing.cullDisable = !call.shaderDesc.flags.culling;

                        const interop::OtherMode &otherMode = call.callDesc.otherMode;
                        raytracing.queryMask = (otherMode.zCmp() || otherMode.zUpd()) ? DepthRayQueryMask : NoDepthRayQueryMask;
                        if (drawData.extraParams[call.callDesc.callIndex].shadowCatcherFactor > 0.0f) {
                            raytracing.queryMask |= ShadowCatcherRayQueryMask;
                        }

                        const RenderBottomLevelASMesh asMesh(indexRes->at(call.meshDesc.faceIndicesStart *IndexStride), worldPosRes->at(0), RenderFormat::R32_UINT, RenderFormat::R32G32B32_FLOAT, call.callDesc.triangleCount * 3, vertexCount, PosStride, false);
                        rtResources->addBottomLevelASMesh(asMesh);

                        if (false) { // TODO: call.shaderDesc.flags.smoothNormal
                            RSPSmoothNormalGenerationCB rspSmoothNormal;
                            rspSmoothNormal.indexStart = call.meshDesc.faceIndicesStart;
                            rspSmoothNormal.indexCount = call.callDesc.triangleCount * 3;
                            rspSmoothNormalVector.push_back(rspSmoothNormal);
                        }
                    }
                    else 
#               endif
                    {
                        auto &triangles = instanceDrawCall.triangles;
                        triangles.shaderDesc = call.shaderDesc;

                        RasterShader *gpuShader = p.ubershadersOnly ? nullptr : p.rasterShaderCache->getGPUShader(call.shaderDesc);
                        if (gpuShader != nullptr) {
                            triangles.pipeline = gpuShader->pipeline.get();
                        }
                        else {
                            const bool copyMode = (call.shaderDesc.otherMode.cycleType() == G_CYC_COPY);
                            triangles.pipeline = rasterShaderUber->getPipeline(
                                !copyMode && call.shaderDesc.otherMode.zCmp() && (call.shaderDesc.otherMode.zMode() != ZMODE_DEC),
                                !copyMode && call.shaderDesc.otherMode.zUpd(),
                                (call.shaderDesc.otherMode.cvgDst() == CVG_DST_WRAP) || (call.shaderDesc.otherMode.cvgDst() == CVG_DST_SAVE));
                        }
                        
                        triangles.faceCount = call.callDesc.triangleCount;
                        triangles.vertexTestZ = (vertexTestZCallIndex >= 0);
                        triangles.postBlendDitherNoise = false;

                        float invRatioScale = 1.0f / aspectRatioScale;
                        float horizontalMisalignment = 0.0f;
                        switch (proj.type) {
                        case Projection::Type::Perspective:
                        case Projection::Type::Orthographic: {
                            instanceDrawCall.type = InstanceDrawCall::Type::IndexedTriangles;
                            triangles.indexStart = triangles.vertexTestZ ? vertexTestZFaceIndicesStart : call.meshDesc.faceIndicesStart;

                            // Custom origin must not be in use to be able to use the stretched viewport.
                            bool useWideViewport = false;
                            if (proj.type == Projection::Type::Perspective) {
                                // The call's scissor spans the whole width of the framebuffer pair scissor. The viewport must not be using an extended origin.
                                useWideViewport = (viewportOrigin == G_EX_ORIGIN_NONE) && (call.callDesc.scissorRect.ulx <= fbPair.scissorRect.ulx) && (call.callDesc.scissorRect.lrx <= fbPair.scissorRect.lrx);
                            }
                            else {
                                // Always use the wide viewport with orthographic projections.
                                useWideViewport = (viewportOrigin == G_EX_ORIGIN_NONE);
                            }

                            if (useWideViewport) {
                                invRatioScale = 1.0f;
                                triangles.viewport = rawViewportWide;
                            }
                            else {
                                triangles.viewport = rawViewportOriginal;
                                moveViewportRect(triangles.viewport, p.resolutionScale, middleViewport, extOriginPercentage, horizontalMisalignment, viewportOrigin);
                            }

                            break;
                        }
                        case Projection::Type::Rectangle: {
                            instanceDrawCall.type = InstanceDrawCall::Type::RegularRect;
                            triangles.indexStart = call.meshDesc.rawVertexStart;
                            fixedRect = fixRect(call.callDesc.rect, fbPair.scissorRect, p.fixRectLR);

                            bool tileCopiesUsed = false;
                            for (uint32_t t = 0; (t < call.callDesc.tileCount) && !tileCopiesUsed; t++) {
                                tileCopiesUsed = drawData.callTiles[call.callDesc.tileIndex + t].tileCopyUsed;
                            }

                            // The call's scissor spans the whole width of the framebuffer pair scissor. The rect must not be using extended origins.
                            const bool regularOrigins = (call.callDesc.rectLeftOrigin == G_EX_ORIGIN_NONE) && (call.callDesc.rectRightOrigin == G_EX_ORIGIN_NONE);
                            const bool coversScissorWidth = regularOrigins && (fixedRect.ulx <= fbPair.scissorRect.ulx) && (fixedRect.lrx >= fbPair.scissorRect.lrx);
                            if (tileCopiesUsed || coversScissorWidth) {
                                invRatioScale = 1.0f;
                            }
                            else {
                                horizontalMisalignment = p.horizontalMisalignment;
                            }

                            triangles.viewport = convertViewportRect(fixedRect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, horizontalMisalignment, call.callDesc.rectLeftOrigin, call.callDesc.rectRightOrigin);

                            if (p.postBlendNoise) {
                                // Indicate if post blend dither noise should be applied.
                                bool rgbDitherNoise = (call.shaderDesc.otherMode.rgbDither() == G_CD_NOISE);
                                triangles.postBlendDitherNoise = rgbDitherNoise && !call.shaderDesc.otherMode.zCmp() && !call.shaderDesc.otherMode.zUpd();
                                triangles.postBlendDitherNoiseNegative = p.postBlendNoiseNegative;
                            }

                            break;
                        }
                        case Projection::Type::Triangle: {
                            instanceDrawCall.type = InstanceDrawCall::Type::RawTriangles;
                            triangles.indexStart = call.meshDesc.rawVertexStart;
                            triangles.viewport = rawViewportWide;
                            break;
                        }
                        case Projection::Type::None:
                        default:
                            break;
                        }

                        triangles.scissor = convertFixedRect(call.callDesc.scissorRect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, int32_t(horizontalMisalignment), call.callDesc.scissorLeftOrigin, call.callDesc.scissorRightOrigin);
                        
                        if (triangles.vertexTestZ) {
                            if ((proj.type == Projection::Type::Perspective) || (proj.type == Projection::Type::Orthographic)) {
                                instanceDrawCallVector[vertexTestZCallIndex].vertexTestZ.indexCount += call.callDesc.triangleCount * 3;
                                vertexTestZFaceIndicesStart += call.callDesc.triangleCount * 3;
                            }
                        }
                    }
                }

                // Determine to use the draw call either in the RT scene or the raster scene.
                const uint32_t instanceIndex = static_cast<uint32_t>(instanceDrawCallVector.size());
#           if RT_ENABLED
                bool rtCall = instanceDrawCall.type == InstanceDrawCall::Type::Raytracing;
                if (rtCall) {
                    // If the current scene is not compatible, we submit it before the raster scene.
                    if (!rtProjCompatible) {
                        checkRtScene(rtScene);
                    }

                    const bool addedRasterScene = checkRasterScene(rasterScene);
                    if (rtScene.instanceIndices.empty()) {
                        float projRatioScale = 1.0f / aspectRatioScale;
                        float invRatioScale = 1.0f / aspectRatioScale;
                        const bool coversScissorWidth = (proj.scissorRect.ulx <= fbPair.scissorRect.ulx) && (proj.scissorRect.lrx >= fbPair.scissorRect.lrx);
                        if (coversScissorWidth) {
                            invRatioScale = 1.0f;
                        }
                        else {
                            projRatioScale = 1.0f;
                        }

                        rtScene.curViewMatrix = drawData.modViewTransforms[proj.transformsIndex];
                        rtScene.curProjMatrix = drawData.modProjTransforms[proj.transformsIndex];
                        rtScene.prevViewMatrix = drawData.prevViewTransforms[proj.transformsIndex];
                        rtScene.prevProjMatrix = drawData.prevProjTransforms[proj.transformsIndex];

                        const auto &viewport = drawData.rspViewports[proj.transformsIndex];
                        rtScene.viewport = convertViewportRect(viewport.rect(), p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, 0.0f, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE);
                        rtScene.scissor = convertFixedRect(proj.scissorRect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, 0, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE);

                        rtScene.presetScene = p.presetScene;
                        rtScene.screenWidth = lround(static_cast<float>(p.fbWidth) * p.resolutionScale.x);
                        rtScene.screenHeight = lround(static_cast<float>(p.fbHeight) * p.resolutionScale.y);

                        if (proj.pointLightCount > 0) {
                            rtScene.pointLights = proj.pointLights.data();
                            rtScene.lightCount = proj.pointLightCount;
                        }
                        else {
                            rtScene.pointLights = nullptr;
                            rtScene.lightCount = 0;
                        }
                    }
                    else if (rtProjCompatible && addedRasterScene) {
                        targetDrawCall.sceneIndices.pop_back();
                        const uint32_t rasterSceneIndex = static_cast<uint32_t>(targetDrawCall.rasterScenes.size() - 1);
                        const auto &rasterScene = targetDrawCall.rasterScenes[rasterSceneIndex];
                        rtScene.interleavedRasters.push_back({ rasterSceneIndex, rasterScene.instanceIndices.back(), 0, 0 });
                    }

                    rtScene.instanceIndices.push_back(instanceIndex);
                }
                else 
#           endif
                {
                    rasterScene.instanceIndices.push_back(instanceIndex);
                }

                instanceDrawCallVector.push_back(instanceDrawCall);
                globalCallIndex++;
            }
        }

#   if RT_ENABLED
        checkRtScene(rtScene);
#   endif
        checkRasterScene(rasterScene);
    }

    void FramebufferRenderer::endFramebuffers(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, bool rtEnabled) {
        bool shaderViewRtEnabled = false;
        std::vector<BufferUploader::Upload> shaderUploads = {
            { renderIndicesVector.data(), { 0, renderIndicesVector.size() }, sizeof(interop::RenderIndices), RenderBufferFlag::STORAGE, { }, &renderIndicesBuffer},
            { &frameParams, { 0, 1 }, sizeof(interop::FrameParams), RenderBufferFlag::CONSTANT, { }, &frameParamsBuffer}
        };

#   if RT_ENABLED
        // FIXME: Add support for multiple raytracing scenes.
        Framebuffer *chosenFramebuffer = nullptr;
        RaytracingScene *chosenRtScene = nullptr;
        if (rtEnabled) {
            rtResources->updateBottomLevelASResources(worker);

            for (uint32_t i = 0; i < framebufferCount; i++) {
                RenderTargetDrawCall &targetDrawCall = framebufferVector[i].renderTargetDrawCall;
                if (!targetDrawCall.rtScenes.empty()) {
                    chosenFramebuffer = &framebufferVector[i];
                    chosenRtScene = &targetDrawCall.rtScenes[0];
                }
            }
        }

        if (chosenRtScene != nullptr) {
            if (rtResources->updateOutputBuffers) {
                rtResources->createOutputBuffers(worker, chosenRtScene->screenWidth, chosenRtScene->screenHeight);
                rtResources->updateOutputBuffers = false;
            }

            const RenderTarget *framebufferTarget = chosenFramebuffer->renderTargetDrawCall.fbStorage->colorTarget;
            interleavedRastersCount = static_cast<uint32_t>(chosenRtScene->interleavedRasters.size());
            rtResources->updateInterleavedRenderTargets(worker, chosenRtScene->screenWidth, chosenRtScene->screenHeight, interleavedRastersCount, framebufferTarget->multisampling, framebufferTarget->usesHDR);

            for (uint32_t i = 0; i < interleavedRastersCount; i++) {
                auto &intRaster = chosenRtScene->interleavedRasters[i];
                RenderTarget *colorTarget = rtResources->interleavedColorTargetVector[i].get();
                RenderTarget *depthTarget = rtResources->interleavedDepthTargetVector[i].get();
                intRaster.colorTextureIndex = getTextureIndex(colorTarget);
                intRaster.depthTextureIndex = getTextureIndex(depthTarget);
                chosenFramebuffer->transitionRenderTargetSet.emplace(colorTarget);
                chosenFramebuffer->transitionRenderTargetSet.emplace(depthTarget);
            }

            // Must have at least one element in the vector.
            if (chosenRtScene->interleavedRasters.empty()) {
                chosenRtScene->interleavedRasters.emplace_back();
            }

            shaderUploads.push_back({ &rtResources->rtParams, { 0, 1 }, sizeof(interop::RaytracingParams), RenderBufferFlag::CONSTANT, { }, &rtResources->rtParamsBuffer });
            shaderUploads.push_back({ chosenRtScene->interleavedRasters.data(), { 0, chosenRtScene->interleavedRasters.size() }, sizeof(interop::InterleavedRaster), RenderBufferFlag::STORAGE, { }, &interleavedRastersBuffer });

            updateRaytracingScene(worker, *chosenRtScene);
            shaderViewRtEnabled = true;
        }
#   endif

        shaderUploader->submit(worker, shaderUploads);
        updateShaderViews(worker, drawBuffers, outputBuffers, shaderViewRtEnabled);
    }

    void FramebufferRenderer::advanceFrame(bool rtEnabled) {
        frameParams.frameCount++;

#   if RT_ENABLED
        if (rtEnabled) {
            rtResources->swapBuffers = !rtResources->swapBuffers;
            rtResources->skipReprojection = false;
        }
#   endif
    }
};

/*
void RT64::View::renderIm3d() {
    if (Im3d::GetDrawListCount() > 0) {
        commandList->SetGraphicsRootSignature(worker->device->getIm3dRootSignature());

        commandList->SetDescriptorHeaps(1, &descriptorHeap);
        commandList->SetGraphicsRootDescriptorTable(0, descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        commandList->RSSetViewports(1, &rtViewport);
        commandList->RSSetScissorRects(1, &rtScissor);

        unsigned int totalVertexCount = 0;
        for (Im3d::U32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i) {
            auto &drawList = Im3d::GetDrawLists()[i];
            totalVertexCount += drawList.m_vertexCount;
        }

        if (totalVertexCount > 0) {
            // Release the previous vertex buffer if it should be bigger.
            if (!im3dVertexBuffer.IsNull() && (totalVertexCount > im3dVertexCount)) {
                im3dVertexBuffer.Release();
            }

            // Create the vertex buffer if it's empty.
            const UINT vertexBufferSize = totalVertexCount * sizeof(Im3d::VertexData);
            if (im3dVertexBuffer.IsNull()) {
                CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
                im3dVertexBuffer = worker->device->allocateResource(D3D12_HEAP_TYPE_UPLOAD, &uploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
                im3dVertexCount = totalVertexCount;
                im3dVertexBufferView.BufferLocation = im3dVertexBuffer.Get()->GetGPUVirtualAddress();
                im3dVertexBufferView.StrideInBytes = sizeof(Im3d::VertexData);
                im3dVertexBufferView.SizeInBytes = vertexBufferSize;
            }

            // Copy data to vertex buffer.
            UINT8 *pDataBegin;
            CD3DX12_RANGE readRange(0, 0);
            D3D12_CHECK(im3dVertexBuffer.Get()->Map(0, &readRange, reinterpret_cast<void **>(&pDataBegin)));
            for (Im3d::U32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i) {
                auto &drawList = Im3d::GetDrawLists()[i];
                size_t copySize = sizeof(Im3d::VertexData) * drawList.m_vertexCount;
                memcpy(pDataBegin, drawList.m_vertexData, copySize);
                pDataBegin += copySize;
            }

            im3dVertexBuffer.Get()->Unmap(0, nullptr);

            unsigned int vertexOffset = 0;
            for (Im3d::U32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i) {
                auto &drawList = Im3d::GetDrawLists()[i];
                commandList->IASetVertexBuffers(0, 1, &im3dVertexBufferView);
                switch (drawList.m_primType) {
                case Im3d::DrawPrimitive_Points:
                    commandList->SetPipelineState(worker->device->getIm3dPipelineStatePoint());
                    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
                    break;
                case Im3d::DrawPrimitive_Lines:
                    commandList->SetPipelineState(worker->device->getIm3dPipelineStateLine());
                    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
                    break;
                case Im3d::DrawPrimitive_Triangles:
                    commandList->SetPipelineState(worker->device->getIm3dPipelineStateTriangle());
                    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    break;
                default:
                    break;
                }

                commandList->DrawInstanced(drawList.m_vertexCount, 1, vertexOffset, 0);
                vertexOffset += drawList.m_vertexCount;
            }
        }
    }
}
*/

/*
namespace RT64 {
    class View {
    private:
        // Im3D
        AllocatedResource im3dVertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW im3dVertexBufferView;
        unsigned int im3dVertexCount;
    };
};
*/
