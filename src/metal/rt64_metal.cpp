#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <xxHash/xxh3.h>

#include "rt64_metal.h"
#include "rt64_metal_helpers.h"
#include "common/rt64_apple.h"

namespace RT64 {
    static constexpr size_t MAX_DRAWABLES = 3;
    static constexpr size_t MAX_DESCRIPTOR_SET_COUNT = 8;

    // MARK: - Helper Structures

    MetalDescriptorSetLayout::MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);
        this->device = device;

        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        // Pre-allocate vectors with known size
        const uint32_t totalDescriptors = desc.descriptorRangesCount + (desc.lastRangeIsBoundless ? desc.boundlessRangeSize : 0);
        descriptorTypes.reserve(totalDescriptors);
        descriptorIndexBases.reserve(totalDescriptors);
        descriptorRangeBinding.reserve(totalDescriptors);
        argumentDescriptors.reserve(totalDescriptors);

        // First pass: Calculate descriptor bases and bindings
        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &range = desc.descriptorRanges[i];
            uint32_t indexBase = uint32_t(descriptorIndexBases.size());

            descriptorIndexBases.resize(descriptorIndexBases.size() + range.count, indexBase);
            descriptorRangeBinding.resize(descriptorRangeBinding.size() + range.count, range.binding);
        }

        // Sort ranges by binding due to how spirv-cross orders them
        std::vector<RenderDescriptorRange> sortedRanges(desc.descriptorRanges, desc.descriptorRanges + desc.descriptorRangesCount);
        std::sort(sortedRanges.begin(), sortedRanges.end(), [](const RenderDescriptorRange &a, const RenderDescriptorRange &b) {
            return a.binding < b.binding;
        });

        // Second pass: Create argument descriptors and handle immutable samplers
        uint32_t rangeCount = desc.lastRangeIsBoundless ? desc.descriptorRangesCount - 1 : desc.descriptorRangesCount;

        for (uint32_t i = 0; i < rangeCount; i++) {
            const RenderDescriptorRange &range = sortedRanges[i];

            // Add descriptor types
            descriptorTypes.resize(descriptorTypes.size() + range.count, range.type);
            entryCount += range.count;

            // Handle immutable samplers
            if (range.immutableSampler) {
                for (uint32_t j = 0; j < range.count; j++) {
                    const auto *sampler = static_cast<const MetalSampler *>(range.immutableSampler[j]);
                    staticSamplers.push_back(sampler->state);
                    samplerIndices.push_back(range.binding + j);
                }
            }

            // Create argument descriptor
            MTL::ArgumentDescriptor *argumentDesc = MTL::ArgumentDescriptor::alloc()->init();
            argumentDesc->setDataType(metal::mapDataType(range.type));
            argumentDesc->setIndex(range.binding);
            argumentDesc->setArrayLength(range.count > 1 ? range.count : 0);

            if (range.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc->setTextureType(MTL::TextureType2D);
            } else if (range.type == RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER || range.type == RenderDescriptorRangeType::FORMATTED_BUFFER) {
                argumentDesc->setTextureType(MTL::TextureTypeTextureBuffer);
            }

            argumentDescriptors.push_back(argumentDesc);
        }

        // Handle boundless range if present
        if (desc.lastRangeIsBoundless) {
            const RenderDescriptorRange &lastRange = desc.descriptorRanges[desc.descriptorRangesCount - 1];
            descriptorTypes.push_back(lastRange.type);
            entryCount += std::max(desc.boundlessRangeSize, 1U);

            MTL::ArgumentDescriptor *argumentDesc = MTL::ArgumentDescriptor::alloc()->init();
            argumentDesc->setDataType(metal::mapDataType(lastRange.type));
            argumentDesc->setIndex(lastRange.binding);
            argumentDesc->setArrayLength(8192); // Fixed upper bound for Metal

            if (lastRange.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc->setTextureType(MTL::TextureType2D);
            } else if (lastRange.type == RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER || lastRange.type == RenderDescriptorRangeType::FORMATTED_BUFFER) {
                argumentDesc->setTextureType(MTL::TextureTypeTextureBuffer);
            }

            argumentDescriptors.push_back(argumentDesc);
        }

        assert(argumentDescriptors.size() > 0);
        descriptorTypeMaxIndex = descriptorTypes.empty() ? 0 : uint32_t(descriptorTypes.size() - 1);
        
        // Create and initialize argument encoder
        NS::Array* pArray = (NS::Array*)CFArrayCreate(kCFAllocatorDefault, (const void **)argumentDescriptors.data(), argumentDescriptors.size(), &kCFTypeArrayCallBacks);
        argumentEncoder = device->mtl->newArgumentEncoder(pArray);

        createEncoderAndBuffer();

        // Release resources
        pArray->release();
        releasePool->release();
    }

    MetalDescriptorSetLayout::~MetalDescriptorSetLayout() {
        argumentEncoder->release();
        descriptorBuffer->release();

        // Release argument descriptors
        for (MTL::ArgumentDescriptor *desc : argumentDescriptors) {
            desc->release();
        }

        // Release samplers
        for (MTL::SamplerState *sampler : staticSamplers) {
            sampler->release();
        }
    }

    void MetalDescriptorSetLayout::createEncoderAndBuffer() {
        // Release previous resources
        if (descriptorBuffer) {
            descriptorBuffer->release();
        }
        
        auto argumentBufferStorageMode =
#if TARGET_OS_IOS
        MTL::ResourceStorageModeShared;
#else
        MTL::ResourceStorageModeManaged;
#endif

        auto g = argumentEncoder->encodedLength();
        descriptorBuffer = device->mtl->newBuffer(argumentEncoder->encodedLength(), argumentBufferStorageMode);
        argumentEncoder->setArgumentBuffer(descriptorBuffer, 0);

        // Set static samplers
        for (size_t i = 0; i < staticSamplers.size(); i++) {
            argumentEncoder->setSamplerState(staticSamplers[i], samplerIndices[i]);
        }
    }

    // MetalBuffer

    MetalBuffer::MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->pool = pool;
        this->desc = desc;
        this->device = device;

        this->mtl = device->mtl->newBuffer(desc.size, metal::mapResourceOption(desc.heapType));
    }

    MetalBuffer::~MetalBuffer() {
        mtl->release();
    }

    void* MetalBuffer::map(uint32_t subresource, const RenderRange* readRange) {
        return mtl->contents();
    }

    void MetalBuffer::unmap(uint32_t subresource, const RenderRange* writtenRange) {
        // Do nothing.
    }

    std::unique_ptr<RenderBufferFormattedView> MetalBuffer::createBufferFormattedView(RenderFormat format) {
        return std::make_unique<MetalBufferFormattedView>(this, format);
    }

    void MetalBuffer::setName(const std::string &name) {
        NS::String *label = NS::String::string(name.c_str(), NS::UTF8StringEncoding);
        mtl->setLabel(label);
    }

    // MetalBufferFormattedView

    MetalBufferFormattedView::MetalBufferFormattedView(RT64::MetalBuffer *buffer, RT64::RenderFormat format) {
        assert(buffer != nullptr);
        assert((buffer->desc.flags & RenderBufferFlag::FORMATTED) && "Buffer must allow formatted views.");

        this->buffer = buffer;

        // Calculate texture properties
        const uint32_t width = buffer->desc.size / RenderFormatSize(format);
        const size_t rowAlignment = metal::alignmentForRenderFormat(buffer->device->mtl, format);
        const auto bytesPerRow = mem::alignUp(buffer->desc.size, rowAlignment);

        // Configure texture properties
        auto pixelFormat = metal::mapPixelFormat(format);
        auto usage = metal::mapTextureUsageFromBufferFlags(buffer->desc.flags);
        auto options = metal::mapResourceOption(buffer->desc.heapType);

        // Create texture with configured descriptor and alignment
        auto descriptor = MTL::TextureDescriptor::textureBufferDescriptor(pixelFormat, width, options, usage);
        this->texture = buffer->mtl->newTexture(descriptor, 0, bytesPerRow);

        descriptor->release();
    }

    MetalBufferFormattedView::~MetalBufferFormattedView() {
        texture->release();
    }

    // MetalTexture

    MetalTexture::MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc) {
        assert(device != nullptr);

        this->pool = pool;
        this->desc = desc;

        auto descriptor = MTL::TextureDescriptor::alloc()->init();
        auto textureType = metal::mapTextureType(desc.dimension, desc.multisampling.sampleCount);

        descriptor->setTextureType(textureType);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        descriptor->setPixelFormat(metal::mapPixelFormat(desc.format));
        descriptor->setWidth(desc.width);
        descriptor->setHeight(desc.height);
        descriptor->setDepth(desc.depth);
        descriptor->setMipmapLevelCount(desc.mipLevels);
        descriptor->setArrayLength(1);
        descriptor->setSampleCount(desc.multisampling.sampleCount);

        MTL::TextureUsage usage = metal::mapTextureUsage(desc.flags);
        descriptor->setUsage(usage);

        if (pool != nullptr) {
            this->mtl = pool->heap->newTexture(descriptor);
        } else {
            this->mtl = device->mtl->newTexture(descriptor);
        }

        // Release resources
        descriptor->release();
    }

    MetalTexture::~MetalTexture() {
        mtl->release();
    }

    std::unique_ptr<RenderTextureView> MetalTexture::createTextureView(const RenderTextureViewDesc &desc) {
        return std::make_unique<MetalTextureView>(this, desc);
    }

    void MetalTexture::setName(const std::string &name) {
        mtl->setLabel(NS::String::string(name.c_str(), NS::UTF8StringEncoding));
    }

    // MetalTextureView

    MetalTextureView::MetalTextureView(MetalTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);
        assert(texture->desc.dimension == desc.dimension && "Creating a view with a different dimension is currently not supported.");
        this->texture = texture->mtl->newTextureView(metal::mapPixelFormat(desc.format), texture->mtl->textureType(), { desc.mipSlice, desc.mipLevels }, { 0, texture->arrayCount });
    }

    MetalTextureView::~MetalTextureView() {
        texture->release();
    }

    // MetalAccelerationStructure

    MetalAccelerationStructure::MetalAccelerationStructure(MetalDevice *device, const RenderAccelerationStructureDesc &desc) {
        assert(device != nullptr);
        assert(desc.buffer.ref != nullptr);

        this->device = device;
        this->type = desc.type;
    }

    MetalAccelerationStructure::~MetalAccelerationStructure() {
        // TODO: Should be handled by ARC
    }

    // MetalPool

    MetalPool::MetalPool(MetalDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);
        this->device = device;

