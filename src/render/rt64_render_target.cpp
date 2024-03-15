//
// RT64
//

#include "rt64_render_target.h"

#include <algorithm>

#include "gbi/rt64_f3d.h"
#include "shared/rt64_fb_common.h"

#include "rt64_raster_shader.h"

#define PRINT_CONSTRUCTOR_DESTRUCTOR 0

namespace RT64 {
    // RenderTarget
    
    const RenderFormat RenderTarget::ColorBufferFormat = RenderFormat::R8G8B8A8_UNORM;
    const RenderFormat RenderTarget::DepthBufferFormat = RenderFormat::D32_FLOAT;
    const long RenderTarget::MaxDimension = 0x4000L;

    RenderTarget::RenderTarget(uint32_t addressForName, Framebuffer::Type type, const RenderMultisampling &multisampling) {
        this->addressForName = addressForName;
        this->type = type;
        this->multisampling = multisampling;

#if PRINT_CONSTRUCTOR_DESTRUCTOR
        fprintf(stdout, "RenderTarget(0x%p)\n", this);
#endif
    }

    RenderTarget::~RenderTarget() {
#if PRINT_CONSTRUCTOR_DESTRUCTOR
        fprintf(stdout, "~RenderTarget(0x%p)\n", this);
#endif
    }

    void RenderTarget::releaseTextures() {
        textureView.reset();
        resolvedTextureView.reset();
        texture.reset();
        resolvedTexture.reset();
        downsampledTexture.reset();
        dummyTexture.reset();
        textureFramebuffer.reset();
        targetCopyDescSet.reset();
        textureCopyDescSet.reset();
        filterDescSet.reset();
        fbWriteDescSet.reset();
    }

    bool RenderTarget::resize(RenderWorker *worker, uint32_t width, uint32_t height) {
        const bool insufficientSize = (this->width < width) || (this->height < height);
        if (insufficientSize) {
            if (type == Framebuffer::Type::Depth) {
                setupDepth(worker, width, height);
            }
            else {
                setupColor(worker, width, height);
            }

            return true;
        }
        else {
            return false;
        }
    }

    void RenderTarget::setupColor(RenderWorker *worker, uint32_t width, uint32_t height) {
        assert(worker != nullptr);

        releaseTextures();

        this->width = width;
        this->height = height;

        downsampleScale = 0;
        format = ColorBufferFormat;
        
        RenderClearValue clearValue = RenderClearValue::Color(RenderColor(), format);
        texture = worker->device->createTexture(RenderTextureDesc::ColorTarget(width, height, format, multisampling, &clearValue));
        textureView = texture->createTextureView(RenderTextureViewDesc::Texture2D(format));
        texture->setName("Render Target Color #" + std::to_string(addressForName));
        textureRevision++;

        if (multisampling.sampleCount > 1) {
            resolvedTexture = worker->device->createTexture(RenderTextureDesc::ColorTarget(width, height, format, RenderMultisampling(), &clearValue));
            resolvedTextureView = resolvedTexture->createTextureView(RenderTextureViewDesc::Texture2D(format));
            resolvedTexture->setName("Render Target Color Resolved #" + std::to_string(addressForName));
        }
    }

    void RenderTarget::setupDepth(RenderWorker *worker, uint32_t width, uint32_t height) {
        assert(worker != nullptr);

        releaseTextures();

        this->width = width;
        this->height = height;

        downsampleScale = 0;
        format = DepthBufferFormat;
        
        RenderClearValue clearValue = RenderClearValue::Depth(RenderDepth(), RenderFormat::D32_FLOAT);
        texture = worker->device->createTexture(RenderTextureDesc::DepthTarget(width, height, format, multisampling, &clearValue));
        textureView = texture->createTextureView(RenderTextureViewDesc::Texture2D(format));
        texture->setName("Render Target Depth #" + std::to_string(addressForName));
        textureRevision++;
    }

