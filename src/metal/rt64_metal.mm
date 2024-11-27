//
// RT64
//

#include "rt64_metal.h"
#import <AppKit/AppKit.h>

namespace RT64 {

    static size_t calculateAlignedSize(size_t size, size_t alignment = 16) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    MTLDataType toMTL(RenderDescriptorRangeType type) {
        switch (type) {
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                return MTLDataTypeTexture;
                
            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
                return MTLDataTypePointer;
                
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
                return MTLDataTypePointer;
                
            case RenderDescriptorRangeType::SAMPLER:
                return MTLDataTypeSampler;
                
            default:
                assert(false && "Unknown descriptor range type.");
                return MTLDataTypeNone;
        }
    }

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

    static MTLVertexFormat toVertexFormat(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTLVertexFormatInvalid;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTLVertexFormatFloat4;
            case RenderFormat::R32G32B32A32_UINT:
                return MTLVertexFormatUInt4;
            case RenderFormat::R32G32B32A32_SINT:
                return MTLVertexFormatInt4;
            case RenderFormat::R32G32B32_FLOAT:
                return MTLVertexFormatFloat3;
            case RenderFormat::R32G32B32_UINT:
                return MTLVertexFormatUInt3;
            case RenderFormat::R32G32B32_SINT:
                return MTLVertexFormatInt3;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTLVertexFormatHalf4;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTLVertexFormatUShort4Normalized;
            case RenderFormat::R16G16B16A16_UINT:
                return MTLVertexFormatUShort4;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTLVertexFormatShort4Normalized;
            case RenderFormat::R16G16B16A16_SINT:
                return MTLVertexFormatShort4;
            case RenderFormat::R32G32_FLOAT:
                return MTLVertexFormatFloat2;
            case RenderFormat::R32G32_UINT:
                return MTLVertexFormatUInt2;
            case RenderFormat::R32G32_SINT:
                return MTLVertexFormatInt2;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTLVertexFormatUChar4Normalized;
            case RenderFormat::R8G8B8A8_UINT:
                return MTLVertexFormatUChar4;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTLVertexFormatChar4Normalized;
            case RenderFormat::R8G8B8A8_SINT:
                return MTLVertexFormatChar4;
            case RenderFormat::R16G16_FLOAT:
                return MTLVertexFormatHalf2;
            case RenderFormat::R16G16_UNORM:
                return MTLVertexFormatUShort2Normalized;
            case RenderFormat::R16G16_UINT:
                return MTLVertexFormatUShort2;
            case RenderFormat::R16G16_SNORM:
                return MTLVertexFormatShort2Normalized;
            case RenderFormat::R16G16_SINT:
                return MTLVertexFormatShort2;
            case RenderFormat::R32_FLOAT:
                return MTLVertexFormatFloat;
            case RenderFormat::R32_UINT:
                return MTLVertexFormatUInt;
            case RenderFormat::R32_SINT:
                return MTLVertexFormatInt;
            case RenderFormat::R8G8_UNORM:
                return MTLVertexFormatUChar2Normalized;
            case RenderFormat::R8G8_UINT:
                return MTLVertexFormatUChar2;
            case RenderFormat::R8G8_SNORM:
                return MTLVertexFormatChar2Normalized;
            case RenderFormat::R8G8_SINT:
                return MTLVertexFormatChar2;
            case RenderFormat::R16_FLOAT:
                return MTLVertexFormatHalf;
            case RenderFormat::R16_UNORM:
                return MTLVertexFormatUShortNormalized;
            case RenderFormat::R16_UINT:
                return MTLVertexFormatUShort;
            case RenderFormat::R16_SNORM:
                return MTLVertexFormatShortNormalized;
            case RenderFormat::R16_SINT:
                return MTLVertexFormatShort;
            case RenderFormat::R8_UNORM:
                return MTLVertexFormatUCharNormalized;
            case RenderFormat::R8_UINT:
                return MTLVertexFormatUChar;
            case RenderFormat::R8_SNORM:
                return MTLVertexFormatCharNormalized;
            case RenderFormat::R8_SINT:
                return MTLVertexFormatChar;
            default:
                assert(false && "Unsupported vertex format.");
                return MTLVertexFormatInvalid;
        }
    }

    static MTLTextureType toTextureType(RenderTextureDimension dimension, RenderSampleCounts sampleCount) {
        switch (dimension) {
            case RenderTextureDimension::TEXTURE_1D:
                assert(sampleCount <= 1 && "Multisampling not supported for 1D textures");
                return MTLTextureType1D;
            case RenderTextureDimension::TEXTURE_2D:
                return (sampleCount > 1) ? MTLTextureType2DMultisample : MTLTextureType2D;
            case RenderTextureDimension::TEXTURE_3D:
                assert(sampleCount <= 1 && "Multisampling not supported for 3D textures");
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

    static MTLPrimitiveTopologyClass toMTL(RenderPrimitiveTopology topology) {
        switch (topology) {
            case RenderPrimitiveTopology::POINT_LIST:
                return MTLPrimitiveTopologyClassPoint;
            case RenderPrimitiveTopology::LINE_LIST:
                return MTLPrimitiveTopologyClassLine;
            case RenderPrimitiveTopology::TRIANGLE_LIST:
                return MTLPrimitiveTopologyClassTriangle;
            default:
                assert(false && "Unknown primitive topology type.");
                return MTLPrimitiveTopologyClassPoint;
        }
    }

    static MTLVertexStepFunction toMTL(RenderInputSlotClassification classification) {
        switch (classification) {
            case RenderInputSlotClassification::PER_VERTEX_DATA:
                return MTLVertexStepFunctionPerVertex;
            case RenderInputSlotClassification::PER_INSTANCE_DATA:
                return MTLVertexStepFunctionPerInstance;
            default:
                assert(false && "Unknown input classification.");
                return MTLVertexStepFunctionPerVertex;
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

    static MTLResourceOptions toMTL(RenderHeapType heapType) {
        switch (heapType) {
            case RenderHeapType::DEFAULT:
                return MTLResourceStorageModePrivate;
            case RenderHeapType::UPLOAD:
                return MTLStorageModeShared;
            case RenderHeapType::READBACK:
                return MTLStorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTLResourceStorageModePrivate;
        }
    }

    static MTLClearColor toClearColor(RenderColor color) {
        return MTLClearColorMake(color.r, color.g, color.b, color.a);
    }

    // MetalBuffer

    MetalBuffer::MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;

        // TODO: Set the right buffer options
        if (pool != nullptr) {
            this->buffer = [pool->heap newBufferWithLength: desc.size options: toMTL(desc.heapType)];
        } else {
            this->buffer = [device->device newBufferWithLength: desc.size options: toMTL(desc.heapType)];
        }
    }

    MetalBuffer::~MetalBuffer() {
        // TODO: ARC should handle this
    }

    void* MetalBuffer::map(uint32_t subresource, const RenderRange* readRange) {
        return [buffer contents];
    }

    void MetalBuffer::unmap(uint32_t subresource, const RenderRange* writtenRange) {
        // For managed buffers, we need to notify Metal about changes
        if (writtenRange && [buffer storageMode] == MTLStorageModeManaged) {
            NSRange range = {
                writtenRange->begin,
                writtenRange->end - writtenRange->begin
            };
            [buffer didModifyRange:range];
        }
    }

    std::unique_ptr<RenderBufferFormattedView> MetalBuffer::createBufferFormattedView(RenderFormat format) {
        return std::make_unique<MetalBufferFormattedView>(this, format);
    }

    void MetalBuffer::setName(const std::string &name) {
        [this->buffer setLabel: [NSString stringWithUTF8String: name.c_str()]];
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
        auto textureType = toTextureType(desc.dimension, desc.multisampling.sampleCount);

        descriptor.textureType = textureType;
        descriptor.storageMode = MTLStorageModePrivate;
        descriptor.pixelFormat = toMTL(desc.format);
        descriptor.width = desc.width;
        descriptor.height = desc.height;
        descriptor.depth = desc.depth;
        descriptor.mipmapLevelCount = desc.mipLevels;
        descriptor.arrayLength = 1;
        descriptor.sampleCount = desc.multisampling.sampleCount;
        descriptor.usage |= (desc.flags & (RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::DEPTH_TARGET)) ? MTLTextureUsageRenderTarget : MTLTextureUsageUnknown;
        descriptor.usage |= (desc.flags & (RenderTextureFlag::UNORDERED_ACCESS)) ? MTLTextureUsageShaderWrite : MTLTextureUsageUnknown;

        if (pool != nullptr) {
            this->mtlTexture = [pool->heap newTextureWithDescriptor: descriptor];
        } else {
            this->mtlTexture = [device->device newTextureWithDescriptor: descriptor];
        }
    }

    MetalTexture::~MetalTexture() {
        // TODO: Should be handled by ARC
    }

    std::unique_ptr<RenderTextureView> MetalTexture::createTextureView(const RenderTextureViewDesc &desc) {
        return std::make_unique<MetalTextureView>(this, desc);
    }

    void MetalTexture::setName(const std::string &name) {
        [this->mtlTexture setLabel: [NSString stringWithUTF8String: name.c_str()]];
    }

    // MetalTextureView

    MetalTextureView::MetalTextureView(MetalTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);
        // TODO: Validate levels and slices
        this->mtlTexture = [texture->mtlTexture
                            newTextureViewWithPixelFormat: toMTL(desc.format)
                            textureType: texture->mtlTexture.textureType
                            levels: NSMakeRange(desc.mipSlice, desc.mipLevels)
                            slices: NSMakeRange(0, 1)
        ];
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

    // MetalPool

    MetalPool::MetalPool(MetalDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLHeapDescriptor *descriptor = [MTLHeapDescriptor new];
        // TODO: Set Descriptor properties correctly
        [descriptor setType: MTLHeapTypeAutomatic];

        this->heap = [device->device newHeapWithDescriptor: descriptor];
    }

    MetalPool::~MetalPool() {
        // TODO: Should be handled by ARC
    }

    std::unique_ptr<RenderBuffer> MetalPool::createBuffer(const RenderBufferDesc &desc) {
        return std::make_unique<MetalBuffer>(device, this, desc);
    }

    std::unique_ptr<RenderTexture> MetalPool::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<MetalTexture>(device, this, desc);
    }

    // MetalPipelineLayout

    MetalPipelineLayout::MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->setCount = desc.descriptorSetDescsCount;

        // TODO: Make sure push constant ranges are laid out correctly

        uint32_t totalPushConstantSize = 0;
        for (uint32_t i = 0; i < desc.pushConstantRangesCount; i++) {
            RenderPushConstantRange range = desc.pushConstantRanges[i];
            pushConstantRanges.push_back(range);
            totalPushConstantSize += range.size;
        }

        if (totalPushConstantSize > 0) {
            size_t alignedSize = calculateAlignedSize(totalPushConstantSize);
            pushConstantsBuffer = [device->device newBufferWithLength: alignedSize options: MTLResourceStorageModeShared];
        }
        
        setCount = desc.descriptorSetDescsCount;

        // Create Descriptor Set Layouts
        for (uint32_t i = 0; i < desc.descriptorSetDescsCount; i++) {
            const RenderDescriptorSetDesc &setDesc = desc.descriptorSetDescs[i];
            setLayoutHandles.emplace_back(new MetalDescriptorSetLayout(device, setDesc));
        }
    }

    MetalDescriptorSetLayout::MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);
        this->device = device;

        // Pre-allocate vectors with known size
        const uint32_t totalDescriptors = desc.descriptorRangesCount + (desc.lastRangeIsBoundless ? desc.boundlessRangeSize : 0);
        descriptorTypes.reserve(totalDescriptors);
        descriptorIndexBases.reserve(totalDescriptors);
        descriptorRangeBinding.reserve(totalDescriptors);
        
        argumentDescriptors = [[NSMutableArray alloc] initWithCapacity:desc.descriptorRangesCount];

        // First pass: Calculate descriptor bases and bindings
        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &range = desc.descriptorRanges[i];
            uint32_t indexBase = uint32_t(descriptorIndexBases.size());
            
            descriptorIndexBases.resize(descriptorIndexBases.size() + range.count, indexBase);
            descriptorRangeBinding.resize(descriptorRangeBinding.size() + range.count, range.binding);
        }

        // Sort ranges by binding due to how spirv-cross orders them
        std::vector<RenderDescriptorRange> sortedRanges(desc.descriptorRanges, desc.descriptorRanges + desc.descriptorRangesCount);
        std::sort(sortedRanges.begin(), sortedRanges.end(),
                  [](const RenderDescriptorRange &a, const RenderDescriptorRange &b) {
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
                    staticSamplers.push_back(sampler->samplerState);
                    samplerIndices.push_back(range.binding + j);
                }
            }

            // Create argument descriptor
            MTLArgumentDescriptor *argumentDesc = [[MTLArgumentDescriptor alloc] init];
            argumentDesc.dataType = toMTL(range.type);
            argumentDesc.index = range.binding;
            argumentDesc.arrayLength = range.count > 1 ? range.count : 0;
            
            if (range.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc.textureType = MTLTextureType2D;
            }

            [argumentDescriptors addObject:argumentDesc];
        }

        // Handle boundless range if present
        if (desc.lastRangeIsBoundless) {
            const RenderDescriptorRange &lastRange = desc.descriptorRanges[desc.descriptorRangesCount - 1];
            descriptorTypes.push_back(lastRange.type);
            entryCount += std::max(desc.boundlessRangeSize, 1U);

            MTLArgumentDescriptor *argumentDesc = [[MTLArgumentDescriptor alloc] init];
            argumentDesc.dataType = toMTL(lastRange.type);
            argumentDesc.index = lastRange.binding;
            argumentDesc.arrayLength = 8192; // Fixed upper bound for Metal
            
            if (lastRange.type == RenderDescriptorRangeType::TEXTURE) {
                argumentDesc.textureType = MTLTextureType2D;
            }

            [argumentDescriptors addObject:argumentDesc];
        }

        assert([argumentDescriptors count] > 0);
        descriptorTypeMaxIndex = descriptorTypes.empty() ? 0 : uint32_t(descriptorTypes.size() - 1);

        // Create and initialize argument encoder
        argumentEncoder = [device->device newArgumentEncoderWithArguments:argumentDescriptors];
        const size_t bufferLength = [argumentEncoder encodedLength];
        descriptorBuffer = [device->device newBufferWithLength:bufferLength options:MTLResourceStorageModeShared];
        
        [argumentEncoder setArgumentBuffer:descriptorBuffer offset:0];

        // Set static samplers
        for (size_t i = 0; i < staticSamplers.size(); i++) {
            [argumentEncoder setSamplerState:staticSamplers[i] atIndex:samplerIndices[i]];
        }
    }

    MetalPipelineLayout::~MetalPipelineLayout() {
        // TODO: Should be handled by ARC
    }

    // MetalShader

    MetalShader::MetalShader(MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) {
        assert(device != nullptr);
        assert(data != nullptr);
        assert(size > 0);
        assert(format == RenderShaderFormat::METAL);

        this->device = device;
        this->format = format;
        this->entryPointName = (entryPointName != nullptr) ? std::string(entryPointName) : std::string();

        NSError *error = nullptr;
        dispatch_data_t dispatchData = dispatch_data_create(data, size, dispatch_get_main_queue(), ^{});
        id<MTLLibrary> library = [device->device newLibraryWithData: dispatchData error: &error];

        if (error != nullptr) {
            fprintf(stderr, "MTLLibrary newLibraryWithData: failed with error %s.\n", [error.localizedDescription cStringUsingEncoding: NSUTF8StringEncoding]);
            return;
        }

        this->function = [library newFunctionWithName:[NSString stringWithUTF8String: entryPointName]];
    }

    MetalShader::~MetalShader() {
        // TODO: Should be handled by ARC
    }

    // MetalSampler

    MetalSampler::MetalSampler(MetalDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLSamplerDescriptor *descriptor = [MTLSamplerDescriptor new];
        descriptor.supportArgumentBuffers = true;
        descriptor.minFilter = toMTL(desc.minFilter);
        descriptor.magFilter = toMTL(desc.magFilter);
        descriptor.mipFilter = toMTL(desc.mipmapMode);
        descriptor.rAddressMode = toMTL(desc.addressU);
        descriptor.sAddressMode = toMTL(desc.addressV);
        descriptor.tAddressMode = toMTL(desc.addressW);
        descriptor.maxAnisotropy = desc.maxAnisotropy;
        descriptor.compareFunction = toMTL(desc.comparisonFunc);
        descriptor.lodMinClamp = desc.minLOD;
        descriptor.lodMaxClamp = desc.maxLOD;
        descriptor.borderColor = toMTL(desc.borderColor);

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
        descriptor.computeFunction = computeShader->function;
        descriptor.label = [NSString stringWithUTF8String: computeShader->entryPointName.c_str()];

        NSError *error = nullptr;
        this->state = [device->device newComputePipelineStateWithDescriptor: descriptor options: MTLPipelineOptionNone reflection: nil error: &error];

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newComputePipelineStateWithDescriptor: failed with error %s.\n", [error.localizedDescription cStringUsingEncoding: NSUTF8StringEncoding]);
            return;
        }
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
        descriptor.inputPrimitiveTopology = toMTL(desc.primitiveTopology);
        descriptor.rasterSampleCount = desc.multisampling.sampleCount;

        assert(desc.vertexShader != nullptr && "Cannot create a valid MTLRenderPipelineState without a vertex shader!");
        const auto *metalShader = static_cast<const MetalShader *>(desc.vertexShader);

        descriptor.vertexFunction = metalShader->function;

        MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];

        for (uint32_t i = 0; i < desc.inputSlotsCount; i++) {
            const RenderInputSlot &inputSlot = desc.inputSlots[i];
            MTLVertexBufferLayoutDescriptor *layoutDescriptor = [MTLVertexBufferLayoutDescriptor new];
            layoutDescriptor.stride = inputSlot.stride;
            layoutDescriptor.stepFunction = toMTL(inputSlot.classification);
            layoutDescriptor.stepRate = (layoutDescriptor.stepFunction == MTLVertexStepFunctionPerInstance) ? inputSlot.stride : 1;
            [vertexDescriptor.layouts setObject: layoutDescriptor atIndexedSubscript: i];
        }

        for (uint32_t i = 0; i < desc.inputElementsCount; i++) {
            const RenderInputElement &inputElement = desc.inputElements[i];
            MTLVertexAttributeDescriptor *attributeDescriptor = [MTLVertexAttributeDescriptor new];
            attributeDescriptor.offset = inputElement.alignedByteOffset;
            attributeDescriptor.bufferIndex = inputElement.slotIndex;
            attributeDescriptor.format = toVertexFormat(inputElement.format);
            [vertexDescriptor.attributes setObject: attributeDescriptor atIndexedSubscript: i];
        }

        descriptor.vertexDescriptor = vertexDescriptor;

        assert(desc.geometryShader == nullptr && "Metal does not support geometry shaders!");

        if (desc.pixelShader != nullptr) {
            const auto *pixelShader = static_cast<const MetalShader *>(desc.pixelShader);
            descriptor.fragmentFunction = pixelShader->function;
        }

        for (uint32_t i = 0; i < desc.renderTargetCount; i++) {
            MTLRenderPipelineColorAttachmentDescriptor *colorAttachmentDescriptor = [MTLRenderPipelineColorAttachmentDescriptor new];
            const RenderBlendDesc &blendDesc = desc.renderTargetBlend[i];
            colorAttachmentDescriptor.blendingEnabled = blendDesc.blendEnabled;
            colorAttachmentDescriptor.sourceRGBBlendFactor = toMTL(blendDesc.srcBlend);
            colorAttachmentDescriptor.destinationRGBBlendFactor = toMTL(blendDesc.dstBlend);
            colorAttachmentDescriptor.rgbBlendOperation = toMTL(blendDesc.blendOp);
            colorAttachmentDescriptor.sourceAlphaBlendFactor = toMTL(blendDesc.srcBlendAlpha);
            colorAttachmentDescriptor.destinationAlphaBlendFactor = toMTL(blendDesc.dstBlendAlpha);
            colorAttachmentDescriptor.alphaBlendOperation = toMTL(blendDesc.blendOpAlpha);
            colorAttachmentDescriptor.writeMask = blendDesc.renderTargetWriteMask;
            colorAttachmentDescriptor.pixelFormat = toMTL(desc.renderTargetFormat[i]);
            [descriptor.colorAttachments setObject: colorAttachmentDescriptor atIndexedSubscript: i];
        }

        descriptor.depthAttachmentPixelFormat = toMTL(desc.depthTargetFormat);
        descriptor.rasterSampleCount = desc.multisampling.sampleCount;

        // State variables, initialized here to be reused in encoder re-binding
        renderState = new MetalRenderState();

        MTLDepthStencilDescriptor *depthStencilDescriptor = [MTLDepthStencilDescriptor new];
        depthStencilDescriptor.depthWriteEnabled = desc.depthWriteEnabled;
        depthStencilDescriptor.depthCompareFunction = desc.depthEnabled ? toMTL(desc.depthFunction) : MTLCompareFunctionAlways;
        renderState->depthStencilState = [device->device newDepthStencilStateWithDescriptor: depthStencilDescriptor];
        renderState->cullMode = toMTL(desc.cullMode);
        renderState->depthClipMode = (desc.depthClipEnabled) ? MTLDepthClipModeClip : MTLDepthClipModeClamp;
        renderState->winding = MTLWindingClockwise;
        renderState->sampleCount = desc.multisampling.sampleCount;
        if (desc.multisampling.sampleCount > 1) {
            renderState->samplePositions = new MTLSamplePosition[desc.multisampling.sampleCount];
            for (uint32_t i = 0; i < desc.multisampling.sampleCount; i++) {
                renderState->samplePositions[i].x = desc.multisampling.sampleLocations[i].x;
                renderState->samplePositions[i].y = desc.multisampling.sampleLocations[i].y;
            }
        }

        NSError *error = nullptr;
        this->state = [device->device newRenderPipelineStateWithDescriptor: descriptor error: &error];
        renderState->renderPipelineState = state;

        if (error != nullptr) {
            fprintf(stderr, "MTLDevice newRenderPipelineStateWithDescriptor: failed with error %s.\n", [error.localizedDescription cStringUsingEncoding: NSUTF8StringEncoding]);
            return;
        }
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline() {
        // TODO: Should be handled by ARC
    }

    RenderPipelineProgram MetalGraphicsPipeline::getProgram(const std::string &name) const {
        assert(false && "Graphics pipelines can't retrieve shader programs.");
        return RenderPipelineProgram();
    }

    // MetalDescriptorSet

    MetalDescriptorSet::MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);
        this->device = device;

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

        this->device = device;
        this->entryCount = entryCount;
    }

    MetalDescriptorSet::~MetalDescriptorSet() {
        // TODO: Should be handled by ARC
    }

    void MetalDescriptorSet::setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) {
        // TODO: Unimplemented.
    }

    void MetalDescriptorSet::setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) {
        const MetalTexture *interfaceTexture = static_cast<const MetalTexture *>(texture);
        const auto nativeResource = (interfaceTexture != nullptr) ? interfaceTexture->mtlTexture : nil;
        uint32_t descriptorIndexClamped = std::min(descriptorIndex, descriptorTypeMaxIndex);
        RenderDescriptorRangeType descriptorType = descriptorTypes[descriptorIndexClamped];
        switch (descriptorType) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE: {
                if (textureView != nullptr) {
                    const MetalTextureView *interfaceTextureView = static_cast<const MetalTextureView *>(textureView);
                    indicesToTextures[descriptorIndex] = interfaceTextureView->mtlTexture;
                } else {
                    indicesToTextures[descriptorIndex] = interfaceTexture->mtlTexture;
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
        // TODO: Unimplemented.
    }

    void MetalDescriptorSet::setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) {
        // TODO: Unimplemented.
    }

    // MetalSwapChain

    MetalSwapChain::MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) {
        this->layer = renderWindow.view;
        CAMetalLayer *metalLayer = (__bridge CAMetalLayer *)layer;
        [metalLayer setDevice:commandQueue->device->device];

        this->commandQueue = commandQueue;

        metalLayer.pixelFormat = toMTL(format);

        // We use only 1 proxy texture for the swap chain, and fetch
        // the next drawable from the layer's pool when needed.
        this->textureCount = 1;
        this->proxyTexture = new MetalTexture();
        this->proxyTexture->parentSwapChain = this;
        this->proxyTexture->desc.flags = RenderTextureFlag::RENDER_TARGET;

        this->renderWindow = renderWindow;
        getWindowSize(width, height);
    }

    MetalSwapChain::~MetalSwapChain() {
        // TODO: Should be handled by ARC
        delete proxyTexture;
    }

    bool MetalSwapChain::present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) {
        assert(layer != nil && "Cannot present without a valid layer.");
        assert(drawable != nil && "Cannot present without a valid drawable.");

        [commandQueue->buffer presentDrawable: drawable];
        return true;
    }

    bool MetalSwapChain::resize() {
        getWindowSize(width, height);

        if ((width == 0) || (height == 0)) {
            return false;
        }

        drawable = nil;
        return true;
    }

    bool MetalSwapChain::needsResize() const {
        uint32_t windowWidth, windowHeight;
        getWindowSize(windowWidth, windowHeight);
        return (layer == nullptr) || (width != windowWidth) || (height != windowHeight);
    }

    void MetalSwapChain::setVsyncEnabled(bool vsyncEnabled) {
        // TODO: Unimplemented.
    }

    bool MetalSwapChain::isVsyncEnabled() const {
        // TODO: Unimplemented.
        return false;
    }

    uint32_t MetalSwapChain::getWidth() const {
        return width;
    }

    uint32_t MetalSwapChain::getHeight() const {
        return height;
    }

    RenderTexture *MetalSwapChain::getTexture(uint32_t textureIndex) {
        return proxyTexture;
    }

    bool MetalSwapChain::acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
        assert(signalSemaphore != nullptr);

        // Ignore textureIndex for Metal in both acquireTexture and getTexture.
        // Metal will always return the next available texture from the layer's pool.
        *textureIndex = 0;
        auto nextDrawable = [(__bridge CAMetalLayer *)layer nextDrawable];
        if (nextDrawable != nil) {
            drawable = nextDrawable;
            return true;
        }
        return false;
    }

    uint32_t MetalSwapChain::getTextureCount() const {
        return textureCount;
    }

    RenderWindow MetalSwapChain::getWindow() const {
        return renderWindow;
    }

    bool MetalSwapChain::isEmpty() const {
        return (layer == nullptr) || (width == 0) || (height == 0);
    }

    uint32_t MetalSwapChain::getRefreshRate() const {
        NSWindow *window = (__bridge NSWindow *)renderWindow.window;
        NSScreen *screen = [window screen];

        if (@available(macOS 12.0, *)) {
            return (int)[screen maximumFramesPerSecond];
        }

        // TODO: Implement this.
        return 0;
    }

    void MetalSwapChain::getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const {
        NSWindow *window = (__bridge NSWindow *)renderWindow.window;
        NSRect contentFrame = [[window contentView] frame];
        dstWidth = contentFrame.size.width;
        dstHeight = contentFrame.size.height;
    }

    // MetalFramebuffer

    MetalFramebuffer::MetalFramebuffer(MetalDevice *device, const RenderFramebufferDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        depthAttachmentReadOnly = desc.depthAttachmentReadOnly;

        for (uint32_t i = 0; i < desc.colorAttachmentsCount; i++) {
            const auto *colorAttachment = static_cast<const MetalTexture *>(desc.colorAttachments[i]);
            assert((colorAttachment->desc.flags & RenderTextureFlag::RENDER_TARGET) && "Color attachment must be a render target.");
            colorAttachments.emplace_back(colorAttachment);

            if (i == 0) {
                if (colorAttachment->parentSwapChain != nullptr) {
                    width = colorAttachment->parentSwapChain->getWidth();
                    height = colorAttachment->parentSwapChain->getHeight();
                } else {
                    width = colorAttachment->desc.width;
                    height = colorAttachment->desc.height;
                }
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
    }

    MetalFramebuffer::~MetalFramebuffer() {
        // TODO: Should be handled by ARC
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
        // TODO: Should be handled by ARC
    }

    void MetalCommandList::begin() { }

    void MetalCommandList::end() {
        endEncoder(true);
    }

    void MetalCommandList::endEncoder(bool clearDescs) {
        clearDrawCalls();
        if (renderEncoder != nil) {
            [renderEncoder endEncoding];
            if (clearDescs) {
                renderDescriptor = nil;
            }
        }

        if (computeEncoder != nil) {
            [computeEncoder endEncoding];
        }

        if (blitEncoder != nil) {
            [blitEncoder endEncoding];
        }

        renderEncoder = nil;
        computeEncoder = nil;
        blitEncoder = nil;
    }

    void MetalCommandList::clearDrawCalls() {
        if (!deferredDrawCalls.empty()) {
            guaranteeRenderEncoder();
        }
        for (auto &drawCall : deferredDrawCalls) {
            if (drawCall.type == DrawCall::Type::Draw) {
                [renderEncoder drawPrimitives: drawCall.primitiveType
                                  vertexStart: drawCall.startVertexLocation
                                  vertexCount: drawCall.vertexCountPerInstance
                                instanceCount: drawCall.instanceCount
                                 baseInstance: drawCall.startInstanceLocation];
            } else if (drawCall.type == DrawCall::Type::DrawIndexed) {
                [renderEncoder drawIndexedPrimitives: drawCall.primitiveType
                                          indexCount: drawCall.indexCountPerInstance
                                           indexType: drawCall.indexType
                                         indexBuffer: drawCall.indexBuffer
                                   indexBufferOffset: drawCall.startIndexLocation
                                       instanceCount: drawCall.instanceCount
                                          baseVertex: drawCall.baseVertexLocation
                                        baseInstance: drawCall.startInstanceLocation];
            }
        }

        deferredDrawCalls.clear();
    }

    void MetalCommandList::guaranteeRenderDescriptor(bool forClearColor) {
        if (forClearColor) {
            if (clearRenderDescriptor == nil) {
                clearRenderDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                configureRenderDescriptor(clearRenderDescriptor);
            }
        } else {
            if (renderDescriptor == nil) {
                renderDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                configureRenderDescriptor(renderDescriptor);
                
                if (activeRenderState->samplePositions != nullptr) {
                    [renderDescriptor setSamplePositions:activeRenderState->samplePositions count:activeRenderState->sampleCount];
                }
            }
        }
    }

    void MetalCommandList::configureRenderDescriptor(MTLRenderPassDescriptor* renderDescriptor) {
        assert(targetFramebuffer != nullptr && "Cannot encode render commands without a target framebuffer");
        colorAttachmentsCount = targetFramebuffer->colorAttachments.size();
        
        for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); i++) {
            auto *colorAttachment = renderDescriptor.colorAttachments[i];
            // If framebuffer was created using a swap chain, use the drawable's texture
            if (i == 0 && targetFramebuffer->colorAttachments[0]->parentSwapChain != nullptr) {
                assert(targetFramebuffer->colorAttachments.size() == 1 && "Swap chain framebuffers must have exactly one color attachment.");
                
                MetalSwapChain *swapChain = targetFramebuffer->colorAttachments[0]->parentSwapChain;
                colorAttachment.texture = swapChain->drawable.texture;
                colorAttachment.loadAction = MTLLoadActionLoad;
                colorAttachment.storeAction = MTLStoreActionStore;
            } else {
                colorAttachment.texture = targetFramebuffer->colorAttachments[i]->mtlTexture;
                colorAttachment.loadAction = MTLLoadActionLoad;
                colorAttachment.storeAction = MTLStoreActionStore;
            }
            
            if (resolveTo.count(colorAttachment.texture) != 0) {
                colorAttachment.resolveTexture = resolveTo[colorAttachment.texture];
                colorAttachment.storeAction = MTLStoreActionMultisampleResolve;
            }
        }
        
        if (targetFramebuffer->depthAttachment != nullptr) {
            auto *depthAttachment = renderDescriptor.depthAttachment;
            depthAttachment.texture = targetFramebuffer->depthAttachment->mtlTexture;
            depthAttachment.loadAction = MTLLoadActionLoad;
            depthAttachment.storeAction = MTLStoreActionStore;
            
            if (resolveTo.count(depthAttachment.texture) != 0) {
                depthAttachment.resolveTexture = resolveTo[depthAttachment.texture];
                depthAttachment.storeAction = MTLStoreActionMultisampleResolve;
            }
            
            if (depthClearValue >= 0.0) {
                depthAttachment.loadAction = MTLLoadActionClear;
                depthAttachment.clearDepth = depthClearValue;
            }
        }
    }

    void MetalCommandList::guaranteeRenderEncoder() {
        if (renderEncoder == nil) {
            guaranteeRenderDescriptor(false);
            renderEncoder = [queue->buffer renderCommandEncoderWithDescriptor: renderDescriptor];

            [renderEncoder setRenderPipelineState: activeRenderState->renderPipelineState];
            [renderEncoder setDepthStencilState: activeRenderState->depthStencilState];
            [renderEncoder setDepthClipMode: activeRenderState->depthClipMode];
            [renderEncoder setCullMode: activeRenderState->cullMode];
            [renderEncoder setFrontFacingWinding: activeRenderState->winding];

            [renderEncoder setViewports: viewportVector.data() count: viewportVector.size()];
            [renderEncoder setScissorRects: scissorVector.data() count: scissorVector.size()];

            for (uint32_t i = 0; i < viewCount; i++) {
                [renderEncoder setVertexBuffer: vertexBuffers[i]
                                        offset: vertexBufferOffsets[i]
                                       atIndex: vertexBufferIndices[i]];
            }

            // Encode Descriptor set layouts and mark resources
            for (uint32_t i = 0; i < activeGraphicsPipelineLayout->setCount; i++) {
                const auto *setLayout = activeGraphicsPipelineLayout->setLayoutHandles[i];

                if (indicesToRenderDescriptorSets.count(i) != 0) {
                    const auto *descriptorSet = indicesToRenderDescriptorSets[i];
                    // Mark resources in the argument buffer as resident
                    for (const auto& pair : descriptorSet->indicesToTextures) {
                        uint32_t index = pair.first;
                        auto *texture = pair.second;
                        if (texture != nil) {
                            [renderEncoder useResource:texture usage:MTLResourceUsageRead stages:MTLRenderStageFragment|MTLRenderStageVertex];

                            uint32_t adjustedIndex = index - setLayout->descriptorIndexBases[index] + setLayout->descriptorRangeBinding[index];
                            [setLayout->argumentEncoder setTexture:texture atIndex: adjustedIndex];
                        }
                    }

                    // TODO: Mark and bind buffers
                }

                [renderEncoder setFragmentBuffer:setLayout->descriptorBuffer offset:0 atIndex:i];
            }

            if (graphicsPushConstantsBuffer != nil) {
                uint32_t pushConstantsIndex = activeGraphicsPipelineLayout->setCount;
                [renderEncoder setFragmentBuffer: graphicsPushConstantsBuffer
                                          offset: 0
                                         atIndex: pushConstantsIndex];
            }
        }
    }

    void MetalCommandList::guaranteeComputeEncoder() {
        if (computeEncoder == nil) {
            endEncoder(false);

            auto computeDescriptor = [MTLComputePassDescriptor new];
            computeEncoder = [queue->buffer computeCommandEncoderWithDescriptor: computeDescriptor];
        }
    }

    void MetalCommandList::guaranteeBlitEncoder() {
        if (blitEncoder == nil) {
            endEncoder(false);

            auto blitDescriptor = [MTLBlitPassDescriptor new];
            blitEncoder = [queue->buffer blitCommandEncoderWithDescriptor: blitDescriptor];
        }
    }

    void MetalCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        // TODO: Ignore for now, Metal should handle most of this itself.
    }

    void MetalCommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
        guaranteeComputeEncoder();
        assert(computeEncoder != nil && "Cannot encode dispatch on nil MTLComputeCommandEncoder!");
        
        [computeEncoder dispatchThreadgroups:MTLSizeMake(threadGroupCountX, threadGroupCountY, threadGroupCountZ)
                      threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    }

    void MetalCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        // TODO: Support Metal RT
    }

    void MetalCommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