//        MTL::HeapDescriptor *descriptor = MTL::HeapDescriptor::alloc()->init();
//        // TODO: Set Descriptor properties correctly
//        descriptor->setType(MTL::HeapTypeAutomatic);
//
//        this->heap = device->mtl->newHeap(descriptor);
//
//        // Release resources
//        descriptor->release();
    }

    MetalPool::~MetalPool() {
        // heap->release();
    }

    std::unique_ptr<RenderBuffer> MetalPool::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<MetalBuffer>(device, this, desc);
    }

    std::unique_ptr<RenderTexture> MetalPool::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<MetalTexture>(device, this, desc);
    }

    // MetalShader

    MetalShader::MetalShader(MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        assert(device != nullptr);
        assert(data != nullptr);
        assert(size > 0);
        assert(format == RenderShaderFormat::METAL);

        this->format = format;
        this->functionName = (entryPointName != nullptr) ? NS::String::string(entryPointName, NS::UTF8StringEncoding) : MTLSTR("");

        NS::Error *error;
        dispatch_data_t dispatchData = dispatch_data_create(data, size, dispatch_get_main_queue(), ^{});
        library = device->mtl->newLibrary(dispatchData, &error);

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newLibraryWithSource: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }
    }

    MetalShader::~MetalShader() {
        functionName->release();
        library->release();
    }

    MTL::Function* MetalShader::createFunction(const RenderSpecConstant *specConstants, uint32_t specConstantsCount) const {
        if (specConstants != nullptr) {
            MTL::FunctionConstantValues *values = MTL::FunctionConstantValues::alloc()->init();
            for (uint32_t i = 0; i < specConstantsCount; i++) {
                const RenderSpecConstant &specConstant = specConstants[i];
                values->setConstantValue(&specConstant.value, MTL::DataTypeUInt, specConstant.index);
            }

            NS::Error *error;
            auto function = library->newFunction(functionName, values, &error);
//            values->release();

            if (error != nullptr) {
                fprintf(stderr, "MTLLibrary newFunction: failed with error: %ld.\n", error->code());
                return nullptr;
            }

            return function;
        } else {
            return library->newFunction(functionName);
        }
    }

    // MetalSampler

    MetalSampler::MetalSampler(MetalDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        MTL::SamplerDescriptor *descriptor = MTL::SamplerDescriptor::alloc()->init();
        descriptor->setSupportArgumentBuffers(true);
        descriptor->setMinFilter(metal::mapSamplerMinMagFilter(desc.minFilter));
        descriptor->setMagFilter(metal::mapSamplerMinMagFilter(desc.magFilter));
        descriptor->setMipFilter(metal::mapSamplerMipFilter(desc.mipmapMode));
        descriptor->setRAddressMode(metal::mapSamplerAddressMode(desc.addressU));
        descriptor->setSAddressMode(metal::mapSamplerAddressMode(desc.addressV));
        descriptor->setTAddressMode(metal::mapSamplerAddressMode(desc.addressW));
        descriptor->setMaxAnisotropy(desc.maxAnisotropy);
        descriptor->setCompareFunction(metal::mapCompareFunction(desc.comparisonFunc));
        descriptor->setLodMinClamp(desc.minLOD);
        descriptor->setLodMaxClamp(desc.maxLOD);
        descriptor->setBorderColor(metal::mapSamplerBorderColor(desc.borderColor));

        this->state = device->mtl->newSamplerState(descriptor);

        // Release resources
        descriptor->release();
    }

    MetalSampler::~MetalSampler() {
        state->release();
    }

    // MetalPipeline

    MetalPipeline::MetalPipeline(MetalDevice *device, MetalPipeline::Type type) {
        assert(device != nullptr);
        assert(type != Type::Unknown);

        this->type = type;
    }

    MetalPipeline::~MetalPipeline() { }

    // MetalComputePipeline

    MetalComputePipeline::MetalComputePipeline(MetalDevice *device, const RenderComputePipelineDesc &desc) : MetalPipeline(device, Type::Compute) {
        assert(desc.computeShader != nullptr);
        assert(desc.pipelineLayout != nullptr);

        const auto *computeShader = static_cast<const MetalShader *>(desc.computeShader);

        MTL::ComputePipelineDescriptor *descriptor = MTL::ComputePipelineDescriptor::alloc()->init();
        auto function = computeShader->createFunction(desc.specConstants, desc.specConstantsCount);
        descriptor->setComputeFunction(function);
        descriptor->setLabel(computeShader->functionName);

        // State variables, initialized here to be reused in encoder re-binding
        state = new MetalComputeState();

        NS::Error *error = nullptr;
        this->state->pipelineState = device->mtl->newComputePipelineState(descriptor, MTL::PipelineOptionNone, nullptr, &error);
        this->state->threadGroupSizeX = desc.threadGroupSizeX;
        this->state->threadGroupSizeY = desc.threadGroupSizeY;
        this->state->threadGroupSizeZ = desc.threadGroupSizeZ;

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newComputePipelineStateWithDescriptor: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }

        // Release resources
        descriptor->release();
        function->release();
    }

    MetalComputePipeline::~MetalComputePipeline() {
        state->pipelineState->release();
        delete state;
    }

    RenderPipelineProgram MetalComputePipeline::getProgram(const std::string &name) const {
        assert(false && "Compute pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalGraphicsPipeline

    MetalGraphicsPipeline::MetalGraphicsPipeline(MetalDevice *device, const RenderGraphicsPipelineDesc &desc) : MetalPipeline(device, Type::Graphics) {
        assert(desc.pipelineLayout != nullptr);
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
        descriptor->setInputPrimitiveTopology(metal::mapPrimitiveTopologyClass(desc.primitiveTopology));
        descriptor->setRasterSampleCount(desc.multisampling.sampleCount);

        assert(desc.vertexShader != nullptr && "Cannot create a valid MTLRenderPipelineState without a vertex shader!");
        const auto *metalShader = static_cast<const MetalShader *>(desc.vertexShader);

        auto vertexFunction = metalShader->createFunction(desc.specConstants, desc.specConstantsCount);
        descriptor->setVertexFunction(vertexFunction);

        MTL::VertexDescriptor *vertexDescriptor = MTL::VertexDescriptor::alloc()->init();

        for (uint32_t i = 0; i < desc.inputSlotsCount; i++) {
            const RenderInputSlot &inputSlot = desc.inputSlots[i];

            auto layout = vertexDescriptor->layouts()->object(i);
            layout->setStride(inputSlot.stride);
            layout->setStepFunction(metal::mapVertexStepFunction(inputSlot.classification));
            layout->setStepRate((layout->stepFunction() == MTL::VertexStepFunctionPerInstance) ? inputSlot.stride : 1);
        }

        for (uint32_t i = 0; i < desc.inputElementsCount; i++) {
            const RenderInputElement &inputElement = desc.inputElements[i];

            auto attributeDescriptor = vertexDescriptor->attributes()->object(i);
            attributeDescriptor->setOffset(inputElement.alignedByteOffset);
            attributeDescriptor->setBufferIndex(inputElement.slotIndex);
            attributeDescriptor->setFormat(metal::mapVertexFormat(inputElement.format));
        }

        descriptor->setVertexDescriptor(vertexDescriptor);

        assert(desc.geometryShader == nullptr && "Metal does not support geometry shaders!");

        if (desc.pixelShader != nullptr) {
            const auto *pixelShader = static_cast<const MetalShader *>(desc.pixelShader);
            auto fragmentFunction = pixelShader->createFunction(desc.specConstants, desc.specConstantsCount);
            descriptor->setFragmentFunction(fragmentFunction);
            fragmentFunction->release();
        }

        for (uint32_t i = 0; i < desc.renderTargetCount; i++) {
            const RenderBlendDesc &blendDesc = desc.renderTargetBlend[i];

            auto blendDescriptor = descriptor->colorAttachments()->object(i);
            blendDescriptor->setBlendingEnabled(blendDesc.blendEnabled);
            blendDescriptor->setSourceRGBBlendFactor(metal::mapBlendFactor(blendDesc.srcBlend));
            blendDescriptor->setDestinationRGBBlendFactor(metal::mapBlendFactor(blendDesc.dstBlend));
            blendDescriptor->setRgbBlendOperation(metal::mapBlendOperation(blendDesc.blendOp));
            blendDescriptor->setSourceAlphaBlendFactor(metal::mapBlendFactor(blendDesc.srcBlendAlpha));
            blendDescriptor->setDestinationAlphaBlendFactor(metal::mapBlendFactor(blendDesc.dstBlendAlpha));
            blendDescriptor->setAlphaBlendOperation(metal::mapBlendOperation(blendDesc.blendOpAlpha));
            blendDescriptor->setWriteMask(blendDesc.renderTargetWriteMask);
            blendDescriptor->setPixelFormat(metal::mapPixelFormat(desc.renderTargetFormat[i]));
        }

        descriptor->setDepthAttachmentPixelFormat(metal::mapPixelFormat(desc.depthTargetFormat));
        descriptor->setRasterSampleCount(desc.multisampling.sampleCount);

        // State variables, initialized here to be reused in encoder re-binding
        MTL::DepthStencilDescriptor *depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
        depthStencilDescriptor->setDepthWriteEnabled(desc.depthWriteEnabled);
        depthStencilDescriptor->setDepthCompareFunction(desc.depthEnabled ? metal::mapCompareFunction(desc.depthFunction) : MTL::CompareFunctionAlways);

        state = new MetalRenderState();
        state->depthStencilState = device->mtl->newDepthStencilState(depthStencilDescriptor);
        state->cullMode = metal::mapCullMode(desc.cullMode);
        state->depthClipMode = (desc.depthClipEnabled) ? MTL::DepthClipModeClip : MTL::DepthClipModeClamp;
        state->winding = MTL::WindingClockwise;
        state->sampleCount = desc.multisampling.sampleCount;
        if (desc.multisampling.sampleCount > 1) {
            state->samplePositions = new MTL::SamplePosition[desc.multisampling.sampleCount];
            for (uint32_t i = 0; i < desc.multisampling.sampleCount; i++) {
                state->samplePositions[i] = { (float)desc.multisampling.sampleLocations[i].x, (float)desc.multisampling.sampleLocations[i].y };
            }
        }

        NS::Error *error = nullptr;

        this->state->renderPipelineState = device->mtl->newRenderPipelineState(descriptor, &error);

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newRenderPipelineState: failed with error %s.\n", error->localizedDescription()->utf8String());
            return;
        }

        // Releae resources
        vertexDescriptor->release();
        vertexFunction->release();
        descriptor->release();
        depthStencilDescriptor->release();
        releasePool->release();
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline() {
        state->renderPipelineState->release();
        state->depthStencilState->release();
        // TODO: better way to dispose of sample positions
        delete[] state->samplePositions;
        delete state;
    }

    RenderPipelineProgram MetalGraphicsPipeline::getProgram(const std::string &name) const {
        assert(false && "Graphics pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalDescriptorSet

    MetalDescriptorSet::MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);

        entryCount = 0;

        // Figure out the total amount of entries that will be required.
        uint32_t rangeCount = desc.descriptorRangesCount;
        if (desc.lastRangeIsBoundless) {
            assert((desc.descriptorRangesCount > 0) && "There must be at least one descriptor set to define the last range as boundless.");
            rangeCount--;
        }

        // Spirv-cross orders by binding number, so we sort
        std::vector<RenderDescriptorRange> sortedRanges(desc.descriptorRanges, desc.descriptorRanges + desc.descriptorRangesCount);
        std::sort(sortedRanges.begin(), sortedRanges.end(), [](const RenderDescriptorRange &a, const RenderDescriptorRange &b) {
            return a.binding < b.binding;
        });

        for (uint32_t i = 0; i < rangeCount; i++) {
            const RenderDescriptorRange &range = sortedRanges[i];
            for (uint32_t j = 0; j < range.count; j++) {
                descriptorTypes.emplace_back(range.type);
                entryCount++;
            }
        }

        if (desc.lastRangeIsBoundless) {
            const RenderDescriptorRange &lastDescriptorRange = desc.descriptorRanges[desc.descriptorRangesCount - 1];
            descriptorTypes.emplace_back(lastDescriptorRange.type);

            // Ensure at least one entry is created for boundless ranges.
            entryCount += std::max(desc.boundlessRangeSize, 1U);
        }

        if (!descriptorTypes.empty()) {
            descriptorTypeMaxIndex = uint32_t(descriptorTypes.size()) - 1;
        }
    }

    MetalDescriptorSet::MetalDescriptorSet(MetalDevice *device, uint32_t entryCount) {
        assert(device != nullptr);
        assert(entryCount > 0);

        this->entryCount = entryCount;
    }

    MetalDescriptorSet::~MetalDescriptorSet() {
        for (auto &buffer : indicesToBuffers) {
            buffer.second.buffer->release();
        }
    }

    void MetalDescriptorSet::setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) {
        if (buffer == nullptr) {
            return;
        }

        uint32_t descriptorIndexClamped = std::min(descriptorIndex, descriptorTypeMaxIndex);
        RenderDescriptorRangeType descriptorType = descriptorTypes[descriptorIndexClamped];
        switch (descriptorType) {
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER: {
                const MetalBufferFormattedView *interfaceBufferFormattedView = static_cast<const MetalBufferFormattedView *>(bufferFormattedView);
                indicesToBufferFormattedViews[descriptorIndex] = interfaceBufferFormattedView->texture;
                break;
            }
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER: {
                const MetalBuffer *interfaceBuffer = static_cast<const MetalBuffer*>(buffer);
                uint32_t offset = 0;

                if (bufferStructuredView != nullptr) {
                    offset = bufferStructuredView->firstElement * bufferStructuredView->structureByteStride;
                }

                indicesToBuffers[descriptorIndex] = MetalBufferBinding(interfaceBuffer->mtl, offset);
                break;
            }
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            case RenderDescriptorRangeType::SAMPLER:
            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                assert(false && "Incompatible descriptor type.");
                break;
            default:
                assert(false && "Unknown descriptor type.");
                break;
        }
    }

    void MetalDescriptorSet::setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) {
        const MetalTexture *interfaceTexture = static_cast<const MetalTexture *>(texture);
        const auto nativeResource = (interfaceTexture != nullptr) ? interfaceTexture->mtl : nullptr;
        uint32_t descriptorIndexClamped = std::min(descriptorIndex, descriptorTypeMaxIndex);
        RenderDescriptorRangeType descriptorType = descriptorTypes[descriptorIndexClamped];
        switch (descriptorType) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE: {
                if (textureView != nullptr) {
                    const MetalTextureView *interfaceTextureView = static_cast<const MetalTextureView *>(textureView);
                    indicesToTextures[descriptorIndex] = interfaceTextureView->texture;
                } else {
                    indicesToTextures[descriptorIndex] = interfaceTexture->mtl;
                }
                break;
            }
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::SAMPLER:
            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                assert(false && "Incompatible descriptor type.");
                break;
            default:
                assert(false && "Unknown descriptor type.");
                break;
        }
    }

    void MetalDescriptorSet::setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) {
        if (sampler != nullptr) {
            const MetalSampler *interfaceSampler = static_cast<const MetalSampler *>(sampler);
            indicesToSamplers[descriptorIndex] = interfaceSampler->state;
        }
    }

    void MetalDescriptorSet::setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) {
        // TODO: Unimplemented.
    }

    // MetalDrawable

    MetalDrawable::MetalDrawable(MetalDevice* device, MetalPool* pool, const RenderTextureDesc& desc) {
        assert(false && "MetalDrawable cannot be constructed directly from device - use fromDrawable() instead");
    }

    MetalDrawable::~MetalDrawable() {
        mtl->release();
    }

    std::unique_ptr<RenderTextureView> MetalDrawable::createTextureView(const RenderTextureViewDesc& desc) {
        assert(false && "Drawables don't support texture views");
    }

    void MetalDrawable::setName(const std::string &name) {
        mtl->texture()->setLabel(NS::String::string(name.c_str(), NS::UTF8StringEncoding));
    }

    // MetalSwapChain

    MetalSwapChain::MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) {
        this->layer = static_cast<CA::MetalLayer*>(renderWindow.view);
        layer->setDevice(commandQueue->device->mtl);
        layer->setPixelFormat(metal::mapPixelFormat(format));

        this->commandQueue = commandQueue;

        // Metal supports a maximum of 3 drawables.
        this->drawables.resize(MAX_DRAWABLES);

        this->renderWindow = renderWindow;
        getWindowSize(width, height);

        // set each of the drawable to have desc.flags = RenderTextureFlag::RENDER_TARGET;
        for (uint32_t i = 0; i < MAX_DRAWABLES; i++) {
            auto& drawable = this->drawables[i];
            drawable.desc.width = width;
            drawable.desc.height = height;
            drawable.desc.format = format;
            drawable.desc.flags = RenderTextureFlag::RENDER_TARGET;
        }
    }

    MetalSwapChain::~MetalSwapChain() {
        layer->release();
        delete commandQueue;
    }

    bool MetalSwapChain::present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) {
        assert(layer != nullptr && "Cannot present without a valid layer.");
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        auto& drawable = drawables[textureIndex];
        assert(drawable.mtl != nullptr && "Cannot present without a valid drawable.");

        // Create a new command buffer just for presenting
        auto presentBuffer = commandQueue->mtl->commandBuffer();
        presentBuffer->setLabel(MTLSTR("PresentCommandBuffer"));

        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(waitSemaphores[i]);
            presentBuffer->encodeWait(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue++);
        }

        presentBuffer->presentDrawable(drawable.mtl);
        presentBuffer->addCompletedHandler([waitSemaphoreCount, waitSemaphores, this](MTL::CommandBuffer* cmdBuffer) {
            dispatch_semaphore_signal(commandQueue->device->renderInterface->drawables_semaphore);
            currentAvailableDrawableIndex = (currentAvailableDrawableIndex + 1) % MAX_DRAWABLES;
        });
        presentBuffer->commit();

        drawable.mtl->release();
        releasePool->release();

        return true;
    }

    bool MetalSwapChain::resize() {
        getWindowSize(width, height);

        if ((width == 0) || (height == 0)) {
            return false;
        }

        auto drawableSize = CGSizeMake(width, height);
        auto current = layer->drawableSize();
        if (!CGSizeEqualToSize(current, drawableSize)) {
            layer->setDrawableSize(drawableSize);
        }

        return true;
    }

    bool MetalSwapChain::needsResize() const {
        uint32_t windowWidth, windowHeight;
        getWindowSize(windowWidth, windowHeight);
        return (layer == nullptr) || (width != windowWidth) || (height != windowHeight);
    }

    void MetalSwapChain::setVsyncEnabled(bool vsyncEnabled) {
        layer->setDisplaySyncEnabled(vsyncEnabled);
    }

    bool MetalSwapChain::isVsyncEnabled() const {
        return layer->displaySyncEnabled();
    }

    uint32_t MetalSwapChain::getWidth() const {
        return width;
    }

    uint32_t MetalSwapChain::getHeight() const {
        return height;
    }

    RenderTexture *MetalSwapChain::getTexture(uint32_t textureIndex) {
        return &drawables[textureIndex];
    }

    bool MetalSwapChain::acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
        assert(signalSemaphore != nullptr);
        assert(textureIndex != nullptr);
        assert(*textureIndex < MAX_DRAWABLES);

        dispatch_semaphore_wait(commandQueue->device->renderInterface->drawables_semaphore, DISPATCH_TIME_FOREVER);
        
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        // Create a command buffer just to encode the signal
        auto acquireBuffer = commandQueue->mtl->commandBuffer();
        acquireBuffer->setLabel(MTLSTR("AcquireTextureCommandBuffer"));
        MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(signalSemaphore);
        acquireBuffer->encodeSignalEvent(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue);
        acquireBuffer->commit();

        auto nextDrawable = layer->nextDrawable();
        if (nextDrawable == nullptr) {
            fprintf(stderr, "No more drawables available for rendering.\n");
            return false;
        }

        // Set the texture index and drawable data
        *textureIndex = currentAvailableDrawableIndex;
        auto& drawable = drawables[currentAvailableDrawableIndex];
        drawable.desc.width = width;
        drawable.desc.height = height;
        drawable.desc.flags = RenderTextureFlag::RENDER_TARGET;
        drawable.desc.format = metal::mapRenderFormat(nextDrawable->texture()->pixelFormat());
        drawable.mtl = nextDrawable;
        
        drawable.mtl->retain();
        releasePool->release();

        return true;
    }

    uint32_t MetalSwapChain::getTextureCount() const {
        return MAX_DRAWABLES;
    }

    RenderWindow MetalSwapChain::getWindow() const {
        return renderWindow;
    }

    bool MetalSwapChain::isEmpty() const {
        return (layer == nullptr) || (width == 0) || (height == 0);
    }

    uint32_t MetalSwapChain::getRefreshRate() const {
        return GetWindowRefreshRate(renderWindow.window);
    }

    void MetalSwapChain::getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const {
        CocoaWindowAttributes attributes;
        GetWindowAttributes(renderWindow.window, &attributes);
        dstWidth = attributes.width;
        dstHeight = attributes.height;
    }

    // MetalFramebuffer

    MetalFramebuffer::MetalFramebuffer(MetalDevice *device, const RenderFramebufferDesc &desc) {
        assert(device != nullptr);
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        colorAttachments.reserve(desc.colorAttachmentsCount);
        depthAttachmentReadOnly = desc.depthAttachmentReadOnly;

        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            const auto *colorAttachment = static_cast<const MetalTexture *>(desc.colorAttachments[i]);
            assert((colorAttachment->desc.flags & RenderTextureFlag::RENDER_TARGET) && "Color attachment must be a render target.");
            colorAttachments.emplace_back(colorAttachment);

            if (i == 0) {
                width = colorAttachment->desc.width;
                height = colorAttachment->desc.height;
            }
        }

        if (desc.depthAttachment != nullptr) {
            depthAttachment = static_cast<const MetalTexture *>(desc.depthAttachment);
            assert((depthAttachment->desc.flags & RenderTextureFlag::DEPTH_TARGET) && "Depth attachment must be a depth target.");

            if (desc.colorAttachmentsCount == 0) {
                width = depthAttachment->desc.width;
                height = depthAttachment->desc.height;
            }
        }

        releasePool->release();
    }

    MetalFramebuffer::~MetalFramebuffer() {
        colorAttachments.clear();
    }

    uint32_t MetalFramebuffer::getWidth() const {
        return width;
    }

    uint32_t MetalFramebuffer::getHeight() const {
        return height;
    }

    // MetalCommandList

    MetalCommandList::MetalCommandList(MetalCommandQueue *queue, RenderCommandListType type) {
        assert(type != RenderCommandListType::UNKNOWN);

        this->device = queue->device;
        this->type = type;
        this->queue = queue;
    }

    MetalCommandList::~MetalCommandList() {
        mtl->release();
        indexBuffer->release();

        for (auto buffer : vertexBuffers) {
            buffer->release();
        }
    }

    void MetalCommandList::begin() {
        assert(mtl == nullptr);
        mtl = queue->mtl->commandBuffer();
    }

    void MetalCommandList::end() {
        endActiveRenderEncoder();
        endActiveResolveTextureComputeEncoder();
        endActiveBlitEncoder();
        endActiveComputeEncoder();

        targetFramebuffer = nullptr;
    }

    void MetalCommandList::commit() {
        mtl->commit();
        mtl->release();
        mtl = nullptr;
    }

    void MetalCommandList::configureRenderDescriptor(MTL::RenderPassDescriptor *renderDescriptor) {
        assert(targetFramebuffer != nullptr && "Cannot encode render commands without a target framebuffer");

        for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); i++) {
            auto colorAttachment = renderDescriptor->colorAttachments()->object(i);
            colorAttachment->setTexture(targetFramebuffer->colorAttachments[i]->getTexture());
            colorAttachment->setLoadAction(MTL::LoadActionLoad);
            colorAttachment->setStoreAction(MTL::StoreActionStore);
        }

        if (targetFramebuffer->depthAttachment != nullptr) {
            auto depthAttachment = renderDescriptor->depthAttachment();
            depthAttachment->setTexture(targetFramebuffer->depthAttachment->mtl);
            depthAttachment->setLoadAction(MTL::LoadActionLoad);
            depthAttachment->setStoreAction(MTL::StoreActionStore);
        }

        if (activeRenderState && activeRenderState->sampleCount > 1) {
            renderDescriptor->setSamplePositions(activeRenderState->samplePositions, activeRenderState->sampleCount);
        }
    }

    void MetalCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        // TODO: Ignore for now, Metal should handle most of this itself.
    }

    void MetalCommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
        checkActiveComputeEncoder();
        assert(activeComputeEncoder != nullptr && "Cannot encode dispatch on nullptr MTLComputeCommandEncoder!");


        MTL::Size threadGroupCount = { threadGroupCountX, threadGroupCountY, threadGroupCountZ };
        MTL::Size threadGroupSize = { activeComputeState->threadGroupSizeX, activeComputeState->threadGroupSizeY, activeComputeState->threadGroupSizeZ };
        activeComputeEncoder->dispatchThreadgroups(threadGroupCount, threadGroupSize);
    }

    void MetalCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        // TODO: Support Metal RT
    }

    void MetalCommandList::setupClearTransform(ClearTransform& transform) {
        // Convert from screen space to NDC space
        float sx = 2.0f / float(targetFramebuffer->width);
        float sy = -2.0f / float(targetFramebuffer->height);
        float tx = -1.0f;
        float ty = 1.0f;

        transform.ndcTransform = simd::float4x4(
            (simd::float4){ sx, 0.0f, 0.0f, 0.0f },
            (simd::float4){ 0.0f, sy, 0.0f, 0.0f },
            (simd::float4){ 0.0f, 0.0f, 1.0f, 0.0f },
            (simd::float4){ tx, ty, 0.0f, 1.0f }
        );
    }

    MetalCommandList::ClearRect prepareClearRects[MAX_CLEAR_RECTS];
    size_t MetalCommandList::prepareClearRectCount(const RenderRect* clearRects, size_t clearRectCount) {
        if (clearRects == nullptr || clearRectCount == 0) {
            prepareClearRects[0] = { { 0, 0 }, { float(targetFramebuffer->width), float(targetFramebuffer->height) } };
            return 1;
        }

        size_t validRectCount = 0;
        for (size_t i = 0; i < clearRectCount && validRectCount < MAX_CLEAR_RECTS; i++) {
            const auto& r = clearRects[i];
            if (!r.isEmpty()) {
                prepareClearRects[validRectCount++] = { { float(r.left), float(r.top) }, { float(r.right - r.left), float(r.bottom - r.top) } };
            }
        }

        return validRectCount;
    }

    void MetalCommandList::processPendingClears() {
        if (pendingColorClearCount == 0 && pendingDepthClearCount == 0) {
            return;
        }

        checkActiveRenderEncoder();

        // Store state cache
        auto previousCache = stateCache;

        // Calculate transform matrix once for all clears
        ClearTransform transform;
        setupClearTransform(transform);

        // Common render state setup (done once)
        auto setupCommonState = [&]() {
            activeRenderEncoder->setViewport({ 0, 0, float(targetFramebuffer->width), float(targetFramebuffer->height), 0.0f, 1.0f });
            activeRenderEncoder->setScissorRect({ 0, 0, targetFramebuffer->width, targetFramebuffer->height });
            activeRenderEncoder->setTriangleFillMode(MTL::TriangleFillModeFill);
            activeRenderEncoder->setCullMode(MTL::CullModeNone);
            activeRenderEncoder->setDepthBias(0.0f, 0.0f, 0.0f);
            activeRenderEncoder->setVertexBytes(&transform, sizeof(transform), 1);
        };

        // Process color clears
        int lastColorAttachmentIndex = -1;
        for (size_t i = 0; i < pendingColorClearCount; i++) {
            const auto& clearOp = pendingColorClears[i];

            activeRenderEncoder->pushDebugGroup(MTLSTR("ColorClear"));

            if (lastColorAttachmentIndex != clearOp.attachmentIndex) {
                MTL::RenderPipelineDescriptor* pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
                pipelineDesc->setVertexFunction(device->renderInterface->clearVertexFunction);
                pipelineDesc->setFragmentFunction(device->renderInterface->clearColorFunction);
                pipelineDesc->setRasterSampleCount(targetFramebuffer->colorAttachments[clearOp.attachmentIndex]->desc.multisampling.sampleCount);

                auto pipelineColorAttachment = pipelineDesc->colorAttachments()->object(clearOp.attachmentIndex);
                pipelineColorAttachment->setPixelFormat(targetFramebuffer->colorAttachments[clearOp.attachmentIndex]->getTexture()->pixelFormat());
                pipelineColorAttachment->setBlendingEnabled(false);

                auto pipelineState = device->renderInterface->getOrCreateClearRenderPipelineState(pipelineDesc);
                activeRenderEncoder->setRenderPipelineState(pipelineState);

                // We only set these properties once per processing of all clears
                if (lastColorAttachmentIndex == -1) setupCommonState();

                lastColorAttachmentIndex = clearOp.attachmentIndex;
                pipelineDesc->release();
            }

            auto rectCount = prepareClearRectCount(clearOp.clearRectCount > 0 ? clearOp.clearRects : nullptr, clearOp.clearRectCount);
            activeRenderEncoder->setVertexBytes(prepareClearRects, sizeof(ClearRect) * rectCount, 0);

            // Use stack for clear colors too since we know the max size
            simd::float4 clearColors[MAX_CLEAR_RECTS];
            for (size_t j = 0; j < rectCount; j++) {
                clearColors[j] = (simd::float4){ clearOp.colorValue.r, clearOp.colorValue.g, clearOp.colorValue.b, clearOp.colorValue.a };
            }
            activeRenderEncoder->setFragmentBytes(clearColors, mem::alignUp(sizeof(simd::float4) * rectCount), 0);

            activeRenderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, 0, 4, rectCount);

            activeRenderEncoder->popDebugGroup();
        }

        // Process depth clears
        bool isFirstDepthClear = true;
        for (size_t i = 0; i < pendingDepthClearCount; i++) {
            if (targetFramebuffer->depthAttachment == nullptr) {
                break;
            }

            const auto& clearOp = pendingDepthClears[i];
            activeRenderEncoder->pushDebugGroup(MTLSTR("DepthClear"));

            if (isFirstDepthClear) {
                MTL::RenderPipelineDescriptor* pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
                pipelineDesc->setVertexFunction(device->renderInterface->clearVertexFunction);
                pipelineDesc->setFragmentFunction(device->renderInterface->clearDepthFunction);
                pipelineDesc->setDepthAttachmentPixelFormat(targetFramebuffer->depthAttachment->mtl->pixelFormat());
                pipelineDesc->setRasterSampleCount(targetFramebuffer->depthAttachment->desc.multisampling.sampleCount);

                auto pipelineState = device->renderInterface->getOrCreateClearRenderPipelineState(pipelineDesc);
                activeRenderEncoder->setRenderPipelineState(pipelineState);
                activeRenderEncoder->setDepthStencilState(device->renderInterface->clearDepthStencilState);

                // Don't set these if we did some color clears, as those woeuld have already set them
                if (pendingColorClearCount == 0) setupCommonState();

                isFirstDepthClear = false;
                pipelineDesc->release();
            }

            auto rectCount = prepareClearRectCount(clearOp.clearRectCount > 0 ? clearOp.clearRects : nullptr, clearOp.clearRectCount);
            activeRenderEncoder->setVertexBytes(prepareClearRects, sizeof(ClearRect) * rectCount, 0);

            float clearDepths[MAX_CLEAR_RECTS];
            for (size_t j = 0; j < rectCount; j++) {
                clearDepths[j] = clearOp.depthValue;
            }
            activeRenderEncoder->setFragmentBytes(clearDepths, sizeof(float) * rectCount, 0);

            activeRenderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, 0, 4, rectCount);

            activeRenderEncoder->popDebugGroup();
        }

        pendingColorClearCount = 0;
        pendingDepthClearCount = 0;

        // Restore previous state if we had one
        stateCache = previousCache;
        dirtyGraphicsState.setAll();
    }

    void MetalCommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
        processPendingClears();
        checkActiveRenderEncoder();
        checkForUpdatesInGraphicsState();

        activeRenderEncoder->drawPrimitives(currentPrimitiveType, startVertexLocation, vertexCountPerInstance, instanceCount, startInstanceLocation);
    }

    void MetalCommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
        processPendingClears();
        checkActiveRenderEncoder();
        checkForUpdatesInGraphicsState();

        activeRenderEncoder->drawIndexedPrimitives(currentPrimitiveType, indexCountPerInstance, currentIndexType, indexBuffer, startIndexLocation, instanceCount, baseVertexLocation, startInstanceLocation);
    }

    void MetalCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const auto *interfacePipeline = static_cast<const MetalPipeline *>(pipeline);
        switch (interfacePipeline->type) {
            case MetalPipeline::Type::Compute: {
                const auto *computePipeline = static_cast<const MetalComputePipeline *>(interfacePipeline);
                if (activeComputeState != computePipeline->state) {
                    activeComputeState = computePipeline->state;
                    dirtyComputeState.pipelineState = 1;
                }
                break;
            }
            case MetalPipeline::Type::Graphics: {
                const auto *graphicsPipeline = static_cast<const MetalGraphicsPipeline *>(interfacePipeline);
                if (activeRenderState) {
                    if (activeRenderState->samplePositions != graphicsPipeline->state->samplePositions || activeRenderState->sampleCount != graphicsPipeline->state->sampleCount) {
                        endActiveRenderEncoder();
                    }
                }

                // TODO: Can we be more granular here?
                if (activeRenderState != graphicsPipeline->state) {
                    activeRenderState = graphicsPipeline->state;
                    dirtyGraphicsState.pipelineState = 1;
                }
                break;
            }
            default:
                assert(false && "Unknown pipeline type.");
                break;
        }
    }

    void MetalCommandList::setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const auto oldLayout = activeComputePipelineLayout;
        activeComputePipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeComputePipelineLayout) {
            // Clear descriptor set bindings since they're no longer valid with the new layout
            indicesToComputeDescriptorSets.clear();

            // Clear push constants since they might have different layouts/ranges
            pushConstants.clear();
            stateCache.lastPushConstants.clear();

            // Mark compute states as dirty that need to be rebound
            dirtyComputeState.descriptorSets = 1;
            dirtyComputeState.pushConstants = 1;
        }
    }

    void MetalCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data) {
        assert(activeComputePipelineLayout != nullptr);
        assert(rangeIndex < activeComputePipelineLayout->pushConstantRanges.size());

        const RenderPushConstantRange &range = activeComputePipelineLayout->pushConstantRanges[rangeIndex];
        pushConstants.resize(activeComputePipelineLayout->pushConstantRanges.size());
        pushConstants[rangeIndex].data.resize(range.size);
        memcpy(pushConstants[rangeIndex].data.data(), data, range.size);
        pushConstants[rangeIndex].binding = range.binding;
        pushConstants[rangeIndex].set = range.set;
        pushConstants[rangeIndex].offset = range.offset;
        pushConstants[rangeIndex].size = range.size;
        pushConstants[rangeIndex].stageFlags = range.stageFlags;

        dirtyComputeState.pushConstants = 1;
    }

    void MetalCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(descriptorSet, setIndex, true);
    }

    void MetalCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const auto oldLayout = activeGraphicsPipelineLayout;
        activeGraphicsPipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeGraphicsPipelineLayout) {
            // Clear descriptor set bindings since they're no longer valid with the new layout
            indicesToRenderDescriptorSets.clear();

            // Clear push constants since they might have different layouts/ranges
            pushConstants.clear();
            stateCache.lastPushConstants.clear();

            // Mark graphics states as dirty that need to be rebound
            dirtyGraphicsState.descriptorSets = 1;
            dirtyGraphicsState.pushConstants = 1;
        }
    }

    void MetalCommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void *data) {
        assert(activeGraphicsPipelineLayout != nullptr);
        assert(rangeIndex < activeGraphicsPipelineLayout->pushConstantRanges.size());

        const RenderPushConstantRange &range = activeGraphicsPipelineLayout->pushConstantRanges[rangeIndex];
        pushConstants.resize(activeGraphicsPipelineLayout->pushConstantRanges.size());
        pushConstants[rangeIndex].data.resize(range.size);
        memcpy(pushConstants[rangeIndex].data.data(), data, range.size);
        pushConstants[rangeIndex].binding = range.binding;
        pushConstants[rangeIndex].set = range.set;
        pushConstants[rangeIndex].offset = range.offset;
        pushConstants[rangeIndex].size = range.size;
        pushConstants[rangeIndex].stageFlags = range.stageFlags;

        dirtyGraphicsState.pushConstants = 1;
    }

    void MetalCommandList::setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(descriptorSet, setIndex, false);
    }

    void MetalCommandList::setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        // TODO: Metal RT
    }

    void MetalCommandList::setRaytracingPushConstants(uint32_t rangeIndex, const void *data) {
        // TODO: Metal RT
    }

    void MetalCommandList::setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        // TODO: Metal RT
        // setDescriptorSet();
    }

    void MetalCommandList::setIndexBuffer(const RenderIndexBufferView *view) {
        if (view != nullptr) {
            const auto *interfaceBuffer = static_cast<const MetalBuffer *>(view->buffer.ref);
            indexBuffer = interfaceBuffer->mtl;
        }
    }

    void MetalCommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
        if ((views != nullptr) && (viewCount > 0)) {
            assert(inputSlots != nullptr);

            bool needsUpdate = false;

            // First time binding or different count requires full update
            if (this->viewCount != viewCount) {
                needsUpdate = true;
            }

            // Resize our storage if needed
            vertexBuffers.resize(viewCount);
            vertexBufferOffsets.resize(viewCount);
            vertexBufferIndices.resize(viewCount);

            // Check for changes in bindings
            for (uint32_t i = 0; i < viewCount; i++) {
                const MetalBuffer* interfaceBuffer = static_cast<const MetalBuffer*>(views[i].buffer.ref);
                uint64_t newOffset = views[i].buffer.offset;
                uint32_t newIndex = startSlot + i;

                // Check if this binding differs from current state
                needsUpdate = i >= stateCache.lastVertexBuffers.size() || interfaceBuffer->mtl != stateCache.lastVertexBuffers[i] || newOffset != stateCache.lastVertexBufferOffsets[i] || newIndex != vertexBufferIndices[i];

                vertexBuffers[i] = interfaceBuffer->mtl;
                vertexBufferOffsets[i] = newOffset;
                vertexBufferIndices[i] = newIndex;
            }

            if (needsUpdate) {
                this->viewCount = viewCount;
                dirtyGraphicsState.vertexBuffers = 1;
            }
        }
    }

    void MetalCommandList::setViewports(const RenderViewport *viewports, uint32_t count) {
        viewportVector.resize(count);

        for (uint32_t i = 0; i < count; i++) {
            MTL::Viewport viewport { viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth };
            viewportVector[i] = viewport;
        }

        // Since viewports are set at the encoder level, we mark it as dirty so it'll be updated on next active encoder check
        if (viewportVector != stateCache.lastViewports) {
            dirtyGraphicsState.viewports = 1;
        }
    }

    void MetalCommandList::setScissors(const RenderRect *scissorRects, uint32_t count) {
        scissorVector.resize(count);

        for (uint32_t i = 0; i < count; i++) {
            MTL::ScissorRect scissor {
                static_cast<NS::UInteger>(scissorRects[i].left),
                static_cast<NS::UInteger>(scissorRects[i].top),
                static_cast<NS::UInteger>(scissorRects[i].right - scissorRects[i].left),
                static_cast<NS::UInteger>(scissorRects[i].bottom - scissorRects[i].top)
            };
            scissorVector[i] = scissor;
        }

        // Since scissors are set at the encoder level, we mark it as dirty so it'll be updated on next active encoder check
        if (scissorVector != stateCache.lastScissors) {
            dirtyGraphicsState.scissors = 1;
        }
    }

    void MetalCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {
        // If we're changing framebuffers, we need to end current encoder and reset state
        if (targetFramebuffer != framebuffer) {
            endOtherEncoders(EncoderType::Render);
            endActiveRenderEncoder();

            targetFramebuffer = static_cast<const MetalFramebuffer*>(framebuffer);

            // Mark all state as needing update with the new encoder
            if (targetFramebuffer != nullptr) {
                dirtyGraphicsState.setAll();
            }
        }
    }

    void MetalCommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(attachmentIndex < targetFramebuffer->colorAttachments.size());
        assert(pendingColorClearCount < MAX_PENDING_CLEARS && "Too many pending color clears");
        assert((!clearRects || clearRectsCount <= MAX_CLEAR_RECTS) && "Too many clear rects");

        PendingColorClear& clearOp = pendingColorClears[pendingColorClearCount];
        clearOp.attachmentIndex = attachmentIndex;
        clearOp.colorValue = colorValue;
        if (clearRects && clearRectsCount > 0) {
            for (uint32_t i = 0; i < clearRectsCount; i++) {
                clearOp.clearRects[i] = clearRects[i];
            }
            clearOp.clearRectCount = clearRectsCount;
        }

        pendingColorClearCount++;
    }

    void MetalCommandList::clearDepth(bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(targetFramebuffer->depthAttachment != nullptr);
        assert(pendingDepthClearCount < MAX_PENDING_CLEARS && "Too many pending color clears");
        assert((!clearRects || clearRectsCount <= MAX_CLEAR_RECTS) && "Too many clear rects");

        if (clearDepth) {
            PendingDepthClear& clearOp = pendingDepthClears[pendingDepthClearCount];
            clearOp.depthValue = depthValue;
            if (clearRects && clearRectsCount > 0) {
                for (uint32_t i = 0; i < clearRectsCount; i++) {
                    clearOp.clearRects[i] = clearRects[i];
                }
                clearOp.clearRectCount = clearRectsCount;
            }

            pendingDepthClearCount++;
        }
    }

    void MetalCommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();

        const auto interfaceDstBuffer = static_cast<const MetalBuffer *>(dstBuffer.ref);
        const auto interfaceSrcBuffer = static_cast<const MetalBuffer *>(srcBuffer.ref);

        activeBlitEncoder->copyFromBuffer(interfaceSrcBuffer->mtl, srcBuffer.offset, interfaceDstBuffer->mtl, dstBuffer.offset, size);
    }

    void MetalCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) {
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();

        const auto dstTexture = static_cast<const MetalTexture *>(dstLocation.texture);
        const auto srcTexture = static_cast<const MetalTexture *>(srcLocation.texture);
        const auto dstBuffer = static_cast<const MetalBuffer *>(dstLocation.buffer);
        const auto srcBuffer = static_cast<const MetalBuffer *>(srcLocation.buffer);

        if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE && srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            // Calculate block size based on destination texture format
            uint32_t blockWidth = RenderFormatBlockWidth(dstTexture->desc.format);

            // Use actual dimensions for the copy size
            MTL::Size size = { srcLocation.placedFootprint.width, srcLocation.placedFootprint.height, srcLocation.placedFootprint.depth};

            // Calculate padded row width for buffer layout
            uint32_t paddedRowWidth = ((srcLocation.placedFootprint.rowWidth + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t bytesPerRow = paddedRowWidth * RenderFormatSize(dstTexture->desc.format);

            // Verify alignment requirements
            assert((srcLocation.placedFootprint.offset % 256) == 0 && "Buffer offset must be aligned");
            assert((bytesPerRow % 256) == 0 && "Bytes per row must be aligned");

            // Calculate bytes per image using the padded height
            uint32_t paddedHeight = ((srcLocation.placedFootprint.height + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t bytesPerImage = bytesPerRow * paddedHeight;

            MTL::Origin dstOrigin = { dstX, dstY, dstZ };

            activeBlitEncoder->pushDebugGroup(MTLSTR("CopyTextureRegion"));
            activeBlitEncoder->copyFromBuffer(
                srcBuffer->mtl,
                srcLocation.placedFootprint.offset,
                bytesPerRow,
                bytesPerImage,
                size,
                dstTexture->mtl,
                dstLocation.subresource.index,
                0,  // slice
                dstOrigin
            );
            activeBlitEncoder->popDebugGroup();
        } else {
            assert(dstTexture != nullptr);
            assert(srcTexture != nullptr);

            MTL::Origin srcOrigin;
            MTL::Size size;

            if (srcBox != nullptr) {
                srcOrigin = { NS::UInteger(srcBox->left), NS::UInteger(srcBox->top), NS::UInteger(srcBox->front) };
                MTL::Size size = { NS::UInteger(srcBox->right - srcBox->left), NS::UInteger(srcBox->bottom - srcBox->top), NS::UInteger(srcBox->back - srcBox->front) };
            } else {
                srcOrigin = { 0, 0, 0 };
                MTL::Size size = { srcTexture->desc.width, srcTexture->desc.height, srcTexture->desc.depth };
            }

            MTL::Origin dstOrigin = { dstX, dstY, dstZ };

            activeBlitEncoder->copyFromTexture(
                srcTexture->mtl,                  // source texture
                srcLocation.subresource.index,    // source mipmap level
                0,                                // source slice (baseArrayLayer)
                srcOrigin,                        // source origin
                size,                             // copy size
                dstTexture->mtl,                 // destination texture
                dstLocation.subresource.index,   // destination mipmap level
                0,                               // destination slice (baseArrayLayer)
                dstOrigin                        // destination origin
            );
          }
    }

    void MetalCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();

        const auto dst = static_cast<const MetalBuffer *>(dstBuffer);
        const auto src = static_cast<const MetalBuffer *>(srcBuffer);

        activeBlitEncoder->pushDebugGroup(MTLSTR("CopyBuffer"));
        activeBlitEncoder->copyFromBuffer(src->mtl, 0, dst->mtl, 0, dst->desc.size);
        activeBlitEncoder->popDebugGroup();
    }

    void MetalCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        endOtherEncoders(EncoderType::Blit);
        checkActiveBlitEncoder();

        const auto dst = static_cast<const MetalTexture *>(dstTexture);
        const auto src = static_cast<const MetalTexture *>(srcTexture);

        activeBlitEncoder->copyFromTexture(src->mtl, dst->mtl);
    }

    void MetalCommandList::resolveTexture(const RT64::RenderTexture *dstTexture, const RT64::RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        const MetalTexture *dst = static_cast<const MetalTexture *>(dstTexture);
        const MetalTexture *src = static_cast<const MetalTexture *>(srcTexture);

        // For full texture resolves, use the more efficient render pass resolve
        processPendingClears();
        endOtherEncoders(EncoderType::Render);
        endActiveRenderEncoder();

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        MTL::RenderPassDescriptor *renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto colorAttachment = renderPassDescriptor->colorAttachments()->object(0);

        colorAttachment->setTexture(src->mtl);
        colorAttachment->setResolveTexture(dst->mtl);
        colorAttachment->setLoadAction(MTL::LoadActionLoad);
        colorAttachment->setStoreAction(MTL::StoreActionMultisampleResolve);

        auto encoder = mtl->renderCommandEncoder(renderPassDescriptor);
        encoder->setLabel(MTLSTR("Resolve Texture Encoder"));
        encoder->endEncoding();

        pool->release();
    }

    void MetalCommandList::resolveTextureRegion(const RT64::RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RT64::RenderTexture *srcTexture, const RT64::RenderRect *srcRect) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        endOtherEncoders(EncoderType::Resolve);
        checkActiveResolveTextureComputeEncoder();

        const MetalTexture *dst = static_cast<const MetalTexture *>(dstTexture);
        const MetalTexture *src = static_cast<const MetalTexture *>(srcTexture);

        // Calculate source region
        uint32_t srcX = 0;
        uint32_t srcY = 0;
        uint32_t width = src->mtl->width();
        uint32_t height = src->mtl->height();

        if (srcRect != nullptr) {
            srcX = srcRect->left;
            srcY = srcRect->top;
            width = srcRect->right - srcRect->left;
            height = srcRect->bottom - srcRect->top;
        }

        // Setup resolve parameters
        struct ResolveParams {
            uint32_t dstOffsetX;
            uint32_t dstOffsetY;
            uint32_t srcOffsetX;
            uint32_t srcOffsetY;
            uint32_t resolveSizeX;
            uint32_t resolveSizeY;
        } params = {
            dstX, dstY,
            srcX, srcY,
            width, height
        };

        activeResolveComputeEncoder->setTexture(src->mtl, 0);
        activeResolveComputeEncoder->setTexture(dst->mtl, 1);
        activeResolveComputeEncoder->setBytes(&params, sizeof(params), 0);

        MTL::Size threadGroupSize = { 8, 8, 1 };
        auto groupSizeX = (width + threadGroupSize.width - 1) / threadGroupSize.width;
        auto groupSizeY = (height + threadGroupSize.height - 1) / threadGroupSize.height;
        MTL::Size gridSize = { groupSizeX, groupSizeY, 1 };
        activeResolveComputeEncoder->dispatchThreadgroups(gridSize, threadGroupSize);
    }

    void MetalCommandList::buildBottomLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, const RT64::RenderBottomLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::buildTopLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, RT64::RenderBufferReference instancesBuffer, const RT64::RenderTopLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::endOtherEncoders(EncoderType type) {
        if (type != EncoderType::Render) {
            endActiveRenderEncoder();
        }
        if (type != EncoderType::Compute) {
            endActiveComputeEncoder();
        }
        if (type != EncoderType::Blit) {
            endActiveBlitEncoder();
        }
        if (type != EncoderType::Resolve) {
            endActiveResolveTextureComputeEncoder();
        }
    }

    void MetalCommandList::checkActiveComputeEncoder() {
        // before switching pipelines see if we have any pending clears to process
        processPendingClears();
        endOtherEncoders(EncoderType::Compute);

        if (activeComputeEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::ComputePassDescriptor *computeDescriptor = MTL::ComputePassDescriptor::alloc()->init();
            activeComputeEncoder = mtl->computeCommandEncoder(computeDescriptor);
            activeComputeEncoder->setLabel(MTLSTR("Compute Encoder"));

            computeDescriptor->release();
            activeComputeEncoder->retain();
            releasePool->release();

            dirtyComputeState.setAll();
        }

        if (dirtyComputeState.pipelineState) {
            activeComputeEncoder->setComputePipelineState(activeComputeState->pipelineState);
            dirtyComputeState.pipelineState = 0;
        }

        if (dirtyComputeState.descriptorSets) {
            bindDescriptorSetLayout(activeComputePipelineLayout, activeComputeEncoder, indicesToComputeDescriptorSets, true);
            dirtyComputeState.descriptorSets = 0;
        }

        if (dirtyComputeState.pushConstants) {
            for (const auto& pushConstant : pushConstants) {
                if (pushConstant.stageFlags & RenderShaderStageFlag::COMPUTE) {
                    activeComputeEncoder->setBytes(pushConstant.data.data(), pushConstant.size, MAX_DESCRIPTOR_SET_COUNT + pushConstant.set);
                }
            }
            stateCache.lastPushConstants = pushConstants;
            dirtyComputeState.pushConstants = 0;
        }
    }

    void MetalCommandList::endActiveComputeEncoder() {
        if (activeComputeEncoder != nullptr) {
            activeComputeEncoder->endEncoding();
            activeComputeEncoder->release();
            activeComputeEncoder = nullptr;

            // Mark all state as needing rebind for next encoder
            dirtyComputeState.setAll();

            // Clear state cache for compute
            stateCache.lastPushConstants.clear();
        }
    }

    void MetalCommandList::checkActiveRenderEncoder() {
        assert(targetFramebuffer != nullptr);
        endOtherEncoders(EncoderType::Render);

        if (activeRenderEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            // target frame buffer & sample positions affect the descriptor
            MTL::RenderPassDescriptor *renderDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
            configureRenderDescriptor(renderDescriptor);
            activeRenderEncoder = mtl->renderCommandEncoder(renderDescriptor);
            activeRenderEncoder->setLabel(MTLSTR("Graphics Render Encoder"));

            activeRenderEncoder->retain();
            releasePool->release();
        }
    }

    void MetalCommandList::checkForUpdatesInGraphicsState() {
        if (dirtyGraphicsState.pipelineState) {
            if (activeRenderState) {
                activeRenderEncoder->setRenderPipelineState(activeRenderState->renderPipelineState);
                activeRenderEncoder->setDepthStencilState(activeRenderState->depthStencilState);
                activeRenderEncoder->setDepthClipMode(activeRenderState->depthClipMode);
                activeRenderEncoder->setCullMode(activeRenderState->cullMode);
                activeRenderEncoder->setFrontFacingWinding(activeRenderState->winding);
                stateCache.lastPipelineState = activeRenderState->renderPipelineState;
            }
            dirtyGraphicsState.pipelineState = 0;
        }

        if (dirtyGraphicsState.viewports) {
            if (viewportVector.size() < 1) return;

            activeRenderEncoder->setViewports(viewportVector.data(), viewportVector.size());
            stateCache.lastViewports = viewportVector;
            dirtyGraphicsState.viewports = 0;
        }

        if (dirtyGraphicsState.scissors) {
            if (scissorVector.size() < 1) return;

            activeRenderEncoder->setScissorRects(scissorVector.data(), scissorVector.size());
            stateCache.lastScissors = scissorVector;
            dirtyGraphicsState.scissors = 0;
        }

        if (dirtyGraphicsState.vertexBuffers) {
            for (uint32_t i = 0; i < viewCount; i++) {
                activeRenderEncoder->setVertexBuffer(vertexBuffers[i], vertexBufferOffsets[i], vertexBufferIndices[i]);
            }

            stateCache.lastVertexBuffers = vertexBuffers;
            stateCache.lastVertexBufferOffsets = vertexBufferOffsets;
            dirtyGraphicsState.vertexBuffers = 0;
        }

        if (dirtyGraphicsState.descriptorSets) {
            if (activeGraphicsPipelineLayout) {
                bindDescriptorSetLayout(activeGraphicsPipelineLayout, activeRenderEncoder, indicesToRenderDescriptorSets, false);
            }
            dirtyGraphicsState.descriptorSets = 0;
        }

        if (dirtyGraphicsState.pushConstants) {
            for (const auto& pushConstant : pushConstants) {
                if (pushConstant.stageFlags & RenderShaderStageFlag::VERTEX) {
                    activeRenderEncoder->setVertexBytes(pushConstant.data.data(), pushConstant.size, MAX_DESCRIPTOR_SET_COUNT + pushConstant.binding);
                }
                if (pushConstant.stageFlags & RenderShaderStageFlag::PIXEL) {
                    activeRenderEncoder->setFragmentBytes(pushConstant.data.data(), pushConstant.size, MAX_DESCRIPTOR_SET_COUNT + pushConstant.binding);
                }
            }

            stateCache.lastPushConstants = pushConstants;
            dirtyGraphicsState.pushConstants = 0;
        }
    }

    void MetalCommandList::endActiveRenderEncoder() {
        if (activeRenderEncoder != nullptr) {
            activeRenderEncoder->endEncoding();
            activeRenderEncoder->release();
            activeRenderEncoder = nullptr;

            // Mark all state as needing rebind for next encoder
            dirtyGraphicsState.setAll();

            // Clear state cache since we'll need to rebind everything
            stateCache.lastPipelineState = nullptr;
            stateCache.lastViewports.clear();
            stateCache.lastScissors.clear();
            stateCache.lastVertexBuffers.clear();
            stateCache.lastVertexBufferOffsets.clear();
            stateCache.lastPushConstants.clear();
        }
    }

    void MetalCommandList::checkActiveBlitEncoder() {
        // before switching pipelines see if we have any pending clears to process
        processPendingClears();
        endOtherEncoders(EncoderType::Blit);

        if (activeBlitEncoder == nullptr) {
            // TODO: We don't specialize this descriptor, so it could be reused.
            auto blitDescriptor = MTL::BlitPassDescriptor::alloc()->init();
            activeBlitEncoder = mtl->blitCommandEncoder(blitDescriptor);
            activeBlitEncoder->setLabel(MTLSTR("Copy Blit Encoder"));

            blitDescriptor->release();
        }
    }

    void MetalCommandList::endActiveBlitEncoder() {
        if (activeBlitEncoder != nullptr) {
            activeBlitEncoder->endEncoding();
            activeBlitEncoder->release();
            activeBlitEncoder = nullptr;
        }
    }

    void MetalCommandList::checkActiveResolveTextureComputeEncoder() {
        assert(targetFramebuffer != nullptr);

        // before switching pipelines see if we have any pending clears to process
        processPendingClears();
        endOtherEncoders(EncoderType::Resolve);

        if (activeResolveComputeEncoder == nullptr) {
            activeResolveComputeEncoder = mtl->computeCommandEncoder();
            activeResolveComputeEncoder->setLabel(MTLSTR("Resolve Texture Encoder"));
            activeResolveComputeEncoder->setComputePipelineState(device->renderInterface->resolveTexturePipelineState);
        }
    }

    void MetalCommandList::endActiveResolveTextureComputeEncoder() {
        if (activeResolveComputeEncoder != nullptr) {
            activeResolveComputeEncoder->endEncoding();
            activeResolveComputeEncoder->release();
            activeResolveComputeEncoder = nullptr;
        }
    }

    void MetalCommandList::setDescriptorSet(RenderDescriptorSet* descriptorSet, uint32_t setIndex, bool isCompute) {
        auto* interfaceDescriptorSet = static_cast<MetalDescriptorSet*>(descriptorSet);

        if (isCompute) {
            auto it = indicesToComputeDescriptorSets.find(setIndex);
            if (it == indicesToComputeDescriptorSets.end() || it->second != interfaceDescriptorSet) {
                activeComputePipelineLayout->setLayoutHandles[setIndex]->createEncoderAndBuffer();
                indicesToComputeDescriptorSets[setIndex] = interfaceDescriptorSet;
                dirtyComputeState.descriptorSets = 1;
            }
        } else {
            auto it = indicesToRenderDescriptorSets.find(setIndex);
            if (it == indicesToRenderDescriptorSets.end() || it->second != interfaceDescriptorSet) {
                activeGraphicsPipelineLayout->setLayoutHandles[setIndex]->createEncoderAndBuffer();
                indicesToRenderDescriptorSets[setIndex] = interfaceDescriptorSet;
                dirtyGraphicsState.descriptorSets = 1;
            }
        }
    }

    void MetalCommandList::bindDescriptorSetLayout(const MetalPipelineLayout* layout, MTL::CommandEncoder* encoder, const std::unordered_map<uint32_t, MetalDescriptorSet*>& descriptorSets, bool isCompute) {
        // Encode Descriptor set layouts and mark resources
        for (uint32_t i = 0; i < layout->setCount; i++) {
            const auto* setLayout = layout->setLayoutHandles[i];

            if (descriptorSets.count(i) != 0) {
                const auto* descriptorSet = descriptorSets.at(i);
                // Mark resources in the argument buffer as resident
                for (const auto& pair : descriptorSet->indicesToTextures) {
                    uint32_t index = pair.first;
                    auto* texture = pair.second;

                    if (texture != nullptr) {
                        uint32_t descriptorIndexClamped = std::min(index, setLayout->descriptorTypeMaxIndex);
                        auto descriptorType = setLayout->descriptorTypes[descriptorIndexClamped];
                        auto usageFlags = metal::mapResourceUsage(descriptorType);

                        if (isCompute) {
                            static_cast<MTL::ComputeCommandEncoder*>(encoder)->useResource(texture, usageFlags);
                        } else {
                            static_cast<MTL::RenderCommandEncoder*>(encoder)->useResource(texture, usageFlags, MTL::RenderStageVertex | MTL::RenderStageFragment);
                        }

                        uint32_t adjustedIndex = index - setLayout->descriptorIndexBases[index] + setLayout->descriptorRangeBinding[index];
                        setLayout->argumentEncoder->setTexture(texture, adjustedIndex);
                    }
                }

                for (const auto& pair : descriptorSet->indicesToBuffers) {
                    uint32_t index = pair.first;
                    const auto& binding = pair.second;

                    if (binding.buffer != nullptr) {
                        uint32_t descriptorIndexClamped = std::min(index, setLayout->descriptorTypeMaxIndex);
                        auto descriptorType = setLayout->descriptorTypes[descriptorIndexClamped];
                        auto usageFlags = metal::mapResourceUsage(descriptorType);

                        if (isCompute) {
                            static_cast<MTL::ComputeCommandEncoder*>(encoder)->useResource(binding.buffer, usageFlags);
                        } else {
                            static_cast<MTL::RenderCommandEncoder*>(encoder)->useResource(binding.buffer, usageFlags, MTL::RenderStageVertex | MTL::RenderStageFragment);
                        }

                        uint32_t adjustedIndex = index - setLayout->descriptorIndexBases[index] + setLayout->descriptorRangeBinding[index];
                        setLayout->argumentEncoder->setBuffer(binding.buffer, binding.offset, adjustedIndex);
                    }
                }

                for (const auto& pair : descriptorSet->indicesToBufferFormattedViews) {
                    uint32_t index = pair.first;
                    auto* texture = pair.second;

                    if (texture != nullptr) {
                        uint32_t descriptorIndexClamped = std::min(index, setLayout->descriptorTypeMaxIndex);
                        auto descriptorType = setLayout->descriptorTypes[descriptorIndexClamped];
                        auto usageFlags = metal::mapResourceUsage(descriptorType);

                        if (isCompute) {
                            static_cast<MTL::ComputeCommandEncoder*>(encoder)->useResource(texture, usageFlags);
                        } else {
                            static_cast<MTL::RenderCommandEncoder*>(encoder)->useResource(texture, usageFlags, MTL::RenderStageVertex | MTL::RenderStageFragment);
                        }
                    }

                    uint32_t adjustedIndex = index - setLayout->descriptorIndexBases[index] + setLayout->descriptorRangeBinding[index];
                    setLayout->argumentEncoder->setTexture(texture, adjustedIndex);
                }

                for (const auto& pair : descriptorSet->indicesToSamplers) {
                    uint32_t index = pair.first;
                    auto* sampler = pair.second;
                    if (sampler != nullptr) {
                        uint32_t adjustedIndex = index - setLayout->descriptorIndexBases[index] + setLayout->descriptorRangeBinding[index];
                        setLayout->argumentEncoder->setSamplerState(sampler, adjustedIndex);
                    }
                }
            }
            
#if     TARGET_OS_OSX
            setLayout->descriptorBuffer->didModifyRange({ 0, setLayout->descriptorBuffer->length() });
#endif

            if (isCompute) {
                static_cast<MTL::ComputeCommandEncoder*>(encoder)->setBuffer(setLayout->descriptorBuffer, 0, i);
            } else {
                static_cast<MTL::RenderCommandEncoder*>(encoder)->setFragmentBuffer(setLayout->descriptorBuffer, 0, i);
            }
        }
    }

    // MetalCommandFence

    MetalCommandFence::MetalCommandFence(MetalDevice *device) {
        semaphore = dispatch_semaphore_create(0);
    }

    MetalCommandFence::~MetalCommandFence() {
        dispatch_release(semaphore);
    }

    // MetalCommandSemaphore

    MetalCommandSemaphore::MetalCommandSemaphore(MetalDevice *device) {
        this->mtl = device->mtl->newEvent();
        this->mtlEventValue = 1;
    }

    MetalCommandSemaphore::~MetalCommandSemaphore() {
        mtl->release();
    }

    // MetalCommandQueue

    MetalCommandQueue::MetalCommandQueue(MetalDevice *device, RenderCommandListType commandListType) {
        assert(device != nullptr);
        assert(commandListType != RenderCommandListType::UNKNOWN);

        this->device = device;
        this->mtl = device->mtl->newCommandQueue();
    }

    MetalCommandQueue::~MetalCommandQueue() {
        mtl->release();
    }

    std::unique_ptr<RenderCommandList> MetalCommandQueue::createCommandList(RenderCommandListType type) {
        return std::make_unique<MetalCommandList>(this, type);
    }

    std::unique_ptr<RenderSwapChain> MetalCommandQueue::createSwapChain(RT64::RenderWindow renderWindow, uint32_t bufferCount, RenderFormat format) {
        return std::make_unique<MetalSwapChain>(this, renderWindow, bufferCount, format);
    }

    void MetalCommandQueue::executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) {
        assert(commandLists != nullptr);
        assert(commandListCount > 0);

        // Encode waiting into the first command list in the chain
        for (uint32_t i = 0; i < waitSemaphoreCount; i++) {
            MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(waitSemaphores[i]);
            const MetalCommandList *interfaceCommandList = static_cast<const MetalCommandList *>(commandLists[0]);
            interfaceCommandList->mtl->encodeWait(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue++);
        }

        for (uint32_t i = 0; i < commandListCount - 1; i++) {
            assert(commandLists[i] != nullptr);

            const MetalCommandList *interfaceCommandList = static_cast<const MetalCommandList *>(commandLists[i]);
            MetalCommandList *mutableCommandList = const_cast<MetalCommandList*>(interfaceCommandList);
            mutableCommandList->commit();
        }

        // Use the last command list to mark the end and signal the fence
        auto *interfaceCommandList = static_cast<const MetalCommandList *>(commandLists[commandListCount - 1]);

        if (signalFence != nullptr) {
            interfaceCommandList->mtl->addCompletedHandler([signalFence, signalSemaphoreCount, signalSemaphores, this](MTL::CommandBuffer* cmdBuffer) {
                dispatch_semaphore_signal(static_cast<MetalCommandFence *>(signalFence)->semaphore);
            });
        }

        for (uint32_t i = 0; i < signalSemaphoreCount; i++) {
            MetalCommandSemaphore *interfaceSemaphore = static_cast<MetalCommandSemaphore *>(signalSemaphores[i]);
            interfaceCommandList->mtl->encodeSignalEvent(interfaceSemaphore->mtl, interfaceSemaphore->mtlEventValue);
        }

        MetalCommandList *mutableCommandList = const_cast<MetalCommandList*>(interfaceCommandList);
        mutableCommandList->commit();
    }

    void MetalCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        auto *metalFence = static_cast<MetalCommandFence *>(fence);
        dispatch_semaphore_wait(metalFence->semaphore, DISPATCH_TIME_FOREVER);
    }

    // MetalPipelineLayout

    MetalPipelineLayout::MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

        this->setCount = desc.descriptorSetDescsCount;

        pushConstantRanges.resize(desc.pushConstantRangesCount);
        memcpy(pushConstantRanges.data(), desc.pushConstantRanges, sizeof(RenderPushConstantRange) * desc.pushConstantRangesCount);

        // Create Descriptor Set Layouts
        for (uint32_t i = 0; i < desc.descriptorSetDescsCount; i++) {
            const RenderDescriptorSetDesc &setDesc = desc.descriptorSetDescs[i];
            setLayoutHandles.emplace_back(new MetalDescriptorSetLayout(device, setDesc));
        }
    }

    MetalPipelineLayout::~MetalPipelineLayout() {}

    // MetalDevice

    MetalDevice::MetalDevice(MetalInterface *renderInterface) {
        assert(renderInterface != nullptr);
        this->renderInterface = renderInterface;
        this->mtl = renderInterface->device;

        // Fill capabilities.
        // TODO: Let's add ray tracing as a second step
        //        capabilities.raytracing = [this->renderInterface->device supportsFamily:MTLGPUFamilyApple9];
        capabilities.maxTextureSize = mtl->supportsFamily(MTL::GPUFamilyApple3) ? 16384 : 8192;
        capabilities.sampleLocations = mtl->programmableSamplePositionsSupported();
#if TARGET_OS_IPHONE
        capabilities.descriptorIndexing = mtl->supportsFamily(MTL::GPUFamilyApple3);
#else
        capabilities.descriptorIndexing = true;
#endif
        capabilities.scalarBlockLayout = true;
        capabilities.presentWait = true;
        capabilities.preferHDR = mtl->recommendedMaxWorkingSetSize() > (512 * 1024 * 1024);
        description.name = "Metal";
    }

    MetalDevice::~MetalDevice() {
        mtl->release();
    }

    std::unique_ptr<RenderDescriptorSet> MetalDevice::createDescriptorSet(const RenderDescriptorSetDesc &desc) {
        return std::make_unique<MetalDescriptorSet>(this, desc);
    }

    std::unique_ptr<RenderShader> MetalDevice::createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        return std::make_unique<MetalShader>(this, data, size, entryPointName, format);
    }

    std::unique_ptr<RenderSampler> MetalDevice::createSampler(const RenderSamplerDesc &desc) {
        return std::make_unique<MetalSampler>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createComputePipeline(const RenderComputePipelineDesc &desc) {
        return std::make_unique<MetalComputePipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) {
        return std::make_unique<MetalGraphicsPipeline>(this, desc);
    }

    std::unique_ptr<RenderPipeline> MetalDevice::createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) {
        // TODO: Support Metal RT
        // return std::make_unique<MetalRaytracingPipeline>(this, desc, previousPipeline);
        return nullptr;
    }

    std::unique_ptr<RenderCommandQueue> MetalDevice::createCommandQueue(RenderCommandListType type) {
        return std::make_unique<MetalCommandQueue>(this, type);
    }

    std::unique_ptr<RenderBuffer> MetalDevice::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<MetalBuffer>(this, nullptr, desc);
    }

    std::unique_ptr<RenderTexture> MetalDevice::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<MetalTexture>(this, nullptr, desc);
    }

    std::unique_ptr<RenderAccelerationStructure> MetalDevice::createAccelerationStructure(const RenderAccelerationStructureDesc &desc) {
        return std::make_unique<MetalAccelerationStructure>(this, desc);
    }

    std::unique_ptr<RenderPool> MetalDevice::createPool(const RenderPoolDesc &desc) {
        return std::make_unique<MetalPool>(this, desc);
    }

    std::unique_ptr<RenderPipelineLayout> MetalDevice::createPipelineLayout(const RenderPipelineLayoutDesc &desc) {
        return std::make_unique<MetalPipelineLayout>(this, desc);
    }

    std::unique_ptr<RenderCommandFence> MetalDevice::createCommandFence() {
        return std::make_unique<MetalCommandFence>(this);
    }

    std::unique_ptr<RenderCommandSemaphore> MetalDevice::createCommandSemaphore() {
        return std::make_unique<MetalCommandSemaphore>(this);
    }

    std::unique_ptr<RenderFramebuffer> MetalDevice::createFramebuffer(const RenderFramebufferDesc &desc) {
        return std::make_unique<MetalFramebuffer>(this, desc);
    }

    void MetalDevice::setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) {
        // TODO: Unimplemented.
    }

    void MetalDevice::setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) {
        // TODO: Unimplemented.
    }

    void MetalDevice::setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) {
        // TODO: Unimplemented.
    }

    const RenderDeviceCapabilities &MetalDevice::getCapabilities() const {
        return capabilities;
    }

    const RenderDeviceDescription &MetalDevice::getDescription() const {
        return description;
    }

    RenderSampleCounts MetalDevice::getSampleCountsSupported(RenderFormat format) const {
        if (mtl->supportsTextureSampleCount(8)) {
            return 8;
        }
        else if (mtl->supportsTextureSampleCount(4)) {
            return 4;
        }
        else if (mtl->supportsTextureSampleCount(2)) {
            return 2;
        }

        return 1;
    }

    void MetalDevice::release() {
        // TODO: Automatic reference counting should take care of this?
    }

    bool MetalDevice::isValid() const {
        return mtl != nullptr;
    }

    bool MetalDevice::beginCapture() {
        auto manager = MTL::CaptureManager::sharedCaptureManager();
        manager->startCapture(mtl);
    }

    bool MetalDevice::endCapture() {
        auto manager = MTL::CaptureManager::sharedCaptureManager();
        manager->stopCapture();
    }

    // MetalInterface

    MetalInterface::MetalInterface() {
        NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

        // We only have one device on Metal atm, so we create it here.
        // Ok, that's not entirely true.. but we'll support just the discrete for now.
        device = MTL::CreateSystemDefaultDevice();
        capabilities.shaderFormat = RenderShaderFormat::METAL;

        createClearShaderLibrary();
        createResolvePipelineState();

        drawables_semaphore = dispatch_semaphore_create(MAX_DRAWABLES);

        releasePool->release();
    }

    MetalInterface::~MetalInterface() {
        for (const auto& [key, state] : clearRenderPipelineStates) {
            state->release();
        }
        
        resolveTexturePipelineState->release();
        clearVertexFunction->release();
        clearColorFunction->release();
        clearDepthFunction->release();
        clearDepthFunction->release();
        device->release();
    }

    std::unique_ptr<RenderDevice> MetalInterface::createDevice() {
        std::unique_ptr<MetalDevice> createdDevice = std::make_unique<MetalDevice>(this);
        return createdDevice->isValid() ? std::move(createdDevice) : nullptr;
    }

    const RenderInterfaceCapabilities &MetalInterface::getCapabilities() const {
        return capabilities;
    }

    bool MetalInterface::isValid() const {
        // check if Metal is available and we support bindless textures: GPUFamilyMac2 or GPUFamilyApple6
        return MTL::CopyAllDevices()->count() > 0 && (device->supportsFamily(MTL::GPUFamilyMac2) || device->supportsFamily(MTL::GPUFamilyApple6));
    }

    void MetalInterface::createResolvePipelineState() {
        const char* resolve_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct ResolveParams {
                uint2 dstOffset;
                uint2 srcOffset;
                uint2 resolveSize;
            };

            kernel void msaaResolve(
                texture2d_ms<float> source [[texture(0)]],
                texture2d<float, access::write> destination [[texture(1)]],
                constant ResolveParams& params [[buffer(0)]],
                uint2 gid [[thread_position_in_grid]])
            {
                if (gid.x >= params.resolveSize.x || gid.y >= params.resolveSize.y) return;
                uint2 dstPos = gid + params.dstOffset;
                uint2 srcPos = gid + params.srcOffset;
                float4 color = float4(0);
                for (uint s = 0; s < source.get_num_samples(); s++) {
                    color += source.read(srcPos, s);
                }
                color /= float(source.get_num_samples());
                destination.write(color, dstPos);
            }
        )";

        NS::Error* error = nullptr;
        auto library = device->newLibrary(NS::String::string(resolve_shader, NS::UTF8StringEncoding), nullptr, &error);
        assert(library != nullptr && "Failed to create library");

        auto resolveFunction = library->newFunction(NS::String::string("msaaResolve", NS::UTF8StringEncoding));
        assert(resolveFunction != nullptr && "Failed to create resolve function");

        error = nullptr;
        resolveTexturePipelineState = device->newComputePipelineState(resolveFunction, &error);
        assert(resolveTexturePipelineState != nullptr && "Failed to create MSAA resolve pipeline state");

        // Destroy
        resolveFunction->release();
        library->release();
    }

    void MetalInterface::createClearShaderLibrary() {
        const char* clear_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct DepthClearFragmentOut {
                float depth [[depth(any)]];
            };

            struct VertexOutput {
                float4 position [[position]];
                uint rect_index [[flat]];
            };

            struct ClearRect {
                float2 position;  // x, y
                float2 size;     // width, height
            };

            struct ClearTransform {
                float4x4 ndcTransform;  // Pre-calculated transform matrix that includes scaling and Y-flip
            };

            // Define quad vertices as a constant buffer to reduce vertex shader work
            constant float2 quadVertices[] = {
                float2(0, 0),  // Top-left
                float2(1, 0),  // Top-right
                float2(0, 1),  // Bottom-left
                float2(1, 1)   // Bottom-right
            };

            vertex VertexOutput clearVert(uint vid [[vertex_id]],
                                        uint instance_id [[instance_id]],
                                        constant ClearRect* rects [[buffer(0)]],
                                        constant ClearTransform& transform [[buffer(1)]])
            {
                VertexOutput out;

                ClearRect rect = rects[instance_id];
                float2 pos = quadVertices[vid];

                // Transform to rect space (screen space coordinates)
                pos = rect.position + (pos * rect.size);

                // Transform from screen space to NDC space using the transform matrix
                out.position = transform.ndcTransform * float4(pos, 0, 1);
                out.rect_index = instance_id;
                return out;
            }

            // Color clear fragment shader
            fragment float4 clearColorFrag(VertexOutput in [[stage_in]],
                                         constant float4* clearColors [[buffer(0)]])
            {
                return clearColors[in.rect_index];
            }

            // Depth clear fragment shader
            fragment DepthClearFragmentOut clearDepthFrag(VertexOutput in [[stage_in]],
                                        constant float* clearDepths [[buffer(0)]])
            {
                DepthClearFragmentOut out;
                out.depth = clearDepths[in.rect_index];
                return out;
            }
        )";

        NS::Error* error = nullptr;
        auto clearShaderLibrary = device->newLibrary(NS::String::string(clear_shader, NS::UTF8StringEncoding), nullptr, &error);
        if (error != nullptr) {
            fprintf(stderr, "Error: %s\n", error->localizedDescription()->utf8String());
        }
        assert(clearShaderLibrary != nullptr && "Failed to create clear color library");

        // Create and cache the shader functions
        clearVertexFunction = clearShaderLibrary->newFunction(MTLSTR("clearVert"));
        clearColorFunction = clearShaderLibrary->newFunction(MTLSTR("clearColorFrag"));
        clearDepthFunction = clearShaderLibrary->newFunction(MTLSTR("clearDepthFrag"));

        // Create depth stencil state
        MTL::DepthStencilDescriptor *depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
        depthDescriptor->setDepthWriteEnabled(true);
        depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
        clearDepthStencilState = device->newDepthStencilState(depthDescriptor);

        depthDescriptor->release();
        clearShaderLibrary->release();
    }

    MTL::RenderPipelineState* MetalInterface::getOrCreateClearRenderPipelineState(MTL::RenderPipelineDescriptor *pipelineDesc) {
        auto hash = metal::hashForRenderPipelineDescriptor(pipelineDesc);

        auto it = clearRenderPipelineStates.find(hash);
        if (it != clearRenderPipelineStates.end()) {
            return it->second;
        } else {
            NS::Error *error = nullptr;
            auto clearPipelineState = device->newRenderPipelineState(pipelineDesc, &error);

            if (error != nullptr) {
                fprintf(stderr, "Failed to create render pipeline state: %s\n", error->localizedDescription()->utf8String());
                return nullptr;
            }

            clearRenderPipelineStates.insert(std::make_pair(hash, clearPipelineState));

            return clearPipelineState;
        }
    }

    // Global creation function.

    std::unique_ptr<RenderInterface> CreateMetalInterface() {
        std::unique_ptr<MetalInterface> createdInterface = std::make_unique<MetalInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
}
