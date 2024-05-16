//
// RT64
//

#include "rt64_metal.h"

namespace RT64 {
    MTLPixelFormat toMTL(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTLPixelFormatInvalid;
            case RenderFormat::R32G32B32A32_TYPELESS:
                return MTLPixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTLPixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_UINT:
                return MTLPixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32A32_SINT:
                return MTLPixelFormatRGBA32Sint;
            case RenderFormat::R32G32B32_TYPELESS:
                return MTLPixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_FLOAT:
                return MTLPixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_UINT:
                return MTLPixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32_SINT:
                return MTLPixelFormatRGBA32Sint;
            case RenderFormat::R16G16B16A16_TYPELESS:
                return MTLPixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTLPixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTLPixelFormatRGBA16Unorm;
            case RenderFormat::R16G16B16A16_UINT:
                return MTLPixelFormatRGBA16Uint;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTLPixelFormatRGBA16Snorm;
            case RenderFormat::R16G16B16A16_SINT:
                return MTLPixelFormatRGBA16Sint;
            case RenderFormat::R32G32_TYPELESS:
                return MTLPixelFormatRG32Float;
            case RenderFormat::R32G32_FLOAT:
                return MTLPixelFormatRG32Float;
            case RenderFormat::R32G32_UINT:
                return MTLPixelFormatRG32Uint;
            case RenderFormat::R32G32_SINT:
                return MTLPixelFormatRG32Sint;
            case RenderFormat::R8G8B8A8_TYPELESS:
                return MTLPixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTLPixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UINT:
                return MTLPixelFormatRGBA8Uint;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTLPixelFormatRGBA8Snorm;
            case RenderFormat::R8G8B8A8_SINT:
                return MTLPixelFormatRGBA8Sint;
            case RenderFormat::B8G8R8A8_UNORM:
                return MTLPixelFormatBGRA8Unorm;
            case RenderFormat::R16G16_TYPELESS:
                return MTLPixelFormatRG16Float;
            case RenderFormat::R16G16_FLOAT:
                return MTLPixelFormatRG16Float;
            case RenderFormat::R16G16_UNORM:
                return MTLPixelFormatRG16Unorm;
            case RenderFormat::R16G16_UINT:
                return MTLPixelFormatRG16Uint;
            case RenderFormat::R16G16_SNORM:
                return MTLPixelFormatRG16Snorm;
            case RenderFormat::R16G16_SINT:
                return MTLPixelFormatRG16Sint;
            case RenderFormat::R32_TYPELESS:
                return MTLPixelFormatR32Float;
            case RenderFormat::D32_FLOAT:
                return MTLPixelFormatDepth32Float;
            case RenderFormat::R32_FLOAT:
                return MTLPixelFormatR32Float;
            case RenderFormat::R32_UINT:
                return MTLPixelFormatR32Uint;
            case RenderFormat::R32_SINT:
                return MTLPixelFormatR32Sint;
            case RenderFormat::R8G8_TYPELESS:
                return MTLPixelFormatRG8Unorm;
            case RenderFormat::R8G8_UNORM:
                return MTLPixelFormatRG8Unorm;
            case RenderFormat::R8G8_UINT:
                return MTLPixelFormatRG8Uint;
            case RenderFormat::R8G8_SNORM:
                return MTLPixelFormatRG8Snorm;
            case RenderFormat::R8G8_SINT:
                return MTLPixelFormatRG8Sint;
            case RenderFormat::R16_TYPELESS:
                return MTLPixelFormatR16Float;
            case RenderFormat::R16_FLOAT:
                return MTLPixelFormatR16Float;
            case RenderFormat::D16_UNORM:
                return MTLPixelFormatDepth16Unorm;
            case RenderFormat::R16_UNORM:
                return MTLPixelFormatR16Unorm;
            case RenderFormat::R16_UINT:
                return MTLPixelFormatR16Uint;
            case RenderFormat::R16_SNORM:
                return MTLPixelFormatR16Snorm;
            case RenderFormat::R16_SINT:
                return MTLPixelFormatR16Sint;
            case RenderFormat::R8_TYPELESS:
                return MTLPixelFormatR8Unorm;
            case RenderFormat::R8_UNORM:
                return MTLPixelFormatR8Unorm;
            case RenderFormat::R8_UINT:
                return MTLPixelFormatR8Uint;
            case RenderFormat::R8_SNORM:
                return MTLPixelFormatR8Snorm;
            case RenderFormat::R8_SINT:
                return MTLPixelFormatR8Sint;
            default:
                assert(false && "Unknown format.");
                return MTLPixelFormatInvalid;
        }
    }

    static MTLTextureType toTextureType(RenderTextureDimension dimension) {
        switch (dimension) {
            case RenderTextureDimension::TEXTURE_1D:
                return MTLTextureType1D;
            case RenderTextureDimension::TEXTURE_2D:
                return MTLTextureType2D;
            case RenderTextureDimension::TEXTURE_3D:
                return MTLTextureType3D;
            default:
                assert(false && "Unknown resource dimension.");
                return MTLTextureType2D;
        }
    }

    static MTLCullMode toMTL(RenderCullMode cullMode) {
        switch (cullMode) {
            case RenderCullMode::NONE:
                return MTLCullModeNone;
            case RenderCullMode::FRONT:
                return MTLCullModeFront;
            case RenderCullMode::BACK:
                return MTLCullModeBack;
            default:
                assert(false && "Unknown cull mode.");
                return MTLCullModeNone;
        }
    }

    static MTLPrimitiveType toMTL(RenderPrimitiveTopology topology) {
        switch (topology) {
            case RenderPrimitiveTopology::POINT_LIST:
                return MTLPrimitiveTypePoint;
            case RenderPrimitiveTopology::LINE_LIST:
                return MTLPrimitiveTypeLine;
            case RenderPrimitiveTopology::TRIANGLE_LIST:
                return MTLPrimitiveTypeTriangle;
            default:
                assert(false && "Unknown primitive topology type.");
                return MTLPrimitiveTypePoint;
        }
    }

    static MTLBlendFactor toMTL(RenderBlend blend) {
        switch (blend) {
            case RenderBlend::ZERO:
                return MTLBlendFactorZero;
            case RenderBlend::ONE:
                return MTLBlendFactorOne;
            case RenderBlend::SRC_COLOR:
                return MTLBlendFactorSourceColor;
            case RenderBlend::INV_SRC_COLOR:
                return MTLBlendFactorOneMinusSourceColor;
            case RenderBlend::SRC_ALPHA:
                return MTLBlendFactorSourceAlpha;
            case RenderBlend::INV_SRC_ALPHA:
                return MTLBlendFactorOneMinusSourceAlpha;
            case RenderBlend::DEST_ALPHA:
                return MTLBlendFactorDestinationAlpha;
            case RenderBlend::INV_DEST_ALPHA:
                return MTLBlendFactorOneMinusDestinationAlpha;
            case RenderBlend::DEST_COLOR:
                return MTLBlendFactorDestinationColor;
            case RenderBlend::INV_DEST_COLOR:
                return MTLBlendFactorOneMinusDestinationColor;
            case RenderBlend::SRC_ALPHA_SAT:
                return MTLBlendFactorSourceAlphaSaturated;
            case RenderBlend::BLEND_FACTOR:
                return MTLBlendFactorBlendColor;
            case RenderBlend::INV_BLEND_FACTOR:
                return MTLBlendFactorOneMinusBlendColor;
            case RenderBlend::SRC1_COLOR:
                return MTLBlendFactorSource1Color;
            case RenderBlend::INV_SRC1_COLOR:
                return MTLBlendFactorOneMinusSource1Color;
            case RenderBlend::SRC1_ALPHA:
                return MTLBlendFactorSource1Alpha;
            case RenderBlend::INV_SRC1_ALPHA:
                return MTLBlendFactorOneMinusSource1Alpha;
            default:
                assert(false && "Unknown blend factor.");
                return MTLBlendFactorZero;
        }
    }

    static MTLBlendOperation toMTL(RenderBlendOperation operation) {
        switch (operation) {
            case RenderBlendOperation::ADD:
                return MTLBlendOperationAdd;
            case RenderBlendOperation::SUBTRACT:
                return MTLBlendOperationSubtract;
            case RenderBlendOperation::REV_SUBTRACT:
                return MTLBlendOperationReverseSubtract;
            case RenderBlendOperation::MIN:
                return MTLBlendOperationMin;
            case RenderBlendOperation::MAX:
                return MTLBlendOperationMax;
            default:
                assert(false && "Unknown blend operation.");
                return MTLBlendOperationAdd;
        }
    }

    // Metal does not support Logic Operations in the public API.

    static MTLCompareFunction toMTL(RenderComparisonFunction function) {
        switch (function) {
            case RenderComparisonFunction::NEVER:
                return MTLCompareFunctionNever;
            case RenderComparisonFunction::LESS:
                return MTLCompareFunctionLess;
            case RenderComparisonFunction::EQUAL:
                return MTLCompareFunctionEqual;
            case RenderComparisonFunction::LESS_EQUAL:
                return MTLCompareFunctionLessEqual;
            case RenderComparisonFunction::GREATER:
                return MTLCompareFunctionGreater;
            case RenderComparisonFunction::NOT_EQUAL:
                return MTLCompareFunctionNotEqual;
            case RenderComparisonFunction::GREATER_EQUAL:
                return MTLCompareFunctionGreaterEqual;
            case RenderComparisonFunction::ALWAYS:
                return MTLCompareFunctionAlways;
            default:
                assert(false && "Unknown comparison function.");
                return MTLCompareFunctionNever;
        }
    }

    static MTLSamplerMinMagFilter toMTL(RenderFilter filter) {
        switch (filter) {
            case RenderFilter::NEAREST:
                return MTLSamplerMinMagFilterNearest;
            case RenderFilter::LINEAR:
                return MTLSamplerMinMagFilterLinear;
            default:
                assert(false && "Unknown filter.");
                return MTLSamplerMinMagFilterNearest;
        }
    }

    static MTLSamplerMipFilter toMTL(RenderMipmapMode mode) {
        switch (mode) {
            case RenderMipmapMode::NEAREST:
                return MTLSamplerMipFilterNearest;
            case RenderMipmapMode::LINEAR:
                return MTLSamplerMipFilterLinear;
            default:
                assert(false && "Unknown mipmap mode.");
                return MTLSamplerMipFilterNearest;
        }
    }

    static MTLSamplerAddressMode toMTL(RenderTextureAddressMode mode) {
        switch (mode) {
            case RenderTextureAddressMode::WRAP:
                return MTLSamplerAddressModeRepeat;
            case RenderTextureAddressMode::MIRROR:
                return MTLSamplerAddressModeMirrorRepeat;
            case RenderTextureAddressMode::CLAMP:
                return MTLSamplerAddressModeClampToEdge;
            case RenderTextureAddressMode::BORDER:
                return MTLSamplerAddressModeClampToBorderColor;
            case RenderTextureAddressMode::MIRROR_ONCE:
                return MTLSamplerAddressModeMirrorClampToEdge;
            default:
                assert(false && "Unknown texture address mode.");
                return MTLSamplerAddressModeRepeat;
        }
    }

    static MTLSamplerBorderColor toMTL(RenderBorderColor color) {
        switch (color) {
            case RenderBorderColor::TRANSPARENT_BLACK:
                return MTLSamplerBorderColorTransparentBlack;
            case RenderBorderColor::OPAQUE_BLACK:
                return MTLSamplerBorderColorOpaqueBlack;
            case RenderBorderColor::OPAQUE_WHITE:
                return MTLSamplerBorderColorOpaqueWhite;
            default:
                assert(false && "Unknown border color.");
                return MTLSamplerBorderColorTransparentBlack;
        }
    }

    static MTLIndexType toIndexType(RenderFormat format) {
        switch (format) {
            case RenderFormat::R16_UINT:
                return MTLIndexTypeUInt16;
            case RenderFormat::R32_UINT:
                return MTLIndexTypeUInt32;
            default:
                assert(false && "Format is not supported as an index type.");
                return MTLIndexTypeUInt16;
        }
    }

    // MetalBuffer

    MetalBuffer::MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;
    }

    MetalBuffer::~MetalBuffer() {
        // TODO: ARC should handle this
    }

    void *MetalBuffer::map(uint32_t subresource, const RT64::RenderRange *readRange) {

    }

    void MetalBuffer::unmap(uint32_t subresource, const RT64::RenderRange *writtenRange) {

    }

    std::unique_ptr<RenderBufferFormattedView> MetalBuffer::createBufferFormattedView(RenderFormat format) {
        return std::make_unique<MetalBufferFormattedView>(this, format);
    }

    // MetalBufferFormattedView

    MetalBufferFormattedView::MetalBufferFormattedView(RT64::MetalBuffer *buffer, RT64::RenderFormat format) {
        assert(buffer != nullptr);
        assert((buffer->desc.flags & RenderBufferFlag::FORMATTED) && "Buffer must allow formatted views.");

        this->buffer = buffer;
    }

    MetalBufferFormattedView::~MetalBufferFormattedView() {
        // TODO: ARC should handle this
    }

    // MetalTexture

    MetalTexture::MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;

        auto descriptor = [MTLTextureDescriptor new];

        [descriptor setTextureType: toTextureType(desc.dimension)];
        [descriptor setPixelFormat: toMTL(desc.format)];
        [descriptor setWidth: desc.width];
        [descriptor setHeight: desc.height];
        [descriptor setDepth: desc.depth];
        [descriptor setMipmapLevelCount: desc.mipLevels];
        [descriptor setArrayLength: 1];
        [descriptor setSampleCount: desc.multisampling.sampleCount];
        // TODO: Usage flags
        [descriptor setUsage: MTLTextureUsageUnknown];

        this->mtlTexture = [device->device newTextureWithDescriptor: descriptor];
    }

    MetalTexture::~MetalTexture() {
        // TODO: Should be handled by ARC
    }

    std::unique_ptr<RenderTextureView> MetalTexture::createTextureView(const RenderTextureViewDesc &desc) {
        return std::make_unique<MetalTextureView>(this, desc);
    }

    void MetalTexture::setName(const std::string &name) {
        // TODO: Set this later on MetalTextureView
    }

    // MetalTextureView

    MetalTextureView::MetalTextureView(MetalTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);

        this->texture = texture;

        this->mtlTexture = texture->mtlTexture;
    }

    MetalTextureView::~MetalTextureView() {
        // TODO: Should be handled by ARC
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

    // MetalPipelineLayout

    MetalPipelineLayout::MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

        this->device = device;
    }

    MetalPipelineLayout::~MetalPipelineLayout() {
        // TODO: Should be handled by ARC
    }

    // MetalShader

    MetalShader::MetalShader(MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        assert(device != nullptr);
        assert(data != nullptr);
        assert(size > 0);
        assert(format != RenderShaderFormat::UNKNOWN);
        assert(format == RenderShaderFormat::METAL);

        this->device = device;
        this->format = format;
        this->entryPointName = (entryPointName != nullptr) ? std::string(entryPointName) : std::string();

        id<MTLLibrary> library = [device->device newLibraryWithData: static_cast<dispatch_data_t>([NSData dataWithBytes:data length:size]) error: nullptr];
        this->function = [library newFunctionWithName: [NSString stringWithUTF8String: entryPointName]];
    }

    MetalShader::~MetalShader() {
        // TODO: Should be handled by ARC
    }

    // MetalSampler

    MetalSampler::MetalSampler(MetalDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLSamplerDescriptor *descriptor = [MTLSamplerDescriptor new];
        [descriptor setMinFilter: toMTL(desc.minFilter)];
        [descriptor setMagFilter: toMTL(desc.magFilter)];
        [descriptor setMipFilter: toMTL(desc.mipmapMode)];
        [descriptor setRAddressMode: toMTL(desc.addressU)];
        [descriptor setSAddressMode: toMTL(desc.addressV)];
        [descriptor setTAddressMode: toMTL(desc.addressW)];
        [descriptor setMaxAnisotropy: desc.maxAnisotropy];
        [descriptor setCompareFunction: toMTL(desc.comparisonFunc)];
        [descriptor setLodMinClamp: desc.minLOD];
        [descriptor setLodMaxClamp: desc.maxLOD];
        [descriptor setBorderColor: toMTL(desc.borderColor)];

        this->samplerState = [device->device newSamplerStateWithDescriptor: descriptor];
    }

    MetalSampler::~MetalSampler() {
        // TODO: Should be handled by ARC
    }

    // MetalPipeline

    MetalPipeline::MetalPipeline(MetalDevice *device, MetalPipeline::Type type) {
        assert(device != nullptr);
        assert(type != Type::Unknown);

        this->device = device;
        this->type = type;
    }

    MetalPipeline::~MetalPipeline() { }

    // MetalComputePipeline

    MetalComputePipeline::MetalComputePipeline(MetalDevice *device, const RenderComputePipelineDesc &desc) : MetalPipeline(device, Type::Compute) {
        assert(desc.computeShader != nullptr);
        assert(desc.pipelineLayout != nullptr);

        const auto *computeShader = static_cast<const MetalShader *>(desc.computeShader);

        MTLComputePipelineDescriptor *descriptor = [MTLComputePipelineDescriptor new];
        [descriptor setComputeFunction: computeShader->function];
        [descriptor setLabel: [NSString stringWithUTF8String: computeShader->entryPointName.c_str()]];

        this->state = [device->device newComputePipelineStateWithDescriptor: descriptor options: MTLPipelineOptionNone reflection: nil error: nullptr];
    }

    MetalComputePipeline::~MetalComputePipeline() {
        // TODO: Should be handled by ARC
    }

    RenderPipelineProgram MetalComputePipeline::getProgram(const std::string &name) const {
        assert(false && "Compute pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalGraphicsPipeline

    MetalGraphicsPipeline::MetalGraphicsPipeline(MetalDevice *device, const RenderGraphicsPipelineDesc &desc) : MetalPipeline(device, Type::Graphics) {
        assert(desc.pipelineLayout != nullptr);

        MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];

        assert(desc.vertexShader != nullptr && "Cannot create a valid MTLRenderPipelineState without a vertex shader!");
        const auto *metalShader = static_cast<const MetalShader *>(desc.vertexShader);

        [descriptor setVertexFunction: metalShader->function];

        assert(desc.geometryShader == nullptr && "Metal does not support geometry shaders!");

        if (desc.pixelShader != nullptr) {
            const auto *pixelShader = static_cast<const MetalShader *>(desc.pixelShader);
            [descriptor setFragmentFunction: pixelShader->function];
        }

        for (uint32_t i = 0; i < desc.inputSlotsCount; i++) {
            MTLPipelineBufferDescriptor *bufferDescriptor = [MTLPipelineBufferDescriptor new];
        }
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline() {
        // TODO: Should be handled by ARC
    }

    RenderPipelineProgram MetalGraphicsPipeline::getProgram(const std::string &name) const {
        assert(false && "Graphics pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalCommandList

    MetalCommandList::MetalCommandList(MetalCommandQueue *queue, RenderCommandListType type) {
        assert(device != nullptr);
        assert(type != RenderCommandListType::UNKNOWN);

        this->device = queue->device;
        this->type = type;
        this->queue = queue;
    }

    MetalCommandList::~MetalCommandList() {
        // TODO: Should be handled by ARC
    }

    void MetalCommandList::begin() {
        switch (type) {
            // Follows D3D12 Command List model
            // DIRECT can Render, Compute, Copy
            // COMPUTE can Compute, Copy
            // COPY can Copy
            case RenderCommandListType::DIRECT: {
                auto renderDescriptor = [MTLRenderPassDescriptor new];
                renderEncoder = [queue->buffer renderCommandEncoderWithDescriptor: renderDescriptor];

                auto computeDescriptor = [MTLComputePassDescriptor new];
                computeEncoder = [queue->buffer computeCommandEncoderWithDescriptor: computeDescriptor];

                auto blitDescriptor = [MTLBlitPassDescriptor new];
                blitEncoder = [queue->buffer blitCommandEncoderWithDescriptor: blitDescriptor];
                break;
            }
            case RenderCommandListType::COMPUTE: {
                auto computeDescriptor = [MTLComputePassDescriptor new];
                computeEncoder = [queue->buffer computeCommandEncoderWithDescriptor: computeDescriptor];

                auto blitDescriptor = [MTLBlitPassDescriptor new];
                blitEncoder = [queue->buffer blitCommandEncoderWithDescriptor: blitDescriptor];
                break;
            }
            case RenderCommandListType::COPY: {
                auto blitDescriptor = [MTLBlitPassDescriptor new];
                blitEncoder = [queue->buffer blitCommandEncoderWithDescriptor: blitDescriptor];
                break;
            }
            default:
                assert(false && "Unknown pipeline type.");
                break;
        }
    }

    void MetalCommandList::end() {
        switch (type) {
            case RenderCommandListType::DIRECT: {
                assert(renderEncoder != nil && "Cannot end encoding on nil MTLRenderCommandEncoder");
                assert(computeEncoder != nil && "Cannot end encoding on nil MTLComputeCommandEncoder");
                assert(blitEncoder != nil && "Cannot end encoding on nil MTLBlitCommandEncoder");

                [renderEncoder endEncoding];
                [computeEncoder endEncoding];
                [blitEncoder endEncoding];

                renderEncoder = nil;
                computeEncoder = nil;
                blitEncoder = nil;
                break;
            }
            case RenderCommandListType::COMPUTE: {
                assert(computeEncoder != nil && "Cannot end encoding on nil MTLComputeCommandEncoder");
                assert(blitEncoder != nil && "Cannot end encoding on nil MTLBlitCommandEncoder");

                [computeEncoder endEncoding];
                [blitEncoder endEncoding];

                computeEncoder = nil;
                blitEncoder = nil;
                break;
            }
            case RenderCommandListType::COPY: {
                assert(blitEncoder != nil && "Cannot end encoding on nil MTLBlitCommandEncoder");

                [blitEncoder endEncoding];
                blitEncoder = nil;
                break;
            }
            default:
                assert(false && "Unknown pipeline type.");
                break;
        }
    }

    void MetalCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        // TODO: Ignore for now, Metal should handle most of this itself.
    }

    void MetalCommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
        assert(computeEncoder != nil && "Cannot encode dispatch on nil MTLComputeCommandEncoder!");

        [computeEncoder dispatchThreadgroups: MTLSizeMake(threadGroupCountX, threadGroupCountY, threadGroupCountZ) threadsPerThreadgroup: MTLSizeMake(1, 1, 1)];
    }

    void MetalCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        // TODO: Support Metal RT
    }

    void MetalCommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
        assert(renderEncoder != nil && "Cannot encode draw on nil MTLRenderCommandEncoder!");

        [renderEncoder drawPrimitives: currentPrimitiveType
                          vertexStart: startVertexLocation
                          vertexCount: vertexCountPerInstance
                        instanceCount: instanceCount
                         baseInstance: startInstanceLocation];
    }

    void MetalCommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
        assert(renderEncoder != nil && "Cannot encode draw on nil MTLRenderCommandEncoder!");

        [renderEncoder drawIndexedPrimitives: currentPrimitiveType
                                  indexCount: indexCountPerInstance
                                   indexType: currentIndexType
                                 indexBuffer: indexBuffer
                           indexBufferOffset: startIndexLocation
                               instanceCount: instanceCount
                                  baseVertex: baseVertexLocation
                                baseInstance: startInstanceLocation];
    }

    void MetalCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const auto *interfacePipeline = static_cast<const MetalPipeline *>(pipeline);
        switch (interfacePipeline->type) {
            case MetalPipeline::Type::Compute: {
                const auto *computePipeline = static_cast<const MetalComputePipeline *>(interfacePipeline);
                assert(computeEncoder != nil && "Cannot set pipeline state on nil MTLComputeCommandEncoder!");
                [computeEncoder setComputePipelineState: computePipeline->state];
                break;
            }
            case MetalPipeline::Type::Graphics: {
                const auto *graphicsPipeline = static_cast<const MetalGraphicsPipeline *>(interfacePipeline);
                assert(renderEncoder != nil && "Cannot set pipeline state on nil MTLRenderCommandEncoder!");
                [renderEncoder setRenderPipelineState: graphicsPipeline->state];
                break;
            }
            default:
                assert(false && "Unknown pipeline type.");
                break;
        }
    }

    void MetalCommandList::setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);
        // TODO: Layouts
    }

    void MetalCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data) {
        assert(computeEncoder != nil && "Cannot set bytes on nil MTLComputeCommandEncoder!");
        // [this->computeEncoder setBytes: data length: atIndex: rangeIndex];
        // TODO: Push Constants
    }

    void MetalCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        // setDescriptorSet()
        // TODO: Descriptor Sets
    }

    void MetalCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);
        // TODO: Layouts
    }

    void MetalCommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void *data) {
        assert(renderEncoder != nil && "Cannot set bytes on nil MTLRenderCommandEncoder!");
        // TODO: Push Constants
    }

    void MetalCommandList::setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        // setDescriptorSet()
        // TODO: Descriptor Sets
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
        // TODO: Argument Buffer Creation & Binding
        if (view != nullptr) {
            const auto *interfaceBuffer = static_cast<const MetalBuffer *>(view->buffer.ref);

        }
    }

    void MetalCommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
        // TODO: Argument Buffer Creation & Binding
        if ((views != nullptr) && (viewCount > 0)) {
            assert(inputSlots != nullptr);
        }
    }

    void MetalCommandList::setViewports(const RenderViewport *viewports, uint32_t count) {
        assert(renderEncoder != nil && "Cannot set viewports on nil MTLRenderCommandEncoder!");

        if (count > 1) {
            thread_local std::vector<MTLViewport> viewportVector;
            viewportVector.clear();

            for (uint32_t i = 0; i < count; i++) {
                viewportVector.emplace_back(MTLViewport { viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth });
            }

            [renderEncoder setViewports: viewportVector.data() count: count];
        }
        else {
            // Single element fast path.
            auto viewport = MTLViewport { viewports[0].x, viewports[0].y, viewports[0].width, viewports[0].height, viewports[0].minDepth, viewports[0].maxDepth };
            [renderEncoder setViewport: viewport];
        }
    }

    void MetalCommandList::setScissors(const RenderRect *scissorRects, uint32_t count) {
        assert(renderEncoder != nil && "Cannot set scissors on nil MTLRenderCommandEncoder!");

        if (count > 1) {
            thread_local std::vector<MTLScissorRect> scissorVector;
            scissorVector.clear();

            for (uint32_t i = 0; i < count; i++) {
                scissorVector.emplace_back(MTLScissorRect { uint32_t(scissorRects[i].left), uint32_t(scissorRects[i].right), uint32_t(scissorRects[i].right - scissorRects[i].left), uint32_t(scissorRects[i].bottom - scissorRects[i].top) });
            }

            [renderEncoder setScissorRects: scissorVector.data() count: count];
        }
        else {
            // Single element fast path.
            auto scissor = MTLScissorRect { uint32_t(scissorRects[0].left), uint32_t(scissorRects[0].right), uint32_t(scissorRects[0].right - scissorRects[0].left), uint32_t(scissorRects[0].bottom - scissorRects[0].top) };
            [renderEncoder setScissorRect: scissor];
        }
    }

    void MetalCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {

    }

    void MetalCommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) {

    }

    void MetalCommandList::clearDepth(bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) {

    }

    void MetalCommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);
        assert(blitEncoder != nil && "Cannot copy buffer region on a nil MTLBlitCommandEncoder");

        const auto interfaceDstBuffer = static_cast<const MetalBuffer *>(dstBuffer.ref);
        const auto interfaceSrcBuffer = static_cast<const MetalBuffer *>(srcBuffer.ref);

        [blitEncoder copyFromBuffer: interfaceDstBuffer->buffer
                       sourceOffset: 0
                           toBuffer: interfaceSrcBuffer->buffer
                  destinationOffset: 0
                               size: size];
    }

    void MetalCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) {
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(blitEncoder != nil && "Cannot copy texture region on a nil MTLBlitCommandEncoder");

        const auto dstTexture = static_cast<const MetalTexture *>(dstLocation.texture);
        const auto srcTexture = static_cast<const MetalTexture *>(srcLocation.texture);
        const auto dstBuffer = static_cast<const MetalBuffer *>(dstLocation.buffer);
        const auto srcBuffer = static_cast<const MetalBuffer *>(srcLocation.buffer);
        if ((dstLocation.type == RenderTextureCopyType::SUBRESOURCE) && (srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT)) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            auto origin = MTLOriginMake(dstX, dstY, dstZ);
            auto size = MTLSizeMake(srcTexture->desc.width, srcTexture->desc.height, srcTexture->desc.depth);

            [blitEncoder copyFromBuffer: srcBuffer->buffer
                           sourceOffset: srcLocation.placedFootprint.offset
                      sourceBytesPerRow: srcLocation.placedFootprint.rowWidth
                    // TODO: Check this calculation is correct
                    sourceBytesPerImage: srcLocation.placedFootprint.rowWidth * srcLocation.placedFootprint.height
                             sourceSize: size
                              toTexture: dstTexture->mtlTexture
                       destinationSlice: 0
                       destinationLevel: 0
                      destinationOrigin: origin];
        }
        else {
            auto origin = MTLOriginMake(dstX, dstY, dstZ);
            MTLSize size;

            if (srcBox != nullptr) {
                size.width = srcBox->right - srcBox->left;
                size.height = srcBox->bottom - srcBox->top;
                size.depth = srcBox->back - srcBox->front;
            }
            else {
                size.width = srcTexture->desc.width;
                size.height = srcTexture->desc.height;
                size.depth = srcTexture->desc.depth;
            }

            [blitEncoder copyFromTexture: dstTexture->mtlTexture
                             sourceSlice: 0
                             sourceLevel: 0
                            sourceOrigin: origin
                              sourceSize: size
                               toTexture: dstTexture->mtlTexture
                        destinationSlice: 0
                        destinationLevel: 0
                       destinationOrigin: origin];
        }
    }

    void MetalCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);
        assert(blitEncoder != nil && "Cannot copy buffer on nil MTLBlitCommandEncoder!");

        const auto dst = static_cast<const MetalBuffer *>(dstBuffer);
        const auto src = static_cast<const MetalBuffer *>(srcBuffer);

        [blitEncoder copyFromBuffer: dst->buffer
                       sourceOffset: 0
                           toBuffer: src->buffer
                  destinationOffset: 0
                               size: dst->desc.size];
    }

    void MetalCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);
        assert(blitEncoder != nil && "Cannot copy texture on nil MTLBlitCommandEncoder!");

        const auto dst = static_cast<const MetalTexture *>(dstTexture);
        const auto src = static_cast<const MetalTexture *>(srcTexture);

        [blitEncoder copyFromTexture: dst->mtlTexture
                           toTexture: src->mtlTexture];
    }

    // MetalCommandQueue

    MetalCommandQueue::MetalCommandQueue(MetalDevice *device, RenderCommandListType commandListType) {
        assert(device != nullptr);
        assert(commandListType != RenderCommandListType::UNKNOWN);

        this->device = device;
        this->buffer = [device->queue commandBuffer];
    }

    MetalCommandQueue::~MetalCommandQueue() {
        // TODO: Should be handled by ARC
    }

    std::unique_ptr<RenderCommandList> MetalCommandQueue::createCommandList(RenderCommandListType type) {
        return std::make_unique<MetalCommandList>(this, type);
    }

    void MetalCommandQueue::executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandFence *signalFence) {
        assert(commandLists != nullptr);
        assert(commandListCount > 0);

        [buffer enqueue];
        [buffer commit];

        this->buffer = [buffer.commandQueue commandBuffer];
    }

    void MetalCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        // TODO: Should be handled by hazard tracking.
    }

    // MetalPool

    MetalPool::MetalPool(MetalDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLHeapDescriptor *descriptor = [MTLHeapDescriptor new];
        // TODO: Set Descriptor properties correctly
        [descriptor setType: MTLHeapTypeAutomatic];

        this->heap = [device->device newHeapWithDescriptor: descriptor];
    }

    // MetalDevice

    MetalDevice::MetalDevice(MetalInterface *renderInterface) {
        assert(renderInterface != nullptr);
        this->renderInterface = renderInterface;
        this->device = renderInterface->device;
        this->queue = [device newCommandQueue];

        // Fill capabilities.
        // TODO: Let's add ray tracing as a second step
//        capabilities.raytracing = [this->renderInterface->device supportsFamily:MTLGPUFamilyApple9];
        capabilities.maxTextureSize = 16384;
        capabilities.sampleLocations = [device areProgrammableSamplePositionsSupported];
        capabilities.descriptorIndexing = true;
        // TODO: check if this came after MacFamily2
        capabilities.scalarBlockLayout = true;
        capabilities.presentWait = true;
    }

    MetalDevice::~MetalDevice() {
        // TODO: Automatic reference counting should take care of this.
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

    std::unique_ptr<RenderFramebuffer> MetalDevice::createFramebuffer(const RenderFramebufferDesc &desc) {
        return std::make_unique<MetalFramebuffer>(this, desc);
    }

    const RenderDeviceCapabilities &MetalDevice::getCapabilities() const {
        return capabilities;
    }

    RenderSampleCounts MetalDevice::getSampleCountsSupported(RenderFormat format) const {

    }

    // VulkanInterface

    MetalInterface::MetalInterface() {
        // We only have one device on Metal atm, so we create it here.
        // Ok, that's not entirely true.. but we'll support just the discrete for now.
        device = MTLCreateSystemDefaultDevice();
        capabilities.shaderFormat = RenderShaderFormat::METAL;
    }

    MetalInterface::~MetalInterface() {
        // TODO: Automatic reference counting should take care of this.
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
        return [MTLCopyAllDevices() count] > 0 && ([device supportsFamily:MTLGPUFamilyMac2] || [device supportsFamily:MTLGPUFamilyApple6]);
    }

    // Global creation function.

    std::unique_ptr<RenderInterface> CreateMetalInterface() {
        std::unique_ptr<MetalInterface> createdInterface = std::make_unique<MetalInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
};