//        guaranteeRenderEncoder();
//        assert(renderEncoder != nil && "Cannot encode draw on nil MTLRenderCommandEncoder!");

//        [renderEncoder drawPrimitives: currentPrimitiveType
//                          vertexStart: startVertexLocation
//                          vertexCount: vertexCountPerInstance
//                        instanceCount: instanceCount
//                         baseInstance: startInstanceLocation];

        deferredDrawCalls.emplace_back(DrawCall{
            .type = DrawCall::Type::Draw,
            .startVertexLocation = startVertexLocation,
            .vertexCountPerInstance = vertexCountPerInstance,
            .instanceCount = instanceCount,
            .startInstanceLocation = startInstanceLocation
        });
    }

    void MetalCommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
//        guaranteeRenderEncoder();
//        assert(renderEncoder != nil && "Cannot encode draw on nil MTLRenderCommandEncoder!");

//        [renderEncoder drawIndexedPrimitives: currentPrimitiveType
//                                  indexCount: indexCountPerInstance
//                                   indexType: currentIndexType
//                                 indexBuffer: indexBuffer
//                           indexBufferOffset: startIndexLocation
//                               instanceCount: instanceCount
//                                  baseVertex: baseVertexLocation
//                                baseInstance: startInstanceLocation];

        deferredDrawCalls.emplace_back(DrawCall{
            .type = DrawCall::Type::DrawIndexed,
            .instanceCount = instanceCount,
            .startInstanceLocation = startInstanceLocation,
            .indexCountPerInstance = indexCountPerInstance,
            .indexBuffer = indexBuffer,
            .baseVertexLocation = baseVertexLocation,
            .startIndexLocation = startIndexLocation,
        });
    }

    void MetalCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const auto *interfacePipeline = static_cast<const MetalPipeline *>(pipeline);
        switch (interfacePipeline->type) {
            case MetalPipeline::Type::Compute: {
                guaranteeComputeEncoder();
                const auto *computePipeline = static_cast<const MetalComputePipeline *>(interfacePipeline);
                assert(computeEncoder != nil && "Cannot set pipeline state on nil MTLComputeCommandEncoder!");
                [computeEncoder setComputePipelineState: computePipeline->state];
                break;
            }
            case MetalPipeline::Type::Graphics: {
                const auto *graphicsPipeline = static_cast<const MetalGraphicsPipeline *>(interfacePipeline);
                activeRenderState = graphicsPipeline->renderState;
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
        setDescriptorSet(descriptorSet, setIndex, true);
    }

    void MetalCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const auto oldLayout = activeGraphicsPipelineLayout;
        activeGraphicsPipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeGraphicsPipelineLayout) {
            indicesToComputeDescriptorSets.clear();
            indicesToRenderDescriptorSets.clear();
            graphicsPushConstantsBuffer = nil;
        }
    }

    void MetalCommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void *data) {
        assert(activeGraphicsPipelineLayout != nullptr);
        assert(rangeIndex < activeGraphicsPipelineLayout->pushConstantRanges.size());

        // TODO: make sure there's parity with Vulkan
        const RenderPushConstantRange &range = activeGraphicsPipelineLayout->pushConstantRanges[rangeIndex];
        uint32_t startOffset = 0;
        for (uint32_t i = 0; i < rangeIndex; i++) {
            startOffset += activeGraphicsPipelineLayout->pushConstantRanges[i].size;
        }
        auto bufferContents = (uint8_t *)[activeGraphicsPipelineLayout->pushConstantsBuffer contents];
        memcpy(bufferContents + startOffset, data, range.size);

        graphicsPushConstantsBuffer = activeGraphicsPipelineLayout->pushConstantsBuffer;
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
            indexBuffer = interfaceBuffer->buffer;
        }
    }

    void MetalCommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
        if ((views != nullptr) && (viewCount > 0)) {
            assert(inputSlots != nullptr);

            this->viewCount = viewCount;
            vertexBuffers.clear();
            vertexBufferOffsets.clear();
            vertexBufferIndices.clear();

            for (uint32_t i = 0; i < viewCount; i++) {
                const MetalBuffer *interfaceBuffer = static_cast<const MetalBuffer *>(views[i].buffer.ref);
                vertexBuffers.emplace_back(interfaceBuffer->buffer);
                vertexBufferOffsets.emplace_back(views[i].buffer.offset);
                vertexBufferIndices.emplace_back(startSlot + i);
            }
        }
    }

    void MetalCommandList::setViewports(const RenderViewport *viewports, uint32_t count) {
        viewportVector.clear();

        for (uint32_t i = 0; i < count; i++) {
            viewportVector.emplace_back(MTLViewport {
                    viewports[i].x,
                    viewports[i].y,
                    viewports[i].width,
                    viewports[i].height,
                    viewports[i].minDepth,
                    viewports[i].maxDepth
            });
        }
    }

    void MetalCommandList::setScissors(const RenderRect *scissorRects, uint32_t count) {
        scissorVector.clear();

        for (uint32_t i = 0; i < count; i++) {
            scissorVector.emplace_back(MTLScissorRect {
                    uint32_t(scissorRects[i].left),
                    uint32_t(scissorRects[i].top),
                    uint32_t(scissorRects[i].right - scissorRects[i].left),
                    uint32_t(scissorRects[i].bottom - scissorRects[i].top)
            });
        }
    }

    void MetalCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {
        endEncoder(true);
        // Need to clear explicitly to remove from new descriptor
        depthClearValue = -1.0;
        if (framebuffer != nullptr) {
            targetFramebuffer = static_cast<const MetalFramebuffer *>(framebuffer);
        } else {
            targetFramebuffer = nullptr;
        }
    }

    void MetalCommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        setupClearPipeline();
        guaranteeRenderDescriptor(true);
        assert(clearRenderDescriptor != nil && "Cannot encode clear on nil MTLRenderPassDescriptor!");
        assert(clearPipelineState != nil && "Cannot encode clear without a valid pipeline state!");

        id <MTLRenderCommandEncoder> renderEncoder = [queue->buffer renderCommandEncoderWithDescriptor: clearRenderDescriptor];
        [renderEncoder setRenderPipelineState: clearPipelineState];
        
        // Convert clear color
        float clearColor[4] = {
            colorValue.r, colorValue.g, colorValue.b, colorValue.a
        };

        [renderEncoder setFragmentBytes:clearColor
                               length:sizeof(float) * 4
                              atIndex:0];
        
        if (clearRectsCount == 0) {
            // Get the drawable size
            uint32_t width = targetFramebuffer->getWidth();
            uint32_t height = targetFramebuffer->getHeight();
            
            // Full screen clear
            MTLViewport viewport = {
                0, 0,
                (double)targetFramebuffer->width,
                (double)targetFramebuffer->height,
                0.0, 1.0
            };
            [renderEncoder setViewport:viewport];
            [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                              vertexStart:0
                              vertexCount:3];
        } else {
            // Clear individual rects
            for (uint32_t i = 0; i < clearRectsCount; i++) {
                const RenderRect& rect = clearRects[i];
                
                // Skip empty rects
                if (rect.isEmpty()) {
                    continue;
                }
                
                // Convert to x,y,width,height format
                double width = rect.right - rect.left;
                double height = rect.bottom - rect.top;
                
                MTLViewport viewport = {
                    (double)rect.left,    // x
                    (double)rect.top,     // y
                    width,                // width
                    height,              // height
                    0.0, 1.0
                };
                
                MTLScissorRect scissor = {
                    (NSUInteger)rect.left,    // x
                    (NSUInteger)rect.top,     // y
                    (NSUInteger)width,        // width
                    (NSUInteger)height        // height
                };
                
                [renderEncoder setViewport:viewport];
                [renderEncoder setScissorRect:scissor];
                [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                                  vertexStart:0
                                  vertexCount:3];
            }
        }
        
        [renderEncoder endEncoding];
        clearRenderDescriptor = nil;
    }

    void MetalCommandList::setupClearPipeline() {
        // Verify we have a valid framebuffer
        if (!targetFramebuffer || targetFramebuffer->colorAttachments.empty()) {
            NSLog(@"Cannot setup clear pipeline without valid framebuffer");
            return;
        }

        MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        
        // Set up shader functions
        NSString *shaderSource = @"#include <metal_stdlib>\n"
                                "using namespace metal;\n"
                                "\n"
                                "vertex float4 clearVert(uint vid [[vertex_id]]) {\n"
                                "    const float2 positions[] = {\n"
                                "        float2(-1, -1),\n"
                                "        float2(3, -1),\n"
                                "        float2(-1, 3)\n"
                                "    };\n"
                                "    return float4(positions[vid], 0, 1);\n"
                                "}\n"
                                "\n"
                                "fragment float4 clearFrag(constant float4& clearColor [[buffer(0)]]) {\n"
                                "    return clearColor;\n"
                                "}\n";

        NSError *error = nil;
        MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
        id<MTLLibrary> library = [device->device newLibraryWithSource:shaderSource
                                                    options:options
                                                      error:&error];
        if (!library) {
            NSLog(@"Failed to create library: %@", error);
            return;
        }
            
        pipelineDesc.vertexFunction = [library newFunctionWithName:@"clearVert"];
        pipelineDesc.fragmentFunction = [library newFunctionWithName:@"clearFrag"];
        
        NSUInteger sampleCount = targetFramebuffer->colorAttachments[0]->desc.multisampling.sampleCount;
        pipelineDesc.rasterSampleCount = sampleCount;
        
        // Configure attachments
        colorAttachmentsCount = targetFramebuffer->colorAttachments.size();
        for (uint32_t i = 0; i < colorAttachmentsCount; i++) {
            MTLPixelFormat format;
            // Handle swapchain case specially
            if (i == 0 && targetFramebuffer->colorAttachments[0]->parentSwapChain != nullptr) {
                MetalSwapChain *swapChain = targetFramebuffer->colorAttachments[0]->parentSwapChain;
                format = swapChain->drawable.texture.pixelFormat;
            } else {
                format = toMTL(targetFramebuffer->colorAttachments[i]->desc.format);
            }
            
            pipelineDesc.colorAttachments[i].pixelFormat = format;
            pipelineDesc.colorAttachments[i].blendingEnabled = NO;
        }
        
        // Set depth format if there's a depth attachment
        if (targetFramebuffer->depthAttachment != nullptr) {
            pipelineDesc.depthAttachmentPixelFormat = targetFramebuffer->depthAttachment->mtlTexture.pixelFormat;
        }

        clearPipelineState = [device->device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!clearPipelineState) {
            NSLog(@"Failed to create pipeline state: %@", error);
            return;
        }
    }

    void MetalCommandList::clearDepth(bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        // TODO: Clear Color with rects, current impl only sets clear on the full attachment
        if (clearDepth) {
            depthClearValue = depthValue;
        }
    }

    void MetalCommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
        guaranteeBlitEncoder();
        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);
        assert(blitEncoder != nil && "Cannot copy buffer region on a nil MTLBlitCommandEncoder");

        const auto interfaceDstBuffer = static_cast<const MetalBuffer *>(dstBuffer.ref);
        const auto interfaceSrcBuffer = static_cast<const MetalBuffer *>(srcBuffer.ref);

        [blitEncoder copyFromBuffer: interfaceSrcBuffer->buffer
                       sourceOffset: 0
                           toBuffer: interfaceDstBuffer->buffer
                  destinationOffset: 0
                               size: size];
    }

    void MetalCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation,
                                            const RenderTextureCopyLocation &srcLocation,
                                            uint32_t dstX, uint32_t dstY, uint32_t dstZ,
                                            const RenderBox *srcBox) {
        guaranteeBlitEncoder();
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(blitEncoder != nil && "Cannot copy texture region on a nil MTLBlitCommandEncoder");

        const auto dstTexture = static_cast<const MetalTexture *>(dstLocation.texture);
        const auto srcTexture = static_cast<const MetalTexture *>(srcLocation.texture);
        const auto dstBuffer = static_cast<const MetalBuffer *>(dstLocation.buffer);
        const auto srcBuffer = static_cast<const MetalBuffer *>(srcLocation.buffer);

        if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE &&
            srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            MTLOrigin dstOrigin = MTLOriginMake(dstX, dstY, dstZ);
            
            // Calculate actual dimensions based on format block size
            uint32_t blockWidth = RenderFormatBlockWidth(srcLocation.placedFootprint.format);
            uint32_t width = ((srcLocation.placedFootprint.width + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t height = ((srcLocation.placedFootprint.height + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t depth = srcLocation.placedFootprint.depth;
            
            MTLSize size = MTLSizeMake(width, height, depth);

            // Calculate proper bytes per row based on format
            uint32_t blockCount = (srcLocation.placedFootprint.rowWidth + blockWidth - 1) / blockWidth;
            uint32_t bytesPerRow = blockCount * RenderFormatSize(srcLocation.placedFootprint.format);
            
            // Calculate bytes per image (row pitch * height)
            uint32_t bytesPerImage = bytesPerRow * height;

            // Verify alignment requirements
            assert((srcLocation.placedFootprint.offset % 256) == 0 && "Buffer offset must be aligned");
            assert((bytesPerRow % 256) == 0 && "Bytes per row must be aligned");

            [blitEncoder copyFromBuffer: srcBuffer->buffer
                         sourceOffset: srcLocation.placedFootprint.offset
                    sourceBytesPerRow: bytesPerRow
                  sourceBytesPerImage: bytesPerImage
                           sourceSize: size
                            toTexture: dstTexture->mtlTexture
                     destinationSlice: dstLocation.subresource.index
                     destinationLevel: 0
                    destinationOrigin: dstOrigin];
        }
        else if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE &&
                 srcLocation.type == RenderTextureCopyType::SUBRESOURCE) {
            assert(dstTexture != nullptr);
            assert(srcTexture != nullptr);

            MTLOrigin srcOrigin;
            MTLSize size;

            if (srcBox != nullptr) {
                srcOrigin = MTLOriginMake(srcBox->left, srcBox->top, srcBox->front);
                size = MTLSizeMake(srcBox->right - srcBox->left,
                                 srcBox->bottom - srcBox->top,
                                 srcBox->back - srcBox->front);
            } else {
                srcOrigin = MTLOriginMake(0, 0, 0);
                size = MTLSizeMake(srcTexture->desc.width,
                                 srcTexture->desc.height,
                                 srcTexture->desc.depth);
            }

            MTLOrigin dstOrigin = MTLOriginMake(dstX, dstY, dstZ);

            [blitEncoder copyFromTexture: srcTexture->mtlTexture
                           sourceSlice: srcLocation.subresource.index
                           sourceLevel: 0
                          sourceOrigin: srcOrigin
                            sourceSize: size
                             toTexture: dstTexture->mtlTexture
                      destinationSlice: dstLocation.subresource.index
                      destinationLevel: 0
                     destinationOrigin: dstOrigin];
        }
        else {
            assert(false && "Unsupported texture copy type combination");
        }
    }

    void MetalCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        guaranteeBlitEncoder();
        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);
        assert(blitEncoder != nil && "Cannot copy buffer on nil MTLBlitCommandEncoder!");

        const auto dst = static_cast<const MetalBuffer *>(dstBuffer);
        const auto src = static_cast<const MetalBuffer *>(srcBuffer);

        [blitEncoder copyFromBuffer: src->buffer
                       sourceOffset: 0
                           toBuffer: dst->buffer
                  destinationOffset: 0
                               size: dst->desc.size];
    }

    void MetalCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        guaranteeBlitEncoder();
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);
        assert(blitEncoder != nil && "Cannot copy texture on nil MTLBlitCommandEncoder!");

        const auto dst = static_cast<const MetalTexture *>(dstTexture);
        const auto src = static_cast<const MetalTexture *>(srcTexture);

        [blitEncoder copyFromTexture: src->mtlTexture
                           toTexture: dst->mtlTexture];
    }

    void MetalCommandList::resolveTexture(const RT64::RenderTexture *dstTexture, const RT64::RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        const auto *interfaceDstTexture = static_cast<const MetalTexture *>(dstTexture);
        const auto *interfaceSrcTexture = static_cast<const MetalTexture *>(srcTexture);

        resolveTo[interfaceSrcTexture->mtlTexture] = interfaceDstTexture->mtlTexture;
    }

    void MetalCommandList::resolveTextureRegion(const RT64::RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RT64::RenderTexture *srcTexture, const RT64::RenderRect *srcRect) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::buildBottomLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, const RT64::RenderBottomLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::buildTopLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, RT64::RenderBufferReference instancesBuffer, const RT64::RenderTopLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::setDescriptorSet(RT64::RenderDescriptorSet *descriptorSet, uint32_t setIndex, bool setCompute) {
        auto *interfaceDescriptorSet = static_cast<MetalDescriptorSet *>(descriptorSet);
        if (setCompute) {
            indicesToComputeDescriptorSets[setIndex] = interfaceDescriptorSet;
        } else {
            indicesToRenderDescriptorSets[setIndex] = interfaceDescriptorSet;
        }
    }

    // MetalCommandFence

    MetalCommandFence::MetalCommandFence(MetalDevice *device) {
        // TODO: Unimplemented and probably unnecessary.
    }

    MetalCommandFence::~MetalCommandFence() {
        // TODO: Should be handled by ARC
    }

    // MetalCommandSemaphore

    MetalCommandSemaphore::MetalCommandSemaphore(MetalDevice *device) {
        // TODO: Unimplemented
    }

    MetalCommandSemaphore::~MetalCommandSemaphore() {
        // TODO: Unimplemented.
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

    std::unique_ptr<RenderSwapChain> MetalCommandQueue::createSwapChain(RT64::RenderWindow renderWindow, uint32_t bufferCount, RenderFormat format) {
        return std::make_unique<MetalSwapChain>(this, renderWindow, bufferCount, format);
    }

    void MetalCommandQueue::executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) {
        assert(commandLists != nullptr);
        assert(commandListCount > 0);

        [buffer enqueue];
        [buffer commit];

        this->buffer = [buffer.commandQueue commandBuffer];
    }

    void MetalCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        // TODO: Should be handled by hazard tracking.
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
        description.name = "Metal";
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
        if ([device supportsTextureSampleCount:8]) {
            return 8;
        }
        else if ([device supportsTextureSampleCount:4]) {
            return 4;
        }
        else if ([device supportsTextureSampleCount:2]) {
            return 2;
        }

        return 1;
    }

    void MetalDevice::release() {
        // TODO: Automatic reference counting should take care of this?
    }

    bool MetalDevice::isValid() const {
        return device != nil;
    }

    // MetalInterface

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