    void RenderTarget::setupDummy(RenderWorker *worker) {
        assert(worker != nullptr);
        
        if (dummyTexture == nullptr) {
            dummyTexture = worker->device->createTexture(RenderTextureDesc::ColorTarget(width, height, ColorBufferFormat, multisampling));
            dummyTexture->setName("Render Target Dummy");
        }
    }

    void RenderTarget::setupColorFramebuffer(RenderWorker *worker) {
        assert(worker != nullptr);

        if (textureFramebuffer == nullptr) {
            const RenderTexture *colorTexture = texture.get();
            textureFramebuffer = worker->device->createFramebuffer(RenderFramebufferDesc(&colorTexture, 1));
        }
    }

    void RenderTarget::setupDepthFramebuffer(RenderWorker *worker) {
        assert(worker != nullptr);

        setupDummy(worker);

        if (textureFramebuffer == nullptr) {
            const RenderTexture *colorTexture = dummyTexture.get();
            textureFramebuffer = worker->device->createFramebuffer(RenderFramebufferDesc(&colorTexture, 1, texture.get()));
        }
    }
    
    void RenderTarget::copyFromTarget(RenderWorker *worker, RenderTarget *src, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);
        assert(src != nullptr);
        assert(format != src->format);

        // Select shader based on the formats.
        RenderTextureLayout requiredTextureLayout = RenderTextureLayout::UNKNOWN;
        const ShaderRecord *shaderRecord = nullptr;
        bool useDummyTexture = false;
        const bool srcUsesMSAA = (src->multisampling.sampleCount > 1);
        if ((format == ColorBufferFormat) && (src->format == DepthBufferFormat)) {
            shaderRecord = srcUsesMSAA ? &shaderLibrary->rtCopyDepthToColorMS : &shaderLibrary->rtCopyDepthToColor;
            requiredTextureLayout = RenderTextureLayout::COLOR_WRITE;
            setupColorFramebuffer(worker);
        }
        else if ((format == DepthBufferFormat) && (src->format == ColorBufferFormat)) {
            useDummyTexture = true;
            shaderRecord = srcUsesMSAA ? &shaderLibrary->rtCopyColorToDepthMS : &shaderLibrary->rtCopyColorToDepth;
            requiredTextureLayout = RenderTextureLayout::DEPTH_WRITE;
            setupDepthFramebuffer(worker);
        }
        else {
            assert(false && "Unsupported render target copy.");
        }

        if (src->targetCopyDescSet == nullptr) {
            src->targetCopyDescSet = std::make_unique<RenderTargetCopyDescriptorSet>(worker->device);
            src->targetCopyDescSet->setTexture(src->targetCopyDescSet->gInput, src->texture.get(), RenderTextureLayout::SHADER_READ, src->textureView.get());
        }

