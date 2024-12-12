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
#include "common/rt64_apple.h"

namespace RT64 {
    const constexpr uint32_t MAX_COLOR_ATTACHMENT_COUNT = 8;
    const constexpr uint32_t COLOR_COUNT = MAX_COLOR_ATTACHMENT_COUNT;
    const constexpr uint32_t DEPTH_INDEX = COLOR_COUNT;
    const constexpr uint32_t STENCIL_INDEX = DEPTH_INDEX + 1;
    const constexpr uint32_t ATTACHMENT_COUNT = STENCIL_INDEX + 1;
    static constexpr size_t MAX_DRAWABLES = 3;

    static size_t calculateAlignedSize(size_t size, size_t alignment = 16) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    static uint64_t hashForRenderPipelineDescriptor(MTL::RenderPipelineDescriptor *pipelineDesc) {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);

        std::uintptr_t sampleCount = pipelineDesc->sampleCount();
        XXH3_64bits_update(&xxh3, &sampleCount, sizeof(sampleCount));

        uint16_t pixel_formats[ATTACHMENT_COUNT] = { 0 };
        for (uint32_t i = 0; i < COLOR_COUNT; i++) {
            if (auto colorAttachment = pipelineDesc->colorAttachments()->object(i)) {
                pixel_formats[i] = static_cast<uint16_t>(colorAttachment->pixelFormat());
            }
        }

        if (pipelineDesc->depthAttachmentPixelFormat() != MTL::PixelFormatInvalid) {
            pixel_formats[DEPTH_INDEX] = static_cast<uint16_t>(pipelineDesc->depthAttachmentPixelFormat());
        }

        XXH3_64bits_update(&xxh3, pixel_formats, sizeof(pixel_formats));
        return XXH3_64bits_digest(&xxh3);
    }

    static MTL::RenderPipelineState* getClearRenderPipelineState(MetalDevice *device, MTL::RenderPipelineDescriptor *pipelineDesc) {
        auto hash = hashForRenderPipelineDescriptor(pipelineDesc);

        auto it = device->renderInterface->clearRenderPipelineStates.find(hash);
        if (it != device->renderInterface->clearRenderPipelineStates.end()) {
            return it->second;
        } else {
            NS::Error *error = nullptr;
            auto clearPipelineState = device->mtl->newRenderPipelineState(pipelineDesc, &error)->autorelease();

            if (error != nullptr) {
                fprintf(stderr, "Failed to create render pipeline state: %s\n", error->localizedDescription()->utf8String());
                return nullptr;
            }

            device->renderInterface->clearRenderPipelineStates.insert(std::make_pair(hash, clearPipelineState));

            return clearPipelineState;
        }
    }

    MTL::DataType toMTL(RenderDescriptorRangeType type) {
        switch (type) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
                return MTL::DataTypeTexture;

            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                return MTL::DataTypePrimitiveAccelerationStructure;

            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
                return MTL::DataTypePointer;

            case RenderDescriptorRangeType::SAMPLER:
                return MTL::DataTypeSampler;

            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    RenderFormat toRHI(MTL::PixelFormat format) {
        switch (format) {
            case MTL::PixelFormatInvalid:
                return RenderFormat::UNKNOWN;
            case MTL::PixelFormatRGBA32Float:
                return RenderFormat::R32G32B32A32_FLOAT;
            case MTL::PixelFormatRGBA32Uint:
                return RenderFormat::R32G32B32A32_UINT;
            case MTL::PixelFormatRGBA32Sint:
                return RenderFormat::R32G32B32A32_SINT;
            case MTL::PixelFormatRGBA16Float:
                return RenderFormat::R16G16B16A16_FLOAT;
            case MTL::PixelFormatRGBA16Unorm:
                return RenderFormat::R16G16B16A16_UNORM;
            case MTL::PixelFormatRGBA16Uint:
                return RenderFormat::R16G16B16A16_UINT;
            case MTL::PixelFormatRGBA16Snorm:
                return RenderFormat::R16G16B16A16_SNORM;
            case MTL::PixelFormatRGBA16Sint:
                return RenderFormat::R16G16B16A16_SINT;
            case MTL::PixelFormatRG32Float:
                return RenderFormat::R32G32_FLOAT;
            case MTL::PixelFormatRG32Uint:
                return RenderFormat::R32G32_UINT;
            case MTL::PixelFormatRG32Sint:
                return RenderFormat::R32G32_SINT;
            case MTL::PixelFormatRGBA8Unorm:
                return RenderFormat::R8G8B8A8_UNORM;
            case MTL::PixelFormatRGBA8Uint:
                return RenderFormat::R8G8B8A8_UINT;
            case MTL::PixelFormatRGBA8Snorm:
                return RenderFormat::R8G8B8A8_SNORM;
            case MTL::PixelFormatRGBA8Sint:
                return RenderFormat::R8G8B8A8_SINT;
            case MTL::PixelFormatBGRA8Unorm:
                return RenderFormat::B8G8R8A8_UNORM;
            case MTL::PixelFormatRG16Float:
                return RenderFormat::R16G16_FLOAT;
            case MTL::PixelFormatRG16Unorm:
                return RenderFormat::R16G16_UNORM;
            case MTL::PixelFormatRG16Uint:
                return RenderFormat::R16G16_UINT;
            case MTL::PixelFormatRG16Snorm:
                return RenderFormat::R16G16_SNORM;
            case MTL::PixelFormatRG16Sint:
                return RenderFormat::R16G16_SINT;
            case MTL::PixelFormatDepth32Float:
                return RenderFormat::D32_FLOAT;
            case MTL::PixelFormatR32Float:
                return RenderFormat::R32_FLOAT;
            case MTL::PixelFormatR32Uint:
                return RenderFormat::R32_UINT;
            case MTL::PixelFormatR32Sint:
                return RenderFormat::R32_SINT;
            case MTL::PixelFormatRG8Unorm:
                return RenderFormat::R8G8_UNORM;
            case MTL::PixelFormatRG8Uint:
                return RenderFormat::R8G8_UINT;
            case MTL::PixelFormatRG8Snorm:
                return RenderFormat::R8G8_SNORM;
            case MTL::PixelFormatRG8Sint:
                return RenderFormat::R8G8_SINT;
            case MTL::PixelFormatR16Float:
                return RenderFormat::R16_FLOAT;
            case MTL::PixelFormatDepth16Unorm:
                return RenderFormat::D16_UNORM;
            case MTL::PixelFormatR16Unorm:
                return RenderFormat::R16_UNORM;
            case MTL::PixelFormatR16Uint:
                return RenderFormat::R16_UINT;
            case MTL::PixelFormatR16Snorm:
                return RenderFormat::R16_SNORM;
            case MTL::PixelFormatR16Sint:
                return RenderFormat::R16_SINT;
            case MTL::PixelFormatR8Unorm:
                return RenderFormat::R8_UNORM;
            case MTL::PixelFormatR8Uint:
                return RenderFormat::R8_UINT;
            case MTL::PixelFormatR8Snorm:
                return RenderFormat::R8_SNORM;
            case MTL::PixelFormatR8Sint:
                return RenderFormat::R8_SINT;
            default:
                assert(false && "Unknown Metal format.");
                return RenderFormat::UNKNOWN;
        }
    }

    MTL::PixelFormat toMTL(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTL::PixelFormatInvalid;
            case RenderFormat::R32G32B32A32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32A32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32A32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RenderFormat::R32G32B32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RenderFormat::R32G32B32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RenderFormat::R32G32B32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RenderFormat::R16G16B16A16_TYPELESS:
                return MTL::PixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTL::PixelFormatRGBA16Float;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTL::PixelFormatRGBA16Unorm;
            case RenderFormat::R16G16B16A16_UINT:
                return MTL::PixelFormatRGBA16Uint;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTL::PixelFormatRGBA16Snorm;
            case RenderFormat::R16G16B16A16_SINT:
                return MTL::PixelFormatRGBA16Sint;
            case RenderFormat::R32G32_TYPELESS:
                return MTL::PixelFormatRG32Float;
            case RenderFormat::R32G32_FLOAT:
                return MTL::PixelFormatRG32Float;
            case RenderFormat::R32G32_UINT:
                return MTL::PixelFormatRG32Uint;
            case RenderFormat::R32G32_SINT:
                return MTL::PixelFormatRG32Sint;
            case RenderFormat::R8G8B8A8_TYPELESS:
                return MTL::PixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTL::PixelFormatRGBA8Unorm;
            case RenderFormat::R8G8B8A8_UINT:
                return MTL::PixelFormatRGBA8Uint;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTL::PixelFormatRGBA8Snorm;
            case RenderFormat::R8G8B8A8_SINT:
                return MTL::PixelFormatRGBA8Sint;
            case RenderFormat::B8G8R8A8_UNORM:
                return MTL::PixelFormatBGRA8Unorm;
            case RenderFormat::R16G16_TYPELESS:
                return MTL::PixelFormatRG16Float;
            case RenderFormat::R16G16_FLOAT:
                return MTL::PixelFormatRG16Float;
            case RenderFormat::R16G16_UNORM:
                return MTL::PixelFormatRG16Unorm;
            case RenderFormat::R16G16_UINT:
                return MTL::PixelFormatRG16Uint;
            case RenderFormat::R16G16_SNORM:
                return MTL::PixelFormatRG16Snorm;
            case RenderFormat::R16G16_SINT:
                return MTL::PixelFormatRG16Sint;
            case RenderFormat::R32_TYPELESS:
                return MTL::PixelFormatR32Float;
            case RenderFormat::D32_FLOAT:
                return MTL::PixelFormatDepth32Float;
            case RenderFormat::R32_FLOAT:
                return MTL::PixelFormatR32Float;
            case RenderFormat::R32_UINT:
                return MTL::PixelFormatR32Uint;
            case RenderFormat::R32_SINT:
                return MTL::PixelFormatR32Sint;
            case RenderFormat::R8G8_TYPELESS:
                return MTL::PixelFormatRG8Unorm;
            case RenderFormat::R8G8_UNORM:
                return MTL::PixelFormatRG8Unorm;
            case RenderFormat::R8G8_UINT:
                return MTL::PixelFormatRG8Uint;
            case RenderFormat::R8G8_SNORM:
                return MTL::PixelFormatRG8Snorm;
            case RenderFormat::R8G8_SINT:
                return MTL::PixelFormatRG8Sint;
            case RenderFormat::R16_TYPELESS:
                return MTL::PixelFormatR16Float;
            case RenderFormat::R16_FLOAT:
                return MTL::PixelFormatR16Float;
            case RenderFormat::D16_UNORM:
                return MTL::PixelFormatDepth16Unorm;
            case RenderFormat::R16_UNORM:
                return MTL::PixelFormatR16Unorm;
            case RenderFormat::R16_UINT:
                return MTL::PixelFormatR16Uint;
            case RenderFormat::R16_SNORM:
                return MTL::PixelFormatR16Snorm;
            case RenderFormat::R16_SINT:
                return MTL::PixelFormatR16Sint;
            case RenderFormat::R8_TYPELESS:
                return MTL::PixelFormatR8Unorm;
            case RenderFormat::R8_UNORM:
                return MTL::PixelFormatR8Unorm;
            case RenderFormat::R8_UINT:
                return MTL::PixelFormatR8Uint;
            case RenderFormat::R8_SNORM:
                return MTL::PixelFormatR8Snorm;
            case RenderFormat::R8_SINT:
                return MTL::PixelFormatR8Sint;
            default:
                assert(false && "Unknown format.");
                return MTL::PixelFormatInvalid;
        }
    }

    static NS::UInteger RenderFormatBufferAlignment(MTL::Device *device, RenderFormat format) {
        auto deviceAlignment = device->minimumLinearTextureAlignmentForPixelFormat(toMTL(format));

    #if TARGET_OS_TV
        auto minTexelBufferOffsetAligment = 64;
    #elif TARGET_OS_IPHONE
        auto minTexelBufferOffsetAligment = 64;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAligment = 16;
        }
    #elif TARGET_OS_MAC
        auto minTexelBufferOffsetAligment = 256;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAligment = 16;
        }
    #endif

        return deviceAlignment ? deviceAlignment : minTexelBufferOffsetAligment;
    }

    static MTL::VertexFormat toVertexFormat(RenderFormat format) {
        switch (format) {
            case RenderFormat::UNKNOWN:
                return MTL::VertexFormatInvalid;
            case RenderFormat::R32G32B32A32_FLOAT:
                return MTL::VertexFormatFloat4;
            case RenderFormat::R32G32B32A32_UINT:
                return MTL::VertexFormatUInt4;
            case RenderFormat::R32G32B32A32_SINT:
                return MTL::VertexFormatInt4;
            case RenderFormat::R32G32B32_FLOAT:
                return MTL::VertexFormatFloat3;
            case RenderFormat::R32G32B32_UINT:
                return MTL::VertexFormatUInt3;
            case RenderFormat::R32G32B32_SINT:
                return MTL::VertexFormatInt3;
            case RenderFormat::R16G16B16A16_FLOAT:
                return MTL::VertexFormatHalf4;
            case RenderFormat::R16G16B16A16_UNORM:
                return MTL::VertexFormatUShort4Normalized;
            case RenderFormat::R16G16B16A16_UINT:
                return MTL::VertexFormatUShort4;
            case RenderFormat::R16G16B16A16_SNORM:
                return MTL::VertexFormatShort4Normalized;
            case RenderFormat::R16G16B16A16_SINT:
                return MTL::VertexFormatShort4;
            case RenderFormat::R32G32_FLOAT:
                return MTL::VertexFormatFloat2;
            case RenderFormat::R32G32_UINT:
                return MTL::VertexFormatUInt2;
            case RenderFormat::R32G32_SINT:
                return MTL::VertexFormatInt2;
            case RenderFormat::R8G8B8A8_UNORM:
                return MTL::VertexFormatUChar4Normalized;
            case RenderFormat::R8G8B8A8_UINT:
                return MTL::VertexFormatUChar4;
            case RenderFormat::R8G8B8A8_SNORM:
                return MTL::VertexFormatChar4Normalized;
            case RenderFormat::R8G8B8A8_SINT:
                return MTL::VertexFormatChar4;
            case RenderFormat::R16G16_FLOAT:
                return MTL::VertexFormatHalf2;
            case RenderFormat::R16G16_UNORM:
                return MTL::VertexFormatUShort2Normalized;
            case RenderFormat::R16G16_UINT:
                return MTL::VertexFormatUShort2;
            case RenderFormat::R16G16_SNORM:
                return MTL::VertexFormatShort2Normalized;
            case RenderFormat::R16G16_SINT:
                return MTL::VertexFormatShort2;
            case RenderFormat::R32_FLOAT:
                return MTL::VertexFormatFloat;
            case RenderFormat::R32_UINT:
                return MTL::VertexFormatUInt;
            case RenderFormat::R32_SINT:
                return MTL::VertexFormatInt;
            case RenderFormat::R8G8_UNORM:
                return MTL::VertexFormatUChar2Normalized;
            case RenderFormat::R8G8_UINT:
                return MTL::VertexFormatUChar2;
            case RenderFormat::R8G8_SNORM:
                return MTL::VertexFormatChar2Normalized;
            case RenderFormat::R8G8_SINT:
                return MTL::VertexFormatChar2;
            case RenderFormat::R16_FLOAT:
                return MTL::VertexFormatHalf;
            case RenderFormat::R16_UNORM:
                return MTL::VertexFormatUShortNormalized;
            case RenderFormat::R16_UINT:
                return MTL::VertexFormatUShort;
            case RenderFormat::R16_SNORM:
                return MTL::VertexFormatShortNormalized;
            case RenderFormat::R16_SINT:
                return MTL::VertexFormatShort;
            case RenderFormat::R8_UNORM:
                return MTL::VertexFormatUCharNormalized;
            case RenderFormat::R8_UINT:
                return MTL::VertexFormatUChar;
            case RenderFormat::R8_SNORM:
                return MTL::VertexFormatCharNormalized;
            case RenderFormat::R8_SINT:
                return MTL::VertexFormatChar;
            default:
                assert(false && "Unsupported vertex format.");
                return MTL::VertexFormatInvalid;
        }
    }

    static MTL::TextureType toTextureType(RenderTextureDimension dimension, RenderSampleCounts sampleCount) {
        switch (dimension) {
            case RenderTextureDimension::TEXTURE_1D:
                assert(sampleCount <= 1 && "Multisampling not supported for 1D textures");
                return MTL::TextureType1D;
            case RenderTextureDimension::TEXTURE_2D:
                return (sampleCount > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
            case RenderTextureDimension::TEXTURE_3D:
                assert(sampleCount <= 1 && "Multisampling not supported for 3D textures");
                return MTL::TextureType3D;
            default:
                assert(false && "Unknown resource dimension.");
                return MTL::TextureType2D;
        }
    }

    static MTL::CullMode toMTL(RenderCullMode cullMode) {
        switch (cullMode) {
            case RenderCullMode::NONE:
                return MTL::CullModeNone;
            case RenderCullMode::FRONT:
                return MTL::CullModeFront;
            case RenderCullMode::BACK:
                return MTL::CullModeBack;
            default:
                assert(false && "Unknown cull mode.");
                return MTL::CullModeNone;
        }
    }

    static MTL::PrimitiveTopologyClass toMTL(RenderPrimitiveTopology topology) {
        switch (topology) {
            case RenderPrimitiveTopology::POINT_LIST:
                return MTL::PrimitiveTopologyClassPoint;
            case RenderPrimitiveTopology::LINE_LIST:
                return MTL::PrimitiveTopologyClassLine;
            case RenderPrimitiveTopology::TRIANGLE_LIST:
                return MTL::PrimitiveTopologyClassTriangle;
            default:
                assert(false && "Unknown primitive topology type.");
                return MTL::PrimitiveTopologyClassPoint;
        }
    }

    static MTL::VertexStepFunction toMTL(RenderInputSlotClassification classification) {
        switch (classification) {
            case RenderInputSlotClassification::PER_VERTEX_DATA:
                return MTL::VertexStepFunctionPerVertex;
            case RenderInputSlotClassification::PER_INSTANCE_DATA:
                return MTL::VertexStepFunctionPerInstance;
            default:
                assert(false && "Unknown input classification.");
                return MTL::VertexStepFunctionPerVertex;
        }
    }

    static MTL::BlendFactor toMTL(RenderBlend blend) {
        switch (blend) {
            case RenderBlend::ZERO:
                return MTL::BlendFactorZero;
            case RenderBlend::ONE:
                return MTL::BlendFactorOne;
            case RenderBlend::SRC_COLOR:
                return MTL::BlendFactorSourceColor;
            case RenderBlend::INV_SRC_COLOR:
                return MTL::BlendFactorOneMinusSourceColor;
            case RenderBlend::SRC_ALPHA:
                return MTL::BlendFactorSourceAlpha;
            case RenderBlend::INV_SRC_ALPHA:
                return MTL::BlendFactorOneMinusSourceAlpha;
            case RenderBlend::DEST_ALPHA:
                return MTL::BlendFactorDestinationAlpha;
            case RenderBlend::INV_DEST_ALPHA:
                return MTL::BlendFactorOneMinusDestinationAlpha;
            case RenderBlend::DEST_COLOR:
                return MTL::BlendFactorDestinationColor;
            case RenderBlend::INV_DEST_COLOR:
                return MTL::BlendFactorOneMinusDestinationColor;
            case RenderBlend::SRC_ALPHA_SAT:
                return MTL::BlendFactorSourceAlphaSaturated;
            case RenderBlend::BLEND_FACTOR:
                return MTL::BlendFactorBlendColor;
            case RenderBlend::INV_BLEND_FACTOR:
                return MTL::BlendFactorOneMinusBlendColor;
            case RenderBlend::SRC1_COLOR:
                return MTL::BlendFactorSource1Color;
            case RenderBlend::INV_SRC1_COLOR:
                return MTL::BlendFactorOneMinusSource1Color;
            case RenderBlend::SRC1_ALPHA:
                return MTL::BlendFactorSource1Alpha;
            case RenderBlend::INV_SRC1_ALPHA:
                return MTL::BlendFactorOneMinusSource1Alpha;
            default:
                assert(false && "Unknown blend factor.");
                return MTL::BlendFactorZero;
        }
    }

    static MTL::BlendOperation toMTL(RenderBlendOperation operation) {
        switch (operation) {
            case RenderBlendOperation::ADD:
                return MTL::BlendOperationAdd;
            case RenderBlendOperation::SUBTRACT:
                return MTL::BlendOperationSubtract;
            case RenderBlendOperation::REV_SUBTRACT:
                return MTL::BlendOperationReverseSubtract;
            case RenderBlendOperation::MIN:
                return MTL::BlendOperationMin;
            case RenderBlendOperation::MAX:
                return MTL::BlendOperationMax;
            default:
                assert(false && "Unknown blend operation.");
                return MTL::BlendOperationAdd;
        }
    }

    // Metal does not support Logic Operations in the public API.

    static MTL::CompareFunction toMTL(RenderComparisonFunction function) {
        switch (function) {
            case RenderComparisonFunction::NEVER:
                return MTL::CompareFunctionNever;
            case RenderComparisonFunction::LESS:
                return MTL::CompareFunctionLess;
            case RenderComparisonFunction::EQUAL:
                return MTL::CompareFunctionEqual;
            case RenderComparisonFunction::LESS_EQUAL:
                return MTL::CompareFunctionLessEqual;
            case RenderComparisonFunction::GREATER:
                return MTL::CompareFunctionGreater;
            case RenderComparisonFunction::NOT_EQUAL:
                return MTL::CompareFunctionNotEqual;
            case RenderComparisonFunction::GREATER_EQUAL:
                return MTL::CompareFunctionGreaterEqual;
            case RenderComparisonFunction::ALWAYS:
                return MTL::CompareFunctionAlways;
            default:
                assert(false && "Unknown comparison function.");
                return MTL::CompareFunctionNever;
        }
    }

    static MTL::SamplerMinMagFilter toMTL(RenderFilter filter) {
        switch (filter) {
            case RenderFilter::NEAREST:
                return MTL::SamplerMinMagFilterNearest;
            case RenderFilter::LINEAR:
                return MTL::SamplerMinMagFilterLinear;
            default:
                assert(false && "Unknown filter.");
                return MTL::SamplerMinMagFilterNearest;
        }
    }

    static MTL::SamplerMipFilter toMTL(RenderMipmapMode mode) {
        switch (mode) {
            case RenderMipmapMode::NEAREST:
                return MTL::SamplerMipFilterNearest;
            case RenderMipmapMode::LINEAR:
                return MTL::SamplerMipFilterLinear;
            default:
                assert(false && "Unknown mipmap mode.");
                return MTL::SamplerMipFilterNearest;
        }
    }

    static MTL::SamplerAddressMode toMTL(RenderTextureAddressMode mode) {
        switch (mode) {
            case RenderTextureAddressMode::WRAP:
                return MTL::SamplerAddressModeRepeat;
            case RenderTextureAddressMode::MIRROR:
                return MTL::SamplerAddressModeMirrorRepeat;
            case RenderTextureAddressMode::CLAMP:
                return MTL::SamplerAddressModeClampToEdge;
            case RenderTextureAddressMode::BORDER:
                return MTL::SamplerAddressModeClampToBorderColor;
            case RenderTextureAddressMode::MIRROR_ONCE:
                return MTL::SamplerAddressModeMirrorClampToEdge;
            default:
                assert(false && "Unknown texture address mode.");
                return MTL::SamplerAddressModeRepeat;
        }
    }

    static MTL::SamplerBorderColor toMTL(RenderBorderColor color) {
        switch (color) {
            case RenderBorderColor::TRANSPARENT_BLACK:
                return MTL::SamplerBorderColorTransparentBlack;
            case RenderBorderColor::OPAQUE_BLACK:
                return MTL::SamplerBorderColorOpaqueBlack;
            case RenderBorderColor::OPAQUE_WHITE:
                return MTL::SamplerBorderColorOpaqueWhite;
            default:
                assert(false && "Unknown border color.");
                return MTL::SamplerBorderColorTransparentBlack;
        }
    }

    static MTL::IndexType toIndexType(RenderFormat format) {
        switch (format) {
            case RenderFormat::R16_UINT:
                return MTL::IndexTypeUInt16;
            case RenderFormat::R32_UINT:
                return MTL::IndexTypeUInt32;
            default:
                assert(false && "Format is not supported as an index type.");
                return MTL::IndexTypeUInt16;
        }
    }

    static MTL::ResourceOptions toMTL(RenderHeapType heapType) {
        switch (heapType) {
            case RenderHeapType::DEFAULT:
                return MTL::ResourceStorageModePrivate;
            case RenderHeapType::UPLOAD:
                return MTL::ResourceStorageModeShared;
            case RenderHeapType::READBACK:
                return MTL::ResourceStorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTL::ResourceStorageModePrivate;
        }
    }

    static MTL::ClearColor toClearColor(RenderColor color) {
        return MTL::ClearColor(color.r, color.g, color.b, color.a);
    }

    static MTL::ResourceUsage getResourceUsage(RenderDescriptorRangeType type) {
        switch (type) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
            case RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::CONSTANT_BUFFER:
            case RenderDescriptorRangeType::SAMPLER:
                return MTL::ResourceUsageRead;

            case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
                return MTL::ResourceUsageRead | MTL::ResourceUsageWrite;
            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    // MARK: - Helper Structures

    MetalDescriptorSetLayout::MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc) {
        assert(device != nullptr);
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
            argumentDesc->setDataType(toMTL(range.type));
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
            argumentDesc->setDataType(toMTL(lastRange.type));
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
        descriptorBuffer = device->mtl->newBuffer(argumentEncoder->encodedLength(), MTL::ResourceStorageModeShared);

        argumentEncoder->setArgumentBuffer(descriptorBuffer, 0);

        // Set static samplers
        for (size_t i = 0; i < staticSamplers.size(); i++) {
            argumentEncoder->setSamplerState(staticSamplers[i], samplerIndices[i]);
        }

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

    // MetalBuffer

    MetalBuffer::MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc) {
        assert(device != nullptr);

        this->pool = pool;
        this->desc = desc;
        this->device = device;

        this->mtl = device->mtl->newBuffer(desc.size, toMTL(desc.heapType));
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

        const uint32_t width = buffer->desc.size / RenderFormatSize(format);
        const size_t rowAlignment = RenderFormatBufferAlignment(buffer->device->mtl, format);
        const auto bytesPerRow = calculateAlignedSize(buffer->desc.size, rowAlignment);

        // Configure texture usage flags
        MTL::TextureUsage usage = (buffer->desc.flags & RenderBufferFlag::UNORDERED_ACCESS)
            ? (MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite)
            : MTL::TextureUsageShaderRead;

        // Create and configure texture descriptor
        auto descriptor = MTL::TextureDescriptor::textureBufferDescriptor(toMTL(format), width, toMTL(buffer->desc.heapType), usage);

        // Create texture with configured descriptor and alignment
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
        auto textureType = toTextureType(desc.dimension, desc.multisampling.sampleCount);

        descriptor->setTextureType(textureType);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        descriptor->setPixelFormat(toMTL(desc.format));
        descriptor->setWidth(desc.width);
        descriptor->setHeight(desc.height);
        descriptor->setDepth(desc.depth);
        descriptor->setMipmapLevelCount(desc.mipLevels);
        descriptor->setArrayLength(1);
        descriptor->setSampleCount(desc.multisampling.sampleCount);

        MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
        usage |= (desc.flags & (RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::DEPTH_TARGET)) ? MTL::TextureUsageRenderTarget : MTL::TextureUsageUnknown;
        usage |= (desc.flags & (RenderTextureFlag::UNORDERED_ACCESS)) ? MTL::TextureUsageShaderWrite : MTL::TextureUsageUnknown;
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
        this->texture = texture->mtl->newTextureView(toMTL(desc.format), texture->mtl->textureType(), NS::Range::Make(desc.mipSlice, desc.mipLevels), NS::Range::Make(0, texture->arrayCount));
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
        descriptor->setMinFilter(toMTL(desc.minFilter));
        descriptor->setMagFilter(toMTL(desc.magFilter));
        descriptor->setMipFilter(toMTL(desc.mipmapMode));
        descriptor->setRAddressMode(toMTL(desc.addressU));
        descriptor->setSAddressMode(toMTL(desc.addressV));
        descriptor->setTAddressMode(toMTL(desc.addressW));
        descriptor->setMaxAnisotropy(desc.maxAnisotropy);
        descriptor->setCompareFunction(toMTL(desc.comparisonFunc));
        descriptor->setLodMinClamp(desc.minLOD);
        descriptor->setLodMaxClamp(desc.maxLOD);
        descriptor->setBorderColor(toMTL(desc.borderColor));

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
        descriptor->setInputPrimitiveTopology(toMTL(desc.primitiveTopology));
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
            layout->setStepFunction(toMTL(inputSlot.classification));
            layout->setStepRate((layout->stepFunction() == MTL::VertexStepFunctionPerInstance) ? inputSlot.stride : 1);
        }

        for (uint32_t i = 0; i < desc.inputElementsCount; i++) {
            const RenderInputElement &inputElement = desc.inputElements[i];

            auto attributeDescriptor = vertexDescriptor->attributes()->object(i);
            attributeDescriptor->setOffset(inputElement.alignedByteOffset);
            attributeDescriptor->setBufferIndex(inputElement.slotIndex);
            attributeDescriptor->setFormat(toVertexFormat(inputElement.format));
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
            blendDescriptor->setSourceRGBBlendFactor(toMTL(blendDesc.srcBlend));
            blendDescriptor->setDestinationRGBBlendFactor(toMTL(blendDesc.dstBlend));
            blendDescriptor->setRgbBlendOperation(toMTL(blendDesc.blendOp));
            blendDescriptor->setSourceAlphaBlendFactor(toMTL(blendDesc.srcBlendAlpha));
            blendDescriptor->setDestinationAlphaBlendFactor(toMTL(blendDesc.dstBlendAlpha));
            blendDescriptor->setAlphaBlendOperation(toMTL(blendDesc.blendOpAlpha));
            blendDescriptor->setWriteMask(blendDesc.renderTargetWriteMask);
            blendDescriptor->setPixelFormat(toMTL(desc.renderTargetFormat[i]));
        }

        descriptor->setDepthAttachmentPixelFormat(toMTL(desc.depthTargetFormat));
        descriptor->setRasterSampleCount(desc.multisampling.sampleCount);

        // State variables, initialized here to be reused in encoder re-binding
        MTL::DepthStencilDescriptor *depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
        depthStencilDescriptor->setDepthWriteEnabled(desc.depthWriteEnabled);
        depthStencilDescriptor->setDepthCompareFunction(desc.depthEnabled ? toMTL(desc.depthFunction) : MTL::CompareFunctionAlways);

        state = new MetalRenderState();
        state->depthStencilState = device->mtl->newDepthStencilState(depthStencilDescriptor);
        state->cullMode = toMTL(desc.cullMode);
        state->depthClipMode = (desc.depthClipEnabled) ? MTL::DepthClipModeClip : MTL::DepthClipModeClamp;
        state->winding = MTL::WindingClockwise;
        state->sampleCount = desc.multisampling.sampleCount;
        if (desc.multisampling.sampleCount > 1) {
            state->samplePositions = new MTL::SamplePosition[desc.multisampling.sampleCount];
            for (uint32_t i = 0; i < desc.multisampling.sampleCount; i++) {
                state->samplePositions[i] = MTL::SamplePosition::Make(desc.multisampling.sampleLocations[i].x, desc.multisampling.sampleLocations[i].y);
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

    // MetalSwapChain

    MetalSwapChain::MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) {
        this->layer = static_cast<CA::MetalLayer*>(renderWindow.view);
        layer->setDevice(commandQueue->device->mtl);
        layer->setPixelFormat(toMTL(format));

        this->commandQueue = commandQueue;
        this->layer->setPixelFormat(toMTL(format));

        // Metal supports a maximum of 3 drawables.
        this->textureCount = 3;
        this->textures.resize(3);

        this->renderWindow = renderWindow;
        getWindowSize(width, height);

        // set each of the drawable to have desc.flags = RenderTextureFlag::RENDER_TARGET;
        for (uint32_t i = 0; i < this->textureCount; i++) {
            auto& drawable = this->textures[i];
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

        auto& texture = textures[textureIndex];
        assert(texture.drawable != nullptr && "Cannot present without a valid drawable.");

        // Create a new command buffer just for presenting
         auto presentBuffer = commandQueue->mtl->commandBuffer();
         presentBuffer->presentDrawable(texture.drawable);
         presentBuffer->addCompletedHandler([this](MTL::CommandBuffer* cmdBuffer) {
             dispatch_semaphore_signal(commandQueue->device->renderInterface->drawables_semaphore);
             currentAvailableDrawableIndex = (currentAvailableDrawableIndex + 1) % textureCount;
         });
         presentBuffer->enqueue();
         presentBuffer->commit();

        texture.drawable->release();
        texture.mtl->release();

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
        return &textures[textureIndex];
    }

    bool MetalSwapChain::acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) {
        assert(signalSemaphore != nullptr);
        assert(textureIndex != nullptr);
        assert(*textureIndex < textureCount);

        dispatch_semaphore_wait(commandQueue->device->renderInterface->drawables_semaphore, DISPATCH_TIME_FOREVER);

        auto nextDrawable = layer->nextDrawable();
        if (nextDrawable == nullptr) {
            fprintf(stderr, "No more drawables available for rendering.\n");
            return false;
        }

        *textureIndex = currentAvailableDrawableIndex;
        auto& drawable = textures[currentAvailableDrawableIndex];
        drawable.desc.width = width;
        drawable.desc.height = height;
        drawable.desc.flags = RenderTextureFlag::RENDER_TARGET;
        drawable.desc.format = toRHI(nextDrawable->texture()->pixelFormat());
        drawable.drawable = nextDrawable;
        drawable.mtl = nextDrawable->texture();
        
        return true;
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
        indexBuffer->release();
        graphicsPushConstantsBuffer->release();
        computePushConstantsBuffer->release();

        for (auto buffer : vertexBuffers) {
            buffer->release();
        }
    }

    void MetalCommandList::begin() {}

    void MetalCommandList::end() {
        endActiveClearColorRenderEncoder();
        endActiveRenderEncoder();
        endActiveResolveTextureComputeEncoder();
        endActiveBlitEncoder();
        endActiveClearDepthRenderEncoder();
        endActiveComputeEncoder();

        targetFramebuffer = nullptr;
    }

    void MetalCommandList::configureRenderDescriptor(MTL::RenderPassDescriptor *renderDescriptor, EncoderType encoderType) {
        assert(targetFramebuffer != nullptr && "Cannot encode render commands without a target framebuffer");

        if (encoderType != EncoderType::ClearDepth) {
            for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); i++) {
                auto colorAttachment = renderDescriptor->colorAttachments()->object(i);
                colorAttachment->setTexture(targetFramebuffer->colorAttachments[i]->mtl);
                colorAttachment->setLoadAction(MTL::LoadActionLoad);
                colorAttachment->setStoreAction(MTL::StoreActionStore);
            }
        }

        if (encoderType != EncoderType::ClearColor) {
            if (targetFramebuffer->depthAttachment != nullptr) {
                auto depthAttachment = renderDescriptor->depthAttachment();
                depthAttachment->setTexture(targetFramebuffer->depthAttachment->mtl);
                depthAttachment->setLoadAction(MTL::LoadActionLoad);
                depthAttachment->setStoreAction(MTL::StoreActionStore);
            }
        }

        if (encoderType == EncoderType::Render && activeRenderState->sampleCount > 1) {
            renderDescriptor->setSamplePositions(activeRenderState->samplePositions, activeRenderState->sampleCount);
        }
    }

    void MetalCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        // TODO: Ignore for now, Metal should handle most of this itself.
    }

    void MetalCommandList::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) {
        checkActiveComputeEncoder();
        assert(activeComputeEncoder != nullptr && "Cannot encode dispatch on nullptr MTLComputeCommandEncoder!");


        auto threadGroupCount = MTL::Size::Make(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
        auto threadGroupSize = MTL::Size::Make(activeComputeState->threadGroupSizeX, activeComputeState->threadGroupSizeY, activeComputeState->threadGroupSizeZ);
        activeComputeEncoder->dispatchThreadgroups(threadGroupCount, threadGroupSize);
    }

    void MetalCommandList::traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) {
        // TODO: Support Metal RT
    }

    void MetalCommandList::drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
        checkActiveRenderEncoder();

        activeRenderEncoder->drawPrimitives(currentPrimitiveType, startVertexLocation, vertexCountPerInstance, instanceCount, startInstanceLocation);
    }

    void MetalCommandList::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
        checkActiveRenderEncoder();

        activeRenderEncoder->drawIndexedPrimitives(currentPrimitiveType, indexCountPerInstance, currentIndexType, indexBuffer, startIndexLocation, instanceCount, baseVertexLocation, startInstanceLocation);
    }

    void MetalCommandList::setPipeline(const RenderPipeline *pipeline) {
        assert(pipeline != nullptr);

        const auto *interfacePipeline = static_cast<const MetalPipeline *>(pipeline);
        switch (interfacePipeline->type) {
            case MetalPipeline::Type::Compute: {
                endActiveComputeEncoder();
                const auto *computePipeline = static_cast<const MetalComputePipeline *>(interfacePipeline);
                activeComputeState = computePipeline->state;
                break;
            }
            case MetalPipeline::Type::Graphics: {
                endActiveRenderEncoder();
                const auto *graphicsPipeline = static_cast<const MetalGraphicsPipeline *>(interfacePipeline);
                activeRenderState = graphicsPipeline->state;
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
            indicesToComputeDescriptorSets.clear();
            computePushConstantsBuffer = nullptr;
        }
    }

    void MetalCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data) {
        assert(activeComputePipelineLayout != nullptr);
        assert(rangeIndex < activeComputePipelineLayout->pushConstantRanges.size());

        // TODO: make sure there's parity with Vulkan
        const RenderPushConstantRange &range = activeComputePipelineLayout->pushConstantRanges[rangeIndex];
        uint32_t startOffset = 0;
        for (uint32_t i = 0; i < rangeIndex; i++) {
            startOffset += activeComputePipelineLayout->pushConstantRanges[i].size;
        }

        auto bufferContents = (uint8_t *)activeComputePipelineLayout->pushConstantsBuffer->contents();
        memcpy(bufferContents + startOffset, data, range.size);

        computePushConstantsBuffer = activeComputePipelineLayout->pushConstantsBuffer;
    }

    void MetalCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        setDescriptorSet(descriptorSet, setIndex, true);
    }

    void MetalCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        assert(pipelineLayout != nullptr);

        const auto oldLayout = activeGraphicsPipelineLayout;
        activeGraphicsPipelineLayout = static_cast<const MetalPipelineLayout *>(pipelineLayout);

        if (oldLayout != activeGraphicsPipelineLayout) {
            indicesToRenderDescriptorSets.clear();
            graphicsPushConstantsBuffer = nullptr;
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

        auto bufferContents = (uint8_t *)activeGraphicsPipelineLayout->pushConstantsBuffer->contents();
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
            indexBuffer = interfaceBuffer->mtl;
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
                vertexBuffers.emplace_back(interfaceBuffer->mtl);
                vertexBufferOffsets.emplace_back(views[i].buffer.offset);
                vertexBufferIndices.emplace_back(startSlot + i);
            }
        }
    }

    void MetalCommandList::setViewports(const RenderViewport *viewports, uint32_t count) {
        viewportVector.clear();

        for (uint32_t i = 0; i < count; i++) {
            MTL::Viewport viewport { viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height, viewports[i].minDepth, viewports[i].maxDepth };
            viewportVector.emplace_back(viewport);
        }
    }

    void MetalCommandList::setScissors(const RenderRect *scissorRects, uint32_t count) {
        scissorVector.clear();

        for (uint32_t i = 0; i < count; i++) {
            MTL::ScissorRect scissor {
                static_cast<NS::UInteger>(scissorRects[i].left),
                static_cast<NS::UInteger>(scissorRects[i].top),
                static_cast<NS::UInteger>(scissorRects[i].right - scissorRects[i].left),
                static_cast<NS::UInteger>(scissorRects[i].bottom - scissorRects[i].top)
            };
            scissorVector.emplace_back(scissor);
        }
    }

    void MetalCommandList::setFramebuffer(const RenderFramebuffer *framebuffer) {
        endActiveClearColorRenderEncoder();
        endActiveRenderEncoder();

        if (framebuffer != nullptr) {
            targetFramebuffer = static_cast<const MetalFramebuffer *>(framebuffer);
        } else {
            targetFramebuffer = nullptr;
        }
    }

    void MetalCommandList::encodeCommonClear(MTL::RenderCommandEncoder *encoder, const RenderRect *clearRects, uint32_t clearRectsCount) {
        if (clearRectsCount == 0) {
            MTL::Viewport viewport { 0, 0, static_cast<double>(targetFramebuffer->width), static_cast<double>(targetFramebuffer->height), 0, 1 };
            encoder->setViewport(viewport);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0.f, 3);
        } else {
            for (uint32_t i = 0; i < clearRectsCount; i++) {
                const RenderRect& rect = clearRects[i];

                if (rect.isEmpty()) {
                    continue;
                }

                int32_t width = rect.right - rect.left;
                int32_t height = rect.bottom - rect.top;

                MTL::Viewport viewport { static_cast<double>(rect.left), static_cast<double>(targetFramebuffer->height - rect.top), static_cast<double>(width), static_cast<double>(-height), 0, 1 };

                // clamp to viewport size as metal does not support larger values than viewport size
                MTL::ScissorRect scissor {
                    static_cast<NS::UInteger>(std::max(0, std::min(rect.left, static_cast<int32_t>(targetFramebuffer->width)))),
                    static_cast<NS::UInteger>(std::max(0, std::min(static_cast<int32_t>(targetFramebuffer->height - rect.top - height),
                                                                  static_cast<int32_t>(targetFramebuffer->height)))),
                    static_cast<NS::UInteger>(std::min(width, static_cast<int32_t>(targetFramebuffer->width - rect.left))),
                    static_cast<NS::UInteger>(std::min(height, static_cast<int32_t>(targetFramebuffer->height -
                                             std::max(0, std::min(static_cast<int32_t>(targetFramebuffer->height - rect.top - height),
                                                                static_cast<int32_t>(targetFramebuffer->height))))))
                };

                encoder->setViewport(viewport);
                encoder->setScissorRect(scissor);
                encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0.f, 3);
            }
        }
    }

    void MetalCommandList::clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(attachmentIndex < targetFramebuffer->colorAttachments.size());

        checkActiveClearColorRenderEncoder();

        float clearColor[4] = { colorValue.r, colorValue.g, colorValue.b, colorValue.a };
        activeClearColorRenderEncoder->setFragmentBytes(clearColor, sizeof(float) * 4, 0);
        encodeCommonClear(activeClearColorRenderEncoder, clearRects, clearRectsCount);
    }

    void MetalCommandList::clearDepth(bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) {
        assert(targetFramebuffer != nullptr);
        assert(targetFramebuffer->depthAttachment != nullptr);

        checkActiveClearDepthRenderEncoder();

        activeClearDepthRenderEncoder->setFragmentBytes(&depthValue, sizeof(float), 0);
        encodeCommonClear(activeClearDepthRenderEncoder, clearRects, clearRectsCount);
    }

    void MetalCommandList::copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) {
        assert(dstBuffer.ref != nullptr);
        assert(srcBuffer.ref != nullptr);

        checkActiveBlitEncoder();

        const auto interfaceDstBuffer = static_cast<const MetalBuffer *>(dstBuffer.ref);
        const auto interfaceSrcBuffer = static_cast<const MetalBuffer *>(srcBuffer.ref);

        activeBlitEncoder->copyFromBuffer(interfaceSrcBuffer->mtl, srcBuffer.offset, interfaceDstBuffer->mtl, dstBuffer.offset, size);
    }

    void MetalCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLocation,
                                             const RenderTextureCopyLocation &srcLocation,
                                             uint32_t dstX, uint32_t dstY, uint32_t dstZ,
                                             const RenderBox *srcBox) {
        assert(dstLocation.type != RenderTextureCopyType::UNKNOWN);
        assert(srcLocation.type != RenderTextureCopyType::UNKNOWN);

        checkActiveBlitEncoder();

        const auto dstTexture = static_cast<const MetalTexture *>(dstLocation.texture);
        const auto srcTexture = static_cast<const MetalTexture *>(srcLocation.texture);
        const auto dstBuffer = static_cast<const MetalBuffer *>(dstLocation.buffer);
        const auto srcBuffer = static_cast<const MetalBuffer *>(srcLocation.buffer);

        if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE &&
            srcLocation.type == RenderTextureCopyType::PLACED_FOOTPRINT) {
            assert(dstTexture != nullptr);
            assert(srcBuffer != nullptr);

            // Calculate actual dimensions based on format block size
            uint32_t blockWidth = RenderFormatBlockWidth(srcLocation.placedFootprint.format);
            uint32_t width = ((srcLocation.placedFootprint.width + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t height = ((srcLocation.placedFootprint.height + blockWidth - 1) / blockWidth) * blockWidth;
            uint32_t depth = srcLocation.placedFootprint.depth;

            MTL::Size size = MTL::Size::Make(width, height, depth);

            // Calculate proper bytes per row based on format
            uint32_t blockCount = (srcLocation.placedFootprint.rowWidth + blockWidth - 1) / blockWidth;
            uint32_t bytesPerRow = blockCount * RenderFormatSize(srcLocation.placedFootprint.format);

            // Verify alignment requirements
            assert((srcLocation.placedFootprint.offset % 256) == 0 && "Buffer offset must be aligned");
            assert((bytesPerRow % 256) == 0 && "Bytes per row must be aligned");

            uint32_t bytesPerImage = bytesPerRow * height;
            MTL::Origin dstOrigin = MTL::Origin::Make(dstX, dstY, dstZ);
            activeBlitEncoder->copyFromBuffer(srcBuffer->mtl, srcLocation.placedFootprint.offset, bytesPerRow, bytesPerImage, size, dstTexture->mtl, dstLocation.subresource.index, 0, dstOrigin);
        }
        else if (dstLocation.type == RenderTextureCopyType::SUBRESOURCE &&
                 srcLocation.type == RenderTextureCopyType::SUBRESOURCE) {
            assert(dstTexture != nullptr);
            assert(srcTexture != nullptr);

            MTL::Origin srcOrigin;
            MTL::Size size;

            if (srcBox != nullptr) {
                srcOrigin = MTL::Origin::Make(srcBox->left, srcBox->top, srcBox->front);
                size = MTL::Size::Make(srcBox->right - srcBox->left, srcBox->bottom - srcBox->top, srcBox->back - srcBox->front);
            } else {
                srcOrigin = MTL::Origin::Make(0, 0, 0);
                size = MTL::Size::Make(srcTexture->desc.width, srcTexture->desc.height, srcTexture->desc.depth);
            }

            MTL::Origin dstOrigin = MTL::Origin::Make(dstX, dstY, dstZ);
            activeBlitEncoder->copyFromTexture(srcTexture->mtl, srcLocation.subresource.index, 0, srcOrigin, size, dstTexture->mtl, dstLocation.subresource.index, 0, dstOrigin);
        }
        else {
            assert(false && "Unsupported texture copy type combination");
        }
    }

    void MetalCommandList::copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) {
        assert(dstBuffer != nullptr);
        assert(srcBuffer != nullptr);

        checkActiveBlitEncoder();

        const auto dst = static_cast<const MetalBuffer *>(dstBuffer);
        const auto src = static_cast<const MetalBuffer *>(srcBuffer);

        activeBlitEncoder->copyFromBuffer(src->mtl, 0, dst->mtl, 0, dst->desc.size);
    }

    void MetalCommandList::copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

        checkActiveBlitEncoder();

        const auto dst = static_cast<const MetalTexture *>(dstTexture);
        const auto src = static_cast<const MetalTexture *>(srcTexture);

        activeBlitEncoder->copyFromTexture(src->mtl, dst->mtl);
    }

    void MetalCommandList::resolveTexture(const RT64::RenderTexture *dstTexture, const RT64::RenderTexture *srcTexture) {
        resolveTextureRegion(dstTexture, 0, 0, srcTexture, nullptr);
    }

    void MetalCommandList::resolveTextureRegion(const RT64::RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RT64::RenderTexture *srcTexture, const RT64::RenderRect *srcRect) {
        assert(dstTexture != nullptr);
        assert(srcTexture != nullptr);

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

        MTL::Size threadGroupSize = MTL::Size::Make(8, 8, 1);
        auto groupSizeX = (width + threadGroupSize.width - 1) / threadGroupSize.width;
        auto groupSizeY = (height + threadGroupSize.height - 1) / threadGroupSize.height;
        MTL::Size gridSize = MTL::Size::Make(groupSizeX, groupSizeY, 1);
        activeResolveComputeEncoder->dispatchThreadgroups(gridSize, threadGroupSize);
    }

    void MetalCommandList::buildBottomLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, const RT64::RenderBottomLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::buildTopLevelAS(const RT64::RenderAccelerationStructure *dstAccelerationStructure, RT64::RenderBufferReference scratchBuffer, RT64::RenderBufferReference instancesBuffer, const RT64::RenderTopLevelASBuildInfo &buildInfo) {
        // TODO: Unimplemented.
    }

    void MetalCommandList::endOtherEncoders(EncoderType type) {
        if (type != EncoderType::ClearColor) {
            endActiveClearColorRenderEncoder();
        }
        if (type != EncoderType::ClearDepth) {
            endActiveClearDepthRenderEncoder();
        }
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
        endOtherEncoders(EncoderType::Compute);

        if (activeComputeEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::ComputePassDescriptor *computeDescriptor = MTL::ComputePassDescriptor::alloc()->init();
            activeComputeEncoder = queue->buffer->computeCommandEncoder(computeDescriptor);
            activeComputeEncoder->setLabel(MTLSTR("Active Compute Encoder"));
            activeComputeEncoder->setComputePipelineState(activeComputeState->pipelineState);

            bindDescriptorSetLayout(activeComputePipelineLayout, activeComputeEncoder, indicesToComputeDescriptorSets, computePushConstantsBuffer, true);

            computeDescriptor->release();
            activeComputeEncoder->retain();
            releasePool->release();
            int g = 0;
        }
    }

    void MetalCommandList::endActiveComputeEncoder() {
        if (activeComputeEncoder != nullptr) {
            activeComputeEncoder->endEncoding();
            activeComputeEncoder->release();
            activeComputeEncoder = nullptr;
        }
    }

    void MetalCommandList::checkActiveRenderEncoder() {
        assert(targetFramebuffer != nullptr);
        endOtherEncoders(EncoderType::Render);

        if (activeRenderEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::RenderPassDescriptor *renderDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
            configureRenderDescriptor(renderDescriptor, EncoderType::Render);

            activeRenderEncoder = queue->buffer->renderCommandEncoder(renderDescriptor);
            activeRenderEncoder->setLabel(MTLSTR("Active Render Encoder"));
            activeRenderEncoder->setRenderPipelineState(activeRenderState->renderPipelineState);
            activeRenderEncoder->setDepthStencilState(activeRenderState->depthStencilState);
            activeRenderEncoder->setDepthClipMode(activeRenderState->depthClipMode);
            activeRenderEncoder->setCullMode(activeRenderState->cullMode);
            activeRenderEncoder->setFrontFacingWinding(activeRenderState->winding);

            activeRenderEncoder->setViewports(viewportVector.data(), viewportVector.size());
            activeRenderEncoder->setScissorRects(scissorVector.data(), scissorVector.size());

            for (uint32_t i = 0; i < viewCount; i++) {
                activeRenderEncoder->setVertexBuffer(vertexBuffers[i], vertexBufferOffsets[i], vertexBufferIndices[i]);
            }

            bindDescriptorSetLayout(activeGraphicsPipelineLayout, activeRenderEncoder, indicesToRenderDescriptorSets, graphicsPushConstantsBuffer, false);

            activeRenderEncoder->retain();
            releasePool->release();
        }
    }

    void MetalCommandList::endActiveRenderEncoder() {
        if (activeRenderEncoder != nullptr) {
            activeRenderEncoder->endEncoding();
            activeRenderEncoder->release();
            activeRenderEncoder = nullptr;
        }
    }

    void MetalCommandList::checkActiveClearColorRenderEncoder() {
        assert(targetFramebuffer != nullptr);
        endOtherEncoders(EncoderType::ClearColor);

        if (activeClearColorRenderEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::RenderPassDescriptor *renderDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
            configureRenderDescriptor(renderDescriptor, EncoderType::ClearColor);

            auto clearVertFunction = device->renderInterface->clearColorShaderLibrary->newFunction(MTLSTR("clearVert"));
            auto clearFragFunction = device->renderInterface->clearColorShaderLibrary->newFunction(MTLSTR("clearFrag"));

            MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
            pipelineDesc->setVertexFunction(clearVertFunction);
            pipelineDesc->setFragmentFunction(clearFragFunction);

            // TODO: Are we using the right color attachment?
            pipelineDesc->setRasterSampleCount(targetFramebuffer->colorAttachments[0]->desc.multisampling.sampleCount);

            // Configure attachments
            for (uint32_t i = 0; i < targetFramebuffer->colorAttachments.size(); i++) {
                MTL::PixelFormat format = toMTL(targetFramebuffer->colorAttachments[i]->desc.format);

                auto pipelineColorAttachment = pipelineDesc->colorAttachments()->object(i);
                pipelineColorAttachment->setPixelFormat(format);
                pipelineColorAttachment->setBlendingEnabled(false);
            }

            activeClearColorRenderEncoder = queue->buffer->renderCommandEncoder(renderDescriptor);
            activeClearColorRenderEncoder->setLabel(MTLSTR("Active Clear Color Encoder"));

            auto clearPipelineState = getClearRenderPipelineState(device, pipelineDesc);
            activeClearColorRenderEncoder->setRenderPipelineState(clearPipelineState);

            // Release resources
            clearVertFunction->release();
            clearFragFunction->release();
            pipelineDesc->release();
            activeClearColorRenderEncoder->retain();
            releasePool->release();
        }
    }

    void MetalCommandList::endActiveClearColorRenderEncoder() {
        if (activeClearColorRenderEncoder != nullptr) {
            activeClearColorRenderEncoder->endEncoding();
            activeClearColorRenderEncoder = nullptr;
        }
    }

    void MetalCommandList::checkActiveBlitEncoder() {
        if (activeBlitEncoder == nullptr) {
            endOtherEncoders(EncoderType::Blit);

            // TODO: We don't specialize this descriptor, so it could be reused.
            auto blitDescriptor = MTL::BlitPassDescriptor::alloc()->init();
            activeBlitEncoder = queue->buffer->blitCommandEncoder(blitDescriptor);
            activeBlitEncoder->setLabel(MTLSTR("Active Blit Encoder"));

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
        endOtherEncoders(EncoderType::Resolve);

        if (activeResolveComputeEncoder == nullptr) {
            activeResolveComputeEncoder = queue->buffer->computeCommandEncoder();
            activeResolveComputeEncoder->setLabel(MTLSTR("Active Resolve Texture Encoder"));
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

    void MetalCommandList::checkActiveClearDepthRenderEncoder() {
        assert(targetFramebuffer != nullptr);
        endOtherEncoders(EncoderType::ClearDepth);

        if (activeClearDepthRenderEncoder == nullptr) {
            NS::AutoreleasePool *releasePool = NS::AutoreleasePool::alloc()->init();

            MTL::RenderPassDescriptor *renderDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
            configureRenderDescriptor(renderDescriptor, EncoderType::ClearDepth);

            auto clearDepthVertFunction = device->renderInterface->clearDepthShaderLibrary->newFunction(MTLSTR("clearDepthVertex"));
            auto clearDepthFragFunction = device->renderInterface->clearDepthShaderLibrary->newFunction(MTLSTR("clearDepthFragment"));

            MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
            pipelineDesc->setVertexFunction(clearDepthVertFunction);
            pipelineDesc->setFragmentFunction(clearDepthFragFunction);
            pipelineDesc->setDepthAttachmentPixelFormat(targetFramebuffer->depthAttachment->mtl->pixelFormat());
            pipelineDesc->setRasterSampleCount(targetFramebuffer->depthAttachment->desc.multisampling.sampleCount);

            activeClearDepthRenderEncoder = queue->buffer->renderCommandEncoder(renderDescriptor);
            activeClearDepthRenderEncoder->setLabel(MTLSTR("Active Clear Depth Encoder"));

            auto clearPipelineState = getClearRenderPipelineState(device, pipelineDesc);
            activeClearDepthRenderEncoder->setRenderPipelineState(clearPipelineState);
            activeClearDepthRenderEncoder->setDepthStencilState(device->renderInterface->clearDepthStencilState);

            // Release resources
            clearDepthVertFunction->release();
            clearDepthFragFunction->release();
            pipelineDesc->release();
            activeClearDepthRenderEncoder->retain();
            releasePool->release();
        }
    }

    void MetalCommandList::endActiveClearDepthRenderEncoder() {
        if (activeClearDepthRenderEncoder != nullptr) {
            activeClearDepthRenderEncoder->endEncoding();
            activeClearDepthRenderEncoder = nullptr;
        }
    }

    void MetalCommandList::setDescriptorSet(RT64::RenderDescriptorSet *descriptorSet, uint32_t setIndex, bool setCompute) {
        auto *interfaceDescriptorSet = static_cast<MetalDescriptorSet *>(descriptorSet);
        if (setCompute) {
            indicesToComputeDescriptorSets[setIndex] = interfaceDescriptorSet;
        } else {
            indicesToRenderDescriptorSets[setIndex] = interfaceDescriptorSet;
        }
    }

    void MetalCommandList::bindDescriptorSetLayout(const MetalPipelineLayout* layout, MTL::CommandEncoder* encoder, const std::unordered_map<uint32_t, MetalDescriptorSet*>& descriptorSets, MTL::Buffer* pushConstantsBuffer, bool isCompute) {
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
                        auto descriptorType = setLayout->descriptorTypes[index];
                        auto usageFlags = getResourceUsage(descriptorType);

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
                        auto descriptorType = setLayout->descriptorTypes[index];
                        auto usageFlags = getResourceUsage(descriptorType);

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
                        auto descriptorType = setLayout->descriptorTypes[index];
                        auto usageFlags = getResourceUsage(descriptorType);

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

            if (isCompute) {
                static_cast<MTL::ComputeCommandEncoder*>(encoder)->setBuffer(setLayout->descriptorBuffer, 0, i);
            } else {
                static_cast<MTL::RenderCommandEncoder*>(encoder)->setFragmentBuffer(setLayout->descriptorBuffer, 0, i);
            }
    }

    if (pushConstantsBuffer != nullptr) {
        uint32_t pushConstantsIndex = layout->setCount;
        if (isCompute) {
            static_cast<MTL::ComputeCommandEncoder*>(encoder)->setBuffer(pushConstantsBuffer, 0, pushConstantsIndex);
        } else {
            static_cast<MTL::RenderCommandEncoder*>(encoder)->setFragmentBuffer(pushConstantsBuffer, 0, pushConstantsIndex);
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
        this->mtl = device->mtl->newCommandQueue();
        this->buffer = mtl->commandBuffer();
    }

    MetalCommandQueue::~MetalCommandQueue() {
        buffer->release();
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

        if (signalFence != nullptr) {
            buffer->addCompletedHandler([signalFence, this](MTL::CommandBuffer* cmdBuffer) {
                dispatch_semaphore_signal(static_cast<MetalCommandFence *>(signalFence)->semaphore);
            });
        }

        buffer->enqueue();
        buffer->commit();

        buffer->release();
        buffer = mtl->commandBuffer();
    }

    void MetalCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        auto *metalFence = static_cast<MetalCommandFence *>(fence);
        dispatch_semaphore_wait(metalFence->semaphore, DISPATCH_TIME_FOREVER);
    }

    // MetalPipelineLayout

    MetalPipelineLayout::MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc) {
        assert(device != nullptr);

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
            pushConstantsBuffer = device->mtl->newBuffer(alignedSize, MTL::ResourceStorageModeShared);
        }

        // Create Descriptor Set Layouts
        for (uint32_t i = 0; i < desc.descriptorSetDescsCount; i++) {
            const RenderDescriptorSetDesc &setDesc = desc.descriptorSetDescs[i];
            setLayoutHandles.emplace_back(new MetalDescriptorSetLayout(device, setDesc));
        }
    }

    MetalPipelineLayout::~MetalPipelineLayout() {
        pushConstantsBuffer->release();
    }

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

        createClearColorShaderLibrary();
        createResolvePipelineState();
        createClearDepthShaderLibrary();

        drawables_semaphore = dispatch_semaphore_create(MAX_DRAWABLES);

        releasePool->release();
    }

    MetalInterface::~MetalInterface() {
        resolveTexturePipelineState->release();
        clearColorShaderLibrary->release();
        clearDepthShaderLibrary->release();
        device->release();
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

    void MetalInterface::createClearColorShaderLibrary() {
        const char* clear_color_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            vertex float4 clearVert(uint vid [[vertex_id]]) {
                const float2 positions[] = {
                    float2(-1, -1),
                    float2(3, -1),
                    float2(-1, 3)
                };

                return float4(positions[vid], 0, 1);
            }

            fragment float4 clearFrag(constant float4& clearColor [[buffer(0)]]) {
                return clearColor;
            }
        )";

        NS::Error* error = nullptr;
        clearColorShaderLibrary = device->newLibrary(NS::String::string(clear_color_shader, NS::UTF8StringEncoding), nullptr, &error);
        assert(clearColorShaderLibrary != nullptr && "Failed to create clear color library");
    }

    void MetalInterface::createClearDepthShaderLibrary() {
        const char* depth_clear_shader = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct DepthClearFragmentOut {
                float depth [[depth(any)]];
            };

            vertex float4 clearDepthVertex(uint vid [[vertex_id]]) {
                const float2 positions[] = {
                    float2(-1, -1),
                    float2(3, -1),
                    float2(-1, 3)
                };

                return float4(positions[vid], 1, 1);
            }

            fragment DepthClearFragmentOut clearDepthFragment(constant float& clearDepth [[buffer(0)]]) {
                DepthClearFragmentOut out;
                out.depth = clearDepth;
                return out;
            }
        )";

        NS::Error* error = nullptr;
        clearDepthShaderLibrary = device->newLibrary(NS::String::string(depth_clear_shader, NS::UTF8StringEncoding), nullptr, &error);
        assert(clearDepthShaderLibrary != nullptr && "Failed to create clear depth library");

        MTL::DepthStencilDescriptor *depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
        depthDescriptor->setDepthWriteEnabled(true);
        depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
        clearDepthStencilState = device->newDepthStencilState(depthDescriptor);

        depthDescriptor->release();
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

    // Global creation function.

    std::unique_ptr<RenderInterface> CreateMetalInterface() {
        std::unique_ptr<MetalInterface> createdInterface = std::make_unique<MetalInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
}
