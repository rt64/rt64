//
// Created by David Chavez on 5/13/24.
//

#include "rt64_metal.h"

namespace RT64 {
    MTLPixelFormat toMtl(RenderFormat format) {
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

    static MTLCullMode toMtl(RenderCullMode cullMode) {
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

    static MTLPrimitiveType toMtl(RenderPrimitiveTopology topology) {
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

    static MTLBlendFactor toMtl(RenderBlend blend) {
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

    static MTLBlendOperation toMtl(RenderBlendOperation operation) {
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

    static MTLCompareFunction toMtl(RenderComparisonFunction function) {
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

    static MTLSamplerMinMagFilter toMtl(RenderFilter filter) {
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

    static MTLSamplerMipFilter toMtl(RenderMipmapMode mode) {
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

    static MTLSamplerAddressMode toMtl(RenderTextureAddressMode mode) {
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

    static MTLSamplerBorderColor toMtl(RenderBorderColor color) {
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

    // MetalTexture

    MetalTexture::MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc) {
        assert(device != nullptr);

        this->device = device;
        this->pool = pool;
        this->desc = desc;

        this->descriptor = [MTLTextureDescriptor new];

        [this->descriptor setTextureType: toTextureType(desc.dimension)];
        [this->descriptor setPixelFormat: toMtl(desc.format)];
        [this->descriptor setWidth: desc.width];
        [this->descriptor setHeight: desc.height];
        [this->descriptor setDepth: desc.depth];
        [this->descriptor setMipmapLevelCount: desc.mipLevels];
        [this->descriptor setArrayLength: 1];
        [this->descriptor setSampleCount: desc.multisampling.sampleCount];
        // TODO: Usage flags
    }

    MetalTexture::~MetalTexture() {
        // TODO: Should be handled by ARC
    }

    // MetalTextureView

    MetalTextureView::MetalTextureView(MetalTexture *texture, const RenderTextureViewDesc &desc) {
        assert(texture != nullptr);

        this->texture = texture;

        this->mtlTexture = [texture->device->renderInterface->device newTextureWithDescriptor: texture->descriptor];
    }

    MetalTextureView::~MetalTextureView() {
        // TODO: Should be handled by ARC
    }

    // MetalSampler

    MetalSampler::MetalSampler(MetalDevice *device, const RenderSamplerDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLSamplerDescriptor *descriptor = [MTLSamplerDescriptor new];
        [descriptor setMinFilter: toMtl(desc.minFilter)];
        [descriptor setMagFilter: toMtl(desc.magFilter)];
        [descriptor setMipFilter: toMtl(desc.mipmapMode)];
        [descriptor setRAddressMode: toMtl(desc.addressU)];
        [descriptor setSAddressMode: toMtl(desc.addressV)];
        [descriptor setTAddressMode: toMtl(desc.addressW)];
        [descriptor setMaxAnisotropy: desc.maxAnisotropy];
        [descriptor setCompareFunction: toMtl(desc.comparisonFunc)];
        [descriptor setLodMinClamp: desc.minLOD];
        [descriptor setLodMaxClamp: desc.maxLOD];
        [descriptor setBorderColor: toMtl(desc.borderColor)];

        this->samplerState = [device->renderInterface->device newSamplerStateWithDescriptor: descriptor];
    }

    MetalSampler::~MetalSampler() {
        // TODO: Should be handled by ARC
    }

    // MetalPipeline

    MetalPipeline::MetalPipeline(RT64::MetalDevice *device, RT64::MetalPipeline::Type type) {
        assert(device != nullptr);
        assert(type != Type::Unknown);

        this->device = device;
        this->type = type;
    }

    MetalPipeline::~MetalPipeline() { }

    // MetalPool

    MetalPool::MetalPool(MetalDevice *device, const RenderPoolDesc &desc) {
        assert(device != nullptr);

        this->device = device;

        MTLHeapDescriptor *descriptor = [MTLHeapDescriptor new];
        // TODO: Set Descriptor properties correctly
        [descriptor setType: MTLHeapTypeAutomatic];

        this->heap = [device->renderInterface->device newHeapWithDescriptor: descriptor];
    }

    // MetalDevice

    MetalDevice::MetalDevice(MetalInterface *renderInterface) {
        assert(renderInterface != nullptr);
        this->renderInterface = renderInterface;

        // Fill capabilities.
        // TODO: Let's add ray tracing as a second step
//        capabilities.raytracing = [this->renderInterface->device supportsFamily:MTLGPUFamilyApple9];
        capabilities.maxTextureSize = 16384;
        capabilities.sampleLocations = [this->renderInterface->device areProgrammableSamplePositionsSupported];
        capabilities.descriptorIndexing = true;
        // TODO: check if this came after MacFamily2
        capabilities.scalarBlockLayout = true;
        capabilities.presentWait = true;
    }

    MetalDevice::~MetalDevice() {
        // TODO: Automatic reference counting should take care of this.
    }

    std::unique_ptr<RenderCommandList> MetalDevice::createCommandList(RenderCommandListType type) {
        return std::make_unique<MetalCommandList>(this, type);
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

    // TODO: Support Metal RT
//    std::unique_ptr<RenderPipeline> MetalDevice::createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) {
//        return std::make_unique<MetalRaytracingPipeline>(this, desc, previousPipeline);
//    }

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

    RenderSampleCounts MetalDevice::getSampleCountsSupported(RT64::RenderFormat format) const {

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
}