        thread_local std::vector<RenderTextureBarrier> framebufferBarriers;
        framebufferBarriers.clear();
        framebufferBarriers.emplace_back(RenderTextureBarrier(texture.get(), requiredTextureLayout));
        framebufferBarriers.emplace_back(RenderTextureBarrier(src->texture.get(), RenderTextureLayout::SHADER_READ));
        if (useDummyTexture) {
            framebufferBarriers.emplace_back(RenderTextureBarrier(dummyTexture.get(), RenderTextureLayout::COLOR_WRITE));
        }

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, framebufferBarriers);
        worker->commandList->setFramebuffer(textureFramebuffer.get());

        // Record the drawing command.
        RenderViewport targetViewport = RenderViewport(float(x), float(y), float(width), float(height));
        RenderRect targetRect(x, y, x + width, y + height);
        worker->commandList->setViewports(targetViewport);
        worker->commandList->setScissors(targetRect);
        worker->commandList->setPipeline(shaderRecord->pipeline.get());
        worker->commandList->setGraphicsPipelineLayout(shaderRecord->pipelineLayout.get());
        worker->commandList->setGraphicsDescriptorSet(src->targetCopyDescSet->get(), 0);
        worker->commandList->setVertexBuffers(0, nullptr, 0, nullptr);
        worker->commandList->drawInstanced(3, 1, 0, 0);

        markForResolve();
    }

    void RenderTarget::resolveFromTarget(RenderWorker *worker, RenderTarget *src) {
        assert(!usesResolve() && "The target must not be an MSAA target to allow resolving from other targets.");

        RenderTextureBarrier resolveBarriers[] = {
            RenderTextureBarrier(src->texture.get(), RenderTextureLayout::RESOLVE_SOURCE),
            RenderTextureBarrier(texture.get(), RenderTextureLayout::RESOLVE_DEST)
        };
        
        const RenderRect srcRect(0, 0, std::min(width, src->width), std::min(height, src->height));
        worker->commandList->barriers(RenderBarrierStage::COPY, resolveBarriers, uint32_t(std::size(resolveBarriers)));
        worker->commandList->resolveTextureRegion(texture.get(), 0, 0, src->texture.get(), &srcRect);
    }

    void RenderTarget::copyFromChanges(RenderWorker *worker, const FramebufferChange &fbChange, uint32_t fbWidth, uint32_t fbHeight, uint32_t rowStart, const hlslpp::float2 resolutionScale, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);
        assert(fbChange.used);

        // Select shader based on the formats.
        RenderTextureLayout requiredTextureLayout = RenderTextureLayout::UNKNOWN;
        const ShaderRecord *shaderRecord = nullptr;
        bool useDummyTexture = false;
        if (format == ColorBufferFormat) {
            shaderRecord = &shaderLibrary->fbChangesDrawColor;
            requiredTextureLayout = RenderTextureLayout::COLOR_WRITE;
            setupColorFramebuffer(worker);
        }
        else if (format == DepthBufferFormat) {
            useDummyTexture = true;
            shaderRecord = &shaderLibrary->fbChangesDrawDepth;
            requiredTextureLayout = RenderTextureLayout::DEPTH_WRITE;
            setupDepthFramebuffer(worker);
        }
        else {
            assert(false && "Unsupported buffer format.");
        }

        thread_local std::vector<RenderTextureBarrier> framebufferBarriers;
        framebufferBarriers.clear();
        framebufferBarriers.emplace_back(RenderTextureBarrier(texture.get(), requiredTextureLayout));
        if (fbChange.pixelTexture != nullptr) { framebufferBarriers.emplace_back(RenderTextureBarrier(fbChange.pixelTexture.get(), RenderTextureLayout::SHADER_READ)); }
        if (fbChange.booleanTexture != nullptr) { framebufferBarriers.emplace_back(RenderTextureBarrier(fbChange.booleanTexture.get(), RenderTextureLayout::SHADER_READ)); }
        if (useDummyTexture) { framebufferBarriers.emplace_back(RenderTextureBarrier(dummyTexture.get(), RenderTextureLayout::COLOR_WRITE)); }

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, framebufferBarriers);
        worker->commandList->setFramebuffer(textureFramebuffer.get());

        struct FbChangesDrawCommonCB {
            uint32_t Resolution[2];
        };

        FbChangesDrawCommonCB commonCB;
        commonCB.Resolution[0] = fbWidth;
        commonCB.Resolution[1] = fbHeight;

        // Adjust the viewport and scissor to the center.
        // TODO: Should it query from the FB pair somehow if it's an FB pair that is supposed to have aspect ratio adjustment?
        float targetWidth, targetHeight, targetLeft, targetTop;
        const bool pillarBox = resolutionScale[0] > resolutionScale[1];
        if (pillarBox) {
            targetWidth = fbWidth * resolutionScale.y;
            targetHeight = fbHeight * resolutionScale.y;
            targetLeft = ((fbWidth * resolutionScale.x) / 2.0f) - (targetWidth / 2.0f);
            targetTop = rowStart * resolutionScale.y;
        }
        else {
            targetWidth = fbWidth * resolutionScale.x;
            targetHeight = fbHeight * resolutionScale.x;
            targetLeft = 0.0f;
            targetTop = rowStart * resolutionScale.y + ((fbHeight * resolutionScale.y) / 2.0f) - (targetHeight / 2.0f);
        }
        
        // Record the drawing command.
        long scissorLeft = long(floor(targetLeft));
        long scissorTop = long(floor(targetTop));
        long scissorRight = long(ceil(targetLeft + targetWidth));
        long scissorBottom = long(ceil(targetTop + targetHeight));
        RenderViewport targetViewport = RenderViewport(targetLeft, targetTop, targetWidth, targetHeight);
        RenderRect targetRect(scissorLeft, scissorTop, scissorRight, scissorBottom);
        worker->commandList->setViewports(targetViewport);
        worker->commandList->setScissors(targetRect);
        worker->commandList->setPipeline(shaderRecord->pipeline.get());
        worker->commandList->setGraphicsPipelineLayout(shaderRecord->pipelineLayout.get());
        worker->commandList->setGraphicsPushConstants(0, &commonCB);
        worker->commandList->setGraphicsDescriptorSet(fbChange.drawDescSet->get(), 0);
        worker->commandList->setVertexBuffers(0, nullptr, 0, nullptr);
        worker->commandList->drawInstanced(3, 1, 0, 0);

        markForResolve();
    }

    void RenderTarget::clearColorTarget(RenderWorker *worker) {
        assert(worker != nullptr);

        setupColorFramebuffer(worker);

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(texture.get(), RenderTextureLayout::COLOR_WRITE));
        worker->commandList->setFramebuffer(textureFramebuffer.get());
        worker->commandList->clearColor();

        markForResolve();
    }

    void RenderTarget::clearDepthTarget(RenderWorker *worker) {
        assert(worker != nullptr);

        setupDepthFramebuffer(worker);
        
        RenderTextureBarrier clearBarriers[] = {
            RenderTextureBarrier(texture.get(), RenderTextureLayout::DEPTH_WRITE),
            RenderTextureBarrier(dummyTexture.get(), RenderTextureLayout::COLOR_WRITE)
        };

        worker->commandList->barriers(RenderBarrierStage::GRAPHICS, clearBarriers, uint32_t(std::size(clearBarriers)));
        worker->commandList->setFramebuffer(textureFramebuffer.get());
        worker->commandList->clearDepth();
        
        markForResolve();
    }

    void RenderTarget::downsampleTarget(RenderWorker *worker, uint32_t scale, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);

        resolveTarget(worker);

        struct BoxFilterCB {
            interop::int2 Resolution;
            interop::int2 ResolutionScale;
            interop::int2 Misalignment;
        };

        if (downsampleScale != scale) {
            downsampledTexture.reset(nullptr);
            downsampleScale = 0;
        }

        uint32_t scaledWidth = std::max(width / scale, 1U);
        uint32_t scaledHeight = std::max(height / scale, 1U);
        if (downsampledTexture == nullptr) {
            downsampledTexture = worker->device->createTexture(RenderTextureDesc::Texture2D(scaledWidth, scaledHeight, 1, ColorBufferFormat, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
            downsampledTexture->setName("Render Target Downsampled");
        }

        if (filterDescSet == nullptr) {
            filterDescSet = std::make_unique<BoxFilterDescriptorSet>(worker->device);
        }

        filterDescSet->setTexture(filterDescSet->gInput, getResolvedTexture(), RenderTextureLayout::SHADER_READ);
        filterDescSet->setTexture(filterDescSet->gOutput, downsampledTexture.get(), RenderTextureLayout::GENERAL);

        BoxFilterCB boxFilterCB;
        boxFilterCB.Resolution[0] = width;
        boxFilterCB.Resolution[1] = height;
        boxFilterCB.ResolutionScale[0] = scale;
        boxFilterCB.ResolutionScale[1] = scale;
        boxFilterCB.Misalignment[0] = invMisalignX;
        boxFilterCB.Misalignment[1] = 0;
        
        RenderTextureBarrier filterBarriers[] = {
            RenderTextureBarrier(getResolvedTexture(), RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(downsampledTexture.get(), RenderTextureLayout::GENERAL)
        };

        // Execute the compute shader for downsampling the image.
        const uint32_t BlockSize = 8;
        const uint32_t dispatchX = (scaledWidth + BlockSize - 1) / BlockSize;
        const uint32_t dispatchY = (scaledHeight + BlockSize - 1) / BlockSize;
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, filterBarriers, uint32_t(std::size(filterBarriers)));
        worker->commandList->setPipeline(shaderLibrary->boxFilter.pipeline.get());
        worker->commandList->setComputePipelineLayout(shaderLibrary->boxFilter.pipelineLayout.get());
        worker->commandList->setComputePushConstants(0, &boxFilterCB);
        worker->commandList->setComputeDescriptorSet(filterDescSet->get(), 0);
        worker->commandList->dispatch(dispatchX, dispatchY, 1);

        downsampleScale = scale;
    }

    void RenderTarget::resolveTarget(RenderWorker *worker) {
        if (!resolvedTextureDirty || !usesResolve()) {
            return;
        }
        
        RenderTextureBarrier resolveBarriers[] = {
            RenderTextureBarrier(texture.get(), RenderTextureLayout::RESOLVE_SOURCE),
            RenderTextureBarrier(resolvedTexture.get(), RenderTextureLayout::RESOLVE_DEST)
        };

        worker->commandList->barriers(RenderBarrierStage::COPY, resolveBarriers, uint32_t(std::size(resolveBarriers)));
        worker->commandList->resolveTexture(resolvedTexture.get(), texture.get());
        resolvedTextureDirty = false;
    }

    void RenderTarget::markForResolve() {
        resolvedTextureDirty = true;
    }

    bool RenderTarget::usesResolve() const {
        return resolvedTexture != nullptr;
    }

    RenderTexture *RenderTarget::getResolvedTexture() const {
        return usesResolve() ? resolvedTexture.get() : texture.get();
    }
    
    RenderTextureView *RenderTarget::getResolvedTextureView() const {
        return usesResolve() ? resolvedTextureView.get() : textureView.get();
    }

    bool RenderTarget::isEmpty() const {
        return texture == nullptr;
    }

    void RenderTarget::computeScaledSize(uint32_t nativeWidth, uint32_t nativeHeight, hlslpp::float2 resolutionScale, uint32_t &scaledWidth, uint32_t &scaledHeight, uint32_t &misalignmentX) {
        const long nativeColorWidthClamped = std::clamp(lround(nativeWidth * resolutionScale.y), 1L, RenderTarget::MaxDimension);
        const long expandedColorWidthClamped = std::clamp(lround(nativeWidth * resolutionScale.x), 1L, RenderTarget::MaxDimension);
        const long colorHeightClamped = std::clamp(lround(nativeHeight * resolutionScale.y), 1L, RenderTarget::MaxDimension);
        scaledWidth = uint32_t(expandedColorWidthClamped);
        scaledHeight = uint32_t(colorHeightClamped);

        const long expandedPixels = std::labs(scaledWidth - nativeColorWidthClamped) / 2;
        const long nativeAlignment = std::max(lround(resolutionScale.y), 1L);
        misalignmentX = (nativeAlignment - (expandedPixels % nativeAlignment)) % nativeAlignment;
    }

    hlslpp::float2 RenderTarget::computeFixedResolutionScale(uint32_t nativeWidth, hlslpp::float2 resolutionScale) {
        long expandedColorWidthClamped = std::clamp(lround(nativeWidth * resolutionScale.x), 1L, RenderTarget::MaxDimension);

        // Alter the resolution scale so it outputs an even resolution number.
        expandedColorWidthClamped += expandedColorWidthClamped & 0x1;
        resolutionScale.x = float(expandedColorWidthClamped) / float(nativeWidth);
        return resolutionScale;
    }
};