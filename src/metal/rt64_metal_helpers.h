//
// RT64
//

#pragma once

#include <Metal/Metal.hpp>

namespace mem {
    constexpr uint64_t alignUp(uint64_t n, uint64_t alignment = 16) {
        return (n + alignment - 1) & ~(alignment - 1);
    }
};

namespace metal {
    // MARK: - Prototypes

    MTL::PixelFormat mapPixelFormat(RT64::RenderFormat format);

    // MARK: - Constants

    constexpr uint32_t MAX_COLOR_ATTACHMENT_COUNT = 8;
    constexpr uint32_t COLOR_COUNT = MAX_COLOR_ATTACHMENT_COUNT;
    constexpr uint32_t DEPTH_INDEX = COLOR_COUNT;
    constexpr uint32_t STENCIL_INDEX = DEPTH_INDEX + 1;
    constexpr uint32_t ATTACHMENT_COUNT = STENCIL_INDEX + 1;

    // MARK: - Helpers

    uint64_t hashForRenderPipelineDescriptor(MTL::RenderPipelineDescriptor *pipelineDesc, bool depthWriteEnabled) {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);

        std::uintptr_t sampleCount = pipelineDesc->sampleCount();
        XXH3_64bits_update(&xxh3, &sampleCount, sizeof(sampleCount));

        uint16_t pixel_info[ATTACHMENT_COUNT] = { 0 };
        for (uint32_t i = 0; i < COLOR_COUNT; i++) {
            if (auto colorAttachment = pipelineDesc->colorAttachments()->object(i)) {
                pixel_info[i] = static_cast<uint16_t>(colorAttachment->pixelFormat()) ^ colorAttachment->writeMask();
            }
        }

        if (pipelineDesc->depthAttachmentPixelFormat() != MTL::PixelFormatInvalid) {
            pixel_info[DEPTH_INDEX] = static_cast<uint16_t>(pipelineDesc->depthAttachmentPixelFormat()) ^ (depthWriteEnabled ? 1 : 0);
        }

        XXH3_64bits_update(&xxh3, pixel_info, sizeof(pixel_info));
        return XXH3_64bits_digest(&xxh3);
    }

    NS::UInteger alignmentForRenderFormat(MTL::Device *device, RT64::RenderFormat format) {
        const auto deviceAlignment = device->minimumLinearTextureAlignmentForPixelFormat(mapPixelFormat(format));

    #if TARGET_OS_TV
        auto minTexelBufferOffsetAlignment = 64;
    #elif TARGET_OS_IPHONE
        auto minTexelBufferOffsetAlignment = 64;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAlignment = 16;
        }
    #elif TARGET_OS_MAC
        auto minTexelBufferOffsetAlignment = 256;
        if (device->supportsFamily(MTL::GPUFamilyApple3)) {
            minTexelBufferOffsetAlignment = 16;
        }
    #endif

        return deviceAlignment ? deviceAlignment : minTexelBufferOffsetAlignment;
    }

    MTL::ScissorRect clampScissorRectIfNecessary(const RT64::RenderRect& rect, const RT64::MetalFramebuffer* targetFramebuffer) {
        // Always clamp the scissor rect to the render target dimensions.
        // RenderRect is signed, but Metal's rect is not. Use a signed max function, then cast to unsigned.
        NS::UInteger left = static_cast<NS::UInteger>(std::max(0, rect.left));
        NS::UInteger top = static_cast<NS::UInteger>(std::max(0, rect.top));
        NS::UInteger right = static_cast<NS::UInteger>(std::max(0, rect.right));
        NS::UInteger bottom = static_cast<NS::UInteger>(std::max(0, rect.bottom));

        if (left >= right || top >= bottom) {
            return MTL::ScissorRect({0u, 0u, 0u, 0u});
        }

        MTL::ScissorRect clampedRect = {
            left,
            top,
            right - left,
            top - bottom
        };

        if (!targetFramebuffer || targetFramebuffer->colorAttachments.empty()) {
            // No need to clamp
            return clampedRect;
        }

        // Always clamp to the attachment dimensions, to avoid Metal API error
        uint32_t maxWidth = INT_MAX;
        uint32_t maxHeight = INT_MAX;
        bool hasAttachments = false;

        for (const auto& attachment : targetFramebuffer->colorAttachments) {
            if (attachment) {
                if (const auto texture = attachment->getTexture()) {
                    maxWidth = std::min(static_cast<NS::UInteger>(maxWidth), texture->width());
                    maxHeight = std::min(static_cast<NS::UInteger>(maxHeight), texture->height());
                    hasAttachments = true;
                }
            }
        }

        // If no valid attachments found, return original rect
        if (!hasAttachments) {
            return clampedRect;
        }

        // Clamp width and height to fit within the render target
        if (clampedRect.x + clampedRect.width > maxWidth) {
            clampedRect.width = maxWidth > clampedRect.x ? maxWidth - clampedRect.x : 0;
        }

        if (clampedRect.y + clampedRect.height > maxHeight) {
            clampedRect.height = maxHeight > clampedRect.y ? maxHeight - clampedRect.y : 0;
        }

        return clampedRect;
    }

    // MARK: - Mapping RHI <> Metal

    MTL::DataType mapDataType(RT64::RenderDescriptorRangeType type) {
        switch (type) {
            case RT64::RenderDescriptorRangeType::TEXTURE:
            case RT64::RenderDescriptorRangeType::READ_WRITE_TEXTURE:
            case RT64::RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
                return MTL::DataTypeTexture;

            case RT64::RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
                return MTL::DataTypePrimitiveAccelerationStructure;

            case RT64::RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RT64::RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RT64::RenderDescriptorRangeType::CONSTANT_BUFFER:
                return MTL::DataTypePointer;

            case RT64::RenderDescriptorRangeType::SAMPLER:
                return MTL::DataTypeSampler;

            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    RT64::RenderFormat mapRenderFormat(MTL::PixelFormat format) {
            switch (format) {
                case MTL::PixelFormatInvalid:
                    return RT64::RenderFormat::UNKNOWN;
                case MTL::PixelFormatRGBA32Float:
                    return RT64::RenderFormat::R32G32B32A32_FLOAT;
                case MTL::PixelFormatRGBA32Uint:
                    return RT64::RenderFormat::R32G32B32A32_UINT;
                case MTL::PixelFormatRGBA32Sint:
                    return RT64::RenderFormat::R32G32B32A32_SINT;
                case MTL::PixelFormatRGBA16Float:
                    return RT64::RenderFormat::R16G16B16A16_FLOAT;
                case MTL::PixelFormatRGBA16Unorm:
                    return RT64::RenderFormat::R16G16B16A16_UNORM;
                case MTL::PixelFormatRGBA16Uint:
                    return RT64::RenderFormat::R16G16B16A16_UINT;
                case MTL::PixelFormatRGBA16Snorm:
                    return RT64::RenderFormat::R16G16B16A16_SNORM;
                case MTL::PixelFormatRGBA16Sint:
                    return RT64::RenderFormat::R16G16B16A16_SINT;
                case MTL::PixelFormatRG32Float:
                    return RT64::RenderFormat::R32G32_FLOAT;
                case MTL::PixelFormatRG32Uint:
                    return RT64::RenderFormat::R32G32_UINT;
                case MTL::PixelFormatRG32Sint:
                    return RT64::RenderFormat::R32G32_SINT;
                case MTL::PixelFormatRGBA8Unorm:
                    return RT64::RenderFormat::R8G8B8A8_UNORM;
                case MTL::PixelFormatRGBA8Uint:
                    return RT64::RenderFormat::R8G8B8A8_UINT;
                case MTL::PixelFormatRGBA8Snorm:
                    return RT64::RenderFormat::R8G8B8A8_SNORM;
                case MTL::PixelFormatRGBA8Sint:
                    return RT64::RenderFormat::R8G8B8A8_SINT;
                case MTL::PixelFormatBGRA8Unorm:
                    return RT64::RenderFormat::B8G8R8A8_UNORM;
                case MTL::PixelFormatRG16Float:
                    return RT64::RenderFormat::R16G16_FLOAT;
                case MTL::PixelFormatRG16Unorm:
                    return RT64::RenderFormat::R16G16_UNORM;
                case MTL::PixelFormatRG16Uint:
                    return RT64::RenderFormat::R16G16_UINT;
                case MTL::PixelFormatRG16Snorm:
                    return RT64::RenderFormat::R16G16_SNORM;
                case MTL::PixelFormatRG16Sint:
                    return RT64::RenderFormat::R16G16_SINT;
                case MTL::PixelFormatDepth32Float:
                    return RT64::RenderFormat::D32_FLOAT;
                case MTL::PixelFormatR32Float:
                    return RT64::RenderFormat::R32_FLOAT;
                case MTL::PixelFormatR32Uint:
                    return RT64::RenderFormat::R32_UINT;
                case MTL::PixelFormatR32Sint:
                    return RT64::RenderFormat::R32_SINT;
                case MTL::PixelFormatRG8Unorm:
                    return RT64::RenderFormat::R8G8_UNORM;
                case MTL::PixelFormatRG8Uint:
                    return RT64::RenderFormat::R8G8_UINT;
                case MTL::PixelFormatRG8Snorm:
                    return RT64::RenderFormat::R8G8_SNORM;
                case MTL::PixelFormatRG8Sint:
                    return RT64::RenderFormat::R8G8_SINT;
                case MTL::PixelFormatR16Float:
                    return RT64::RenderFormat::R16_FLOAT;
                case MTL::PixelFormatDepth16Unorm:
                    return RT64::RenderFormat::D16_UNORM;
                case MTL::PixelFormatR16Unorm:
                    return RT64::RenderFormat::R16_UNORM;
                case MTL::PixelFormatR16Uint:
                    return RT64::RenderFormat::R16_UINT;
                case MTL::PixelFormatR16Snorm:
                    return RT64::RenderFormat::R16_SNORM;
                case MTL::PixelFormatR16Sint:
                    return RT64::RenderFormat::R16_SINT;
                case MTL::PixelFormatR8Unorm:
                    return RT64::RenderFormat::R8_UNORM;
                case MTL::PixelFormatR8Uint:
                    return RT64::RenderFormat::R8_UINT;
                case MTL::PixelFormatR8Snorm:
                    return RT64::RenderFormat::R8_SNORM;
                case MTL::PixelFormatR8Sint:
                    return RT64::RenderFormat::R8_SINT;
                // Block compressed formats
                case MTL::PixelFormatBC1_RGBA:
                    return RT64::RenderFormat::BC1_UNORM;
                case MTL::PixelFormatBC1_RGBA_sRGB:
                    return RT64::RenderFormat::BC1_UNORM_SRGB;
                case MTL::PixelFormatBC2_RGBA:
                    return RT64::RenderFormat::BC2_UNORM;
                case MTL::PixelFormatBC2_RGBA_sRGB:
                    return RT64::RenderFormat::BC2_UNORM_SRGB;
                case MTL::PixelFormatBC3_RGBA:
                    return RT64::RenderFormat::BC3_UNORM;
                case MTL::PixelFormatBC3_RGBA_sRGB:
                    return RT64::RenderFormat::BC3_UNORM_SRGB;
                case MTL::PixelFormatBC4_RUnorm:
                    return RT64::RenderFormat::BC4_UNORM;
                case MTL::PixelFormatBC4_RSnorm:
                    return RT64::RenderFormat::BC4_SNORM;
                case MTL::PixelFormatBC5_RGUnorm:
                    return RT64::RenderFormat::BC5_UNORM;
                case MTL::PixelFormatBC5_RGSnorm:
                    return RT64::RenderFormat::BC5_SNORM;
                case MTL::PixelFormatBC6H_RGBFloat:
                    return RT64::RenderFormat::BC6H_SF16;
                case MTL::PixelFormatBC6H_RGBUfloat:
                    return RT64::RenderFormat::BC6H_UF16;
                case MTL::PixelFormatBC7_RGBAUnorm:
                    return RT64::RenderFormat::BC7_UNORM;
                case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
                    return RT64::RenderFormat::BC7_UNORM_SRGB;
                default:
                    assert(false && "Unknown Metal format.");
                    return RT64::RenderFormat::UNKNOWN;
            }
        }

    MTL::PixelFormat mapPixelFormat(RT64::RenderFormat format) {
        switch (format) {
            case RT64::RenderFormat::UNKNOWN:
                return MTL::PixelFormatInvalid;
            case RT64::RenderFormat::R32G32B32A32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RT64::RenderFormat::R32G32B32A32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RT64::RenderFormat::R32G32B32A32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RT64::RenderFormat::R32G32B32A32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RT64::RenderFormat::R32G32B32_TYPELESS:
                return MTL::PixelFormatRGBA32Float;
            case RT64::RenderFormat::R32G32B32_FLOAT:
                return MTL::PixelFormatRGBA32Float;
            case RT64::RenderFormat::R32G32B32_UINT:
                return MTL::PixelFormatRGBA32Uint;
            case RT64::RenderFormat::R32G32B32_SINT:
                return MTL::PixelFormatRGBA32Sint;
            case RT64::RenderFormat::R16G16B16A16_TYPELESS:
                return MTL::PixelFormatRGBA16Float;
            case RT64::RenderFormat::R16G16B16A16_FLOAT:
                return MTL::PixelFormatRGBA16Float;
            case RT64::RenderFormat::R16G16B16A16_UNORM:
                return MTL::PixelFormatRGBA16Unorm;
            case RT64::RenderFormat::R16G16B16A16_UINT:
                return MTL::PixelFormatRGBA16Uint;
            case RT64::RenderFormat::R16G16B16A16_SNORM:
                return MTL::PixelFormatRGBA16Snorm;
            case RT64::RenderFormat::R16G16B16A16_SINT:
                return MTL::PixelFormatRGBA16Sint;
            case RT64::RenderFormat::R32G32_TYPELESS:
                return MTL::PixelFormatRG32Float;
            case RT64::RenderFormat::R32G32_FLOAT:
                return MTL::PixelFormatRG32Float;
            case RT64::RenderFormat::R32G32_UINT:
                return MTL::PixelFormatRG32Uint;
            case RT64::RenderFormat::R32G32_SINT:
                return MTL::PixelFormatRG32Sint;
            case RT64::RenderFormat::R8G8B8A8_TYPELESS:
                return MTL::PixelFormatRGBA8Unorm;
            case RT64::RenderFormat::R8G8B8A8_UNORM:
                return MTL::PixelFormatRGBA8Unorm;
            case RT64::RenderFormat::R8G8B8A8_UINT:
                return MTL::PixelFormatRGBA8Uint;
            case RT64::RenderFormat::R8G8B8A8_SNORM:
                return MTL::PixelFormatRGBA8Snorm;
            case RT64::RenderFormat::R8G8B8A8_SINT:
                return MTL::PixelFormatRGBA8Sint;
            case RT64::RenderFormat::B8G8R8A8_UNORM:
                return MTL::PixelFormatBGRA8Unorm;
            case RT64::RenderFormat::R16G16_TYPELESS:
                return MTL::PixelFormatRG16Float;
            case RT64::RenderFormat::R16G16_FLOAT:
                return MTL::PixelFormatRG16Float;
            case RT64::RenderFormat::R16G16_UNORM:
                return MTL::PixelFormatRG16Unorm;
            case RT64::RenderFormat::R16G16_UINT:
                return MTL::PixelFormatRG16Uint;
            case RT64::RenderFormat::R16G16_SNORM:
                return MTL::PixelFormatRG16Snorm;
            case RT64::RenderFormat::R16G16_SINT:
                return MTL::PixelFormatRG16Sint;
            case RT64::RenderFormat::R32_TYPELESS:
                return MTL::PixelFormatR32Float;
            case RT64::RenderFormat::D32_FLOAT:
                return MTL::PixelFormatDepth32Float;
            case RT64::RenderFormat::R32_FLOAT:
                return MTL::PixelFormatR32Float;
            case RT64::RenderFormat::R32_UINT:
                return MTL::PixelFormatR32Uint;
            case RT64::RenderFormat::R32_SINT:
                return MTL::PixelFormatR32Sint;
            case RT64::RenderFormat::R8G8_TYPELESS:
                return MTL::PixelFormatRG8Unorm;
            case RT64::RenderFormat::R8G8_UNORM:
                return MTL::PixelFormatRG8Unorm;
            case RT64::RenderFormat::R8G8_UINT:
                return MTL::PixelFormatRG8Uint;
            case RT64::RenderFormat::R8G8_SNORM:
                return MTL::PixelFormatRG8Snorm;
            case RT64::RenderFormat::R8G8_SINT:
                return MTL::PixelFormatRG8Sint;
            case RT64::RenderFormat::R16_TYPELESS:
                return MTL::PixelFormatR16Float;
            case RT64::RenderFormat::R16_FLOAT:
                return MTL::PixelFormatR16Float;
            case RT64::RenderFormat::D16_UNORM:
                return MTL::PixelFormatDepth16Unorm;
            case RT64::RenderFormat::R16_UNORM:
                return MTL::PixelFormatR16Unorm;
            case RT64::RenderFormat::R16_UINT:
                return MTL::PixelFormatR16Uint;
            case RT64::RenderFormat::R16_SNORM:
                return MTL::PixelFormatR16Snorm;
            case RT64::RenderFormat::R16_SINT:
                return MTL::PixelFormatR16Sint;
            case RT64::RenderFormat::R8_TYPELESS:
                return MTL::PixelFormatR8Unorm;
            case RT64::RenderFormat::R8_UNORM:
                return MTL::PixelFormatR8Unorm;
            case RT64::RenderFormat::R8_UINT:
                return MTL::PixelFormatR8Uint;
            case RT64::RenderFormat::R8_SNORM:
                return MTL::PixelFormatR8Snorm;
            case RT64::RenderFormat::R8_SINT:
                return MTL::PixelFormatR8Sint;
            // Block compressed formats
           case RT64::RenderFormat::BC1_TYPELESS:
               return MTL::PixelFormatBC1_RGBA;
           case RT64::RenderFormat::BC1_UNORM:
               return MTL::PixelFormatBC1_RGBA;
           case RT64::RenderFormat::BC1_UNORM_SRGB:
               return MTL::PixelFormatBC1_RGBA_sRGB;
           case RT64::RenderFormat::BC2_TYPELESS:
               return MTL::PixelFormatBC2_RGBA;
           case RT64::RenderFormat::BC2_UNORM:
               return MTL::PixelFormatBC2_RGBA;
           case RT64::RenderFormat::BC2_UNORM_SRGB:
               return MTL::PixelFormatBC2_RGBA_sRGB;
           case RT64::RenderFormat::BC3_TYPELESS:
               return MTL::PixelFormatBC3_RGBA;
           case RT64::RenderFormat::BC3_UNORM:
               return MTL::PixelFormatBC3_RGBA;
           case RT64::RenderFormat::BC3_UNORM_SRGB:
               return MTL::PixelFormatBC3_RGBA_sRGB;
           case RT64::RenderFormat::BC4_TYPELESS:
               return MTL::PixelFormatBC4_RUnorm;
           case RT64::RenderFormat::BC4_UNORM:
               return MTL::PixelFormatBC4_RUnorm;
           case RT64::RenderFormat::BC4_SNORM:
               return MTL::PixelFormatBC4_RSnorm;
           case RT64::RenderFormat::BC5_TYPELESS:
               return MTL::PixelFormatBC5_RGUnorm;
           case RT64::RenderFormat::BC5_UNORM:
               return MTL::PixelFormatBC5_RGUnorm;
           case RT64::RenderFormat::BC5_SNORM:
               return MTL::PixelFormatBC5_RGSnorm;
           case RT64::RenderFormat::BC6H_TYPELESS:
               return MTL::PixelFormatBC6H_RGBFloat;
           case RT64::RenderFormat::BC6H_UF16:
               return MTL::PixelFormatBC6H_RGBUfloat;
           case RT64::RenderFormat::BC6H_SF16:
               return MTL::PixelFormatBC6H_RGBFloat;
           case RT64::RenderFormat::BC7_TYPELESS:
               return MTL::PixelFormatBC7_RGBAUnorm;
           case RT64::RenderFormat::BC7_UNORM:
               return MTL::PixelFormatBC7_RGBAUnorm;
           case RT64::RenderFormat::BC7_UNORM_SRGB:
               return MTL::PixelFormatBC7_RGBAUnorm_sRGB;
            default:
                assert(false && "Unknown format.");
                return MTL::PixelFormatInvalid;
        }
    }

    MTL::VertexFormat mapVertexFormat(RT64::RenderFormat format) {
        switch (format) {
            case RT64::RenderFormat::UNKNOWN:
                return MTL::VertexFormatInvalid;
            case RT64::RenderFormat::R32G32B32A32_FLOAT:
                return MTL::VertexFormatFloat4;
            case RT64::RenderFormat::R32G32B32A32_UINT:
                return MTL::VertexFormatUInt4;
            case RT64::RenderFormat::R32G32B32A32_SINT:
                return MTL::VertexFormatInt4;
            case RT64::RenderFormat::R32G32B32_FLOAT:
                return MTL::VertexFormatFloat3;
            case RT64::RenderFormat::R32G32B32_UINT:
                return MTL::VertexFormatUInt3;
            case RT64::RenderFormat::R32G32B32_SINT:
                return MTL::VertexFormatInt3;
            case RT64::RenderFormat::R16G16B16A16_FLOAT:
                return MTL::VertexFormatHalf4;
            case RT64::RenderFormat::R16G16B16A16_UNORM:
                return MTL::VertexFormatUShort4Normalized;
            case RT64::RenderFormat::R16G16B16A16_UINT:
                return MTL::VertexFormatUShort4;
            case RT64::RenderFormat::R16G16B16A16_SNORM:
                return MTL::VertexFormatShort4Normalized;
            case RT64::RenderFormat::R16G16B16A16_SINT:
                return MTL::VertexFormatShort4;
            case RT64::RenderFormat::R32G32_FLOAT:
                return MTL::VertexFormatFloat2;
            case RT64::RenderFormat::R32G32_UINT:
                return MTL::VertexFormatUInt2;
            case RT64::RenderFormat::R32G32_SINT:
                return MTL::VertexFormatInt2;
            case RT64::RenderFormat::R8G8B8A8_UNORM:
                return MTL::VertexFormatUChar4Normalized;
            case RT64::RenderFormat::R8G8B8A8_UINT:
                return MTL::VertexFormatUChar4;
            case RT64::RenderFormat::R8G8B8A8_SNORM:
                return MTL::VertexFormatChar4Normalized;
            case RT64::RenderFormat::R8G8B8A8_SINT:
                return MTL::VertexFormatChar4;
            case RT64::RenderFormat::R16G16_FLOAT:
                return MTL::VertexFormatHalf2;
            case RT64::RenderFormat::R16G16_UNORM:
                return MTL::VertexFormatUShort2Normalized;
            case RT64::RenderFormat::R16G16_UINT:
                return MTL::VertexFormatUShort2;
            case RT64::RenderFormat::R16G16_SNORM:
                return MTL::VertexFormatShort2Normalized;
            case RT64::RenderFormat::R16G16_SINT:
                return MTL::VertexFormatShort2;
            case RT64::RenderFormat::R32_FLOAT:
                return MTL::VertexFormatFloat;
            case RT64::RenderFormat::R32_UINT:
                return MTL::VertexFormatUInt;
            case RT64::RenderFormat::R32_SINT:
                return MTL::VertexFormatInt;
            case RT64::RenderFormat::R8G8_UNORM:
                return MTL::VertexFormatUChar2Normalized;
            case RT64::RenderFormat::R8G8_UINT:
                return MTL::VertexFormatUChar2;
            case RT64::RenderFormat::R8G8_SNORM:
                return MTL::VertexFormatChar2Normalized;
            case RT64::RenderFormat::R8G8_SINT:
                return MTL::VertexFormatChar2;
            case RT64::RenderFormat::R16_FLOAT:
                return MTL::VertexFormatHalf;
            case RT64::RenderFormat::R16_UNORM:
                return MTL::VertexFormatUShortNormalized;
            case RT64::RenderFormat::R16_UINT:
                return MTL::VertexFormatUShort;
            case RT64::RenderFormat::R16_SNORM:
                return MTL::VertexFormatShortNormalized;
            case RT64::RenderFormat::R16_SINT:
                return MTL::VertexFormatShort;
            case RT64::RenderFormat::R8_UNORM:
                return MTL::VertexFormatUCharNormalized;
            case RT64::RenderFormat::R8_UINT:
                return MTL::VertexFormatUChar;
            case RT64::RenderFormat::R8_SNORM:
                return MTL::VertexFormatCharNormalized;
            case RT64::RenderFormat::R8_SINT:
                return MTL::VertexFormatChar;
            default:
                assert(false && "Unsupported vertex format.");
                return MTL::VertexFormatInvalid;
        }
    }

    MTL::IndexType mapIndexFormat(RT64::RenderFormat format) {
        switch (format) {
            case RT64::RenderFormat::R16_UINT:
                return MTL::IndexTypeUInt16;
            case RT64::RenderFormat::R32_UINT:
                return MTL::IndexTypeUInt32;
            default:
                assert(false && "Format is not supported as an index type.");
                return MTL::IndexTypeUInt16;
        }
    }

    MTL::TextureType mapTextureType(RT64::RenderTextureDimension dimension, RT64::RenderSampleCounts sampleCount) {
        switch (dimension) {
            case RT64::RenderTextureDimension::TEXTURE_1D:
                assert(sampleCount <= 1 && "Multisampling not supported for 1D textures");
                return MTL::TextureType1D;
            case RT64::RenderTextureDimension::TEXTURE_2D:
                return (sampleCount > 1) ? MTL::TextureType2DMultisample : MTL::TextureType2D;
            case RT64::RenderTextureDimension::TEXTURE_3D:
                assert(sampleCount <= 1 && "Multisampling not supported for 3D textures");
                return MTL::TextureType3D;
            default:
                assert(false && "Unknown resource dimension.");
                return MTL::TextureType2D;
        }
    }

    MTL::CullMode mapCullMode(RT64::RenderCullMode cullMode) {
        switch (cullMode) {
            case RT64::RenderCullMode::NONE:
                return MTL::CullModeNone;
            case RT64::RenderCullMode::FRONT:
                return MTL::CullModeFront;
            case RT64::RenderCullMode::BACK:
                return MTL::CullModeBack;
            default:
                assert(false && "Unknown cull mode.");
                return MTL::CullModeNone;
        }
    }

    MTL::PrimitiveTopologyClass mapPrimitiveTopologyClass(RT64::RenderPrimitiveTopology topology) {
        switch (topology) {
            case RT64::RenderPrimitiveTopology::POINT_LIST:
                return MTL::PrimitiveTopologyClassPoint;
            case RT64::RenderPrimitiveTopology::LINE_LIST:
                return MTL::PrimitiveTopologyClassLine;
            case RT64::RenderPrimitiveTopology::TRIANGLE_LIST:
                return MTL::PrimitiveTopologyClassTriangle;
            default:
                assert(false && "Unknown primitive topology type.");
                return MTL::PrimitiveTopologyClassPoint;
        }
    }

    MTL::VertexStepFunction mapVertexStepFunction(RT64::RenderInputSlotClassification classification) {
        switch (classification) {
            case RT64::RenderInputSlotClassification::PER_VERTEX_DATA:
                return MTL::VertexStepFunctionPerVertex;
            case RT64::RenderInputSlotClassification::PER_INSTANCE_DATA:
                return MTL::VertexStepFunctionPerInstance;
            default:
                assert(false && "Unknown input classification.");
                return MTL::VertexStepFunctionPerVertex;
        }
    }

    MTL::BlendFactor mapBlendFactor(RT64::RenderBlend blend) {
        switch (blend) {
            case RT64::RenderBlend::ZERO:
                return MTL::BlendFactorZero;
            case RT64::RenderBlend::ONE:
                return MTL::BlendFactorOne;
            case RT64::RenderBlend::SRC_COLOR:
                return MTL::BlendFactorSourceColor;
            case RT64::RenderBlend::INV_SRC_COLOR:
                return MTL::BlendFactorOneMinusSourceColor;
            case RT64::RenderBlend::SRC_ALPHA:
                return MTL::BlendFactorSourceAlpha;
            case RT64::RenderBlend::INV_SRC_ALPHA:
                return MTL::BlendFactorOneMinusSourceAlpha;
            case RT64::RenderBlend::DEST_ALPHA:
                return MTL::BlendFactorDestinationAlpha;
            case RT64::RenderBlend::INV_DEST_ALPHA:
                return MTL::BlendFactorOneMinusDestinationAlpha;
            case RT64::RenderBlend::DEST_COLOR:
                return MTL::BlendFactorDestinationColor;
            case RT64::RenderBlend::INV_DEST_COLOR:
                return MTL::BlendFactorOneMinusDestinationColor;
            case RT64::RenderBlend::SRC_ALPHA_SAT:
                return MTL::BlendFactorSourceAlphaSaturated;
            case RT64::RenderBlend::BLEND_FACTOR:
                return MTL::BlendFactorBlendColor;
            case RT64::RenderBlend::INV_BLEND_FACTOR:
                return MTL::BlendFactorOneMinusBlendColor;
            case RT64::RenderBlend::SRC1_COLOR:
                return MTL::BlendFactorSource1Color;
            case RT64::RenderBlend::INV_SRC1_COLOR:
                return MTL::BlendFactorOneMinusSource1Color;
            case RT64::RenderBlend::SRC1_ALPHA:
                return MTL::BlendFactorSource1Alpha;
            case RT64::RenderBlend::INV_SRC1_ALPHA:
                return MTL::BlendFactorOneMinusSource1Alpha;
            default:
                assert(false && "Unknown blend factor.");
                return MTL::BlendFactorZero;
        }
    }

    MTL::BlendOperation mapBlendOperation(RT64::RenderBlendOperation operation) {
        switch (operation) {
            case RT64::RenderBlendOperation::ADD:
                return MTL::BlendOperationAdd;
            case RT64::RenderBlendOperation::SUBTRACT:
                return MTL::BlendOperationSubtract;
            case RT64::RenderBlendOperation::REV_SUBTRACT:
                return MTL::BlendOperationReverseSubtract;
            case RT64::RenderBlendOperation::MIN:
                return MTL::BlendOperationMin;
            case RT64::RenderBlendOperation::MAX:
                return MTL::BlendOperationMax;
            default:
                assert(false && "Unknown blend operation.");
                return MTL::BlendOperationAdd;
        }
    }

    // Metal does not support Logic Operations in the public API.

    MTL::CompareFunction mapCompareFunction(RT64::RenderComparisonFunction function) {
        switch (function) {
            case RT64::RenderComparisonFunction::NEVER:
                return MTL::CompareFunctionNever;
            case RT64::RenderComparisonFunction::LESS:
                return MTL::CompareFunctionLess;
            case RT64::RenderComparisonFunction::EQUAL:
                return MTL::CompareFunctionEqual;
            case RT64::RenderComparisonFunction::LESS_EQUAL:
                return MTL::CompareFunctionLessEqual;
            case RT64::RenderComparisonFunction::GREATER:
                return MTL::CompareFunctionGreater;
            case RT64::RenderComparisonFunction::NOT_EQUAL:
                return MTL::CompareFunctionNotEqual;
            case RT64::RenderComparisonFunction::GREATER_EQUAL:
                return MTL::CompareFunctionGreaterEqual;
            case RT64::RenderComparisonFunction::ALWAYS:
                return MTL::CompareFunctionAlways;
            default:
                assert(false && "Unknown comparison function.");
                return MTL::CompareFunctionNever;
        }
    }

    MTL::SamplerMinMagFilter mapSamplerMinMagFilter(RT64::RenderFilter filter) {
        switch (filter) {
            case RT64::RenderFilter::NEAREST:
                return MTL::SamplerMinMagFilterNearest;
            case RT64::RenderFilter::LINEAR:
                return MTL::SamplerMinMagFilterLinear;
            default:
                assert(false && "Unknown filter.");
                return MTL::SamplerMinMagFilterNearest;
        }
    }

    MTL::SamplerMipFilter mapSamplerMipFilter(RT64::RenderMipmapMode mode) {
        switch (mode) {
            case RT64::RenderMipmapMode::NEAREST:
                return MTL::SamplerMipFilterNearest;
            case RT64::RenderMipmapMode::LINEAR:
                return MTL::SamplerMipFilterLinear;
            default:
                assert(false && "Unknown mipmap mode.");
                return MTL::SamplerMipFilterNearest;
        }
    }

    MTL::SamplerAddressMode mapSamplerAddressMode(RT64::RenderTextureAddressMode mode) {
        switch (mode) {
            case RT64::RenderTextureAddressMode::WRAP:
                return MTL::SamplerAddressModeRepeat;
            case RT64::RenderTextureAddressMode::MIRROR:
                return MTL::SamplerAddressModeMirrorRepeat;
            case RT64::RenderTextureAddressMode::CLAMP:
                return MTL::SamplerAddressModeClampToEdge;
            case RT64::RenderTextureAddressMode::BORDER:
                return MTL::SamplerAddressModeClampToBorderColor;
            case RT64::RenderTextureAddressMode::MIRROR_ONCE:
                return MTL::SamplerAddressModeMirrorClampToEdge;
            default:
                assert(false && "Unknown texture address mode.");
                return MTL::SamplerAddressModeRepeat;
        }
    }

    MTL::SamplerBorderColor mapSamplerBorderColor(RT64::RenderBorderColor color) {
        switch (color) {
            case RT64::RenderBorderColor::TRANSPARENT_BLACK:
                return MTL::SamplerBorderColorTransparentBlack;
            case RT64::RenderBorderColor::OPAQUE_BLACK:
                return MTL::SamplerBorderColorOpaqueBlack;
            case RT64::RenderBorderColor::OPAQUE_WHITE:
                return MTL::SamplerBorderColorOpaqueWhite;
            default:
                assert(false && "Unknown border color.");
                return MTL::SamplerBorderColorTransparentBlack;
        }
    }

    MTL::ResourceOptions mapResourceOption(RT64::RenderHeapType heapType) {
        switch (heapType) {
            case RT64::RenderHeapType::DEFAULT:
                return MTL::ResourceStorageModePrivate;
            case RT64::RenderHeapType::UPLOAD:
                return MTL::ResourceStorageModeShared;
            case RT64::RenderHeapType::READBACK:
                return MTL::ResourceStorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTL::ResourceStorageModePrivate;
        }
    }

    MTL::StorageMode mapStorageMode(RT64::RenderHeapType heapType) {
        switch (heapType) {
            case RT64::RenderHeapType::DEFAULT:
                return MTL::StorageModePrivate;
            case RT64::RenderHeapType::UPLOAD:
                return MTL::StorageModeShared;
            case RT64::RenderHeapType::READBACK:
                return MTL::StorageModeShared;
            default:
                assert(false && "Unknown heap type.");
                return MTL::StorageModePrivate;
        }
    }

    MTL::ClearColor mapClearColor(RT64::RenderColor color) {
        return MTL::ClearColor(color.r, color.g, color.b, color.a);
    }

    MTL::ResourceUsage mapResourceUsage(RT64::RenderDescriptorRangeType type) {
        switch (type) {
            case RT64::RenderDescriptorRangeType::TEXTURE:
            case RT64::RenderDescriptorRangeType::FORMATTED_BUFFER:
            case RT64::RenderDescriptorRangeType::ACCELERATION_STRUCTURE:
            case RT64::RenderDescriptorRangeType::STRUCTURED_BUFFER:
            case RT64::RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
            case RT64::RenderDescriptorRangeType::CONSTANT_BUFFER:
            case RT64::RenderDescriptorRangeType::SAMPLER:
                return MTL::ResourceUsageRead;

            case RT64::RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
            case RT64::RenderDescriptorRangeType::READ_WRITE_TEXTURE:
                return MTL::ResourceUsageRead | MTL::ResourceUsageWrite;
            default:
                assert(false && "Unknown descriptor range type.");
                return MTL::DataTypeNone;
        }
    }

    MTL::TextureUsage mapTextureUsageFromBufferFlags(RT64::RenderBufferFlags flags) {
        MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
        usage |= (flags & RT64::RenderBufferFlag::UNORDERED_ACCESS) ? MTL::TextureUsageShaderWrite : MTL::TextureUsageUnknown;

        return usage;
    }

    MTL::TextureUsage mapTextureUsage(RT64::RenderTextureFlags flags) {
        MTL::TextureUsage usage = MTL::TextureUsageShaderRead;

        if (flags & RT64::RenderTextureFlag::RENDER_TARGET)
            usage |= MTL::TextureUsageRenderTarget;

        if (flags & RT64::RenderTextureFlag::DEPTH_TARGET)
            usage |= MTL::TextureUsageRenderTarget;

        if (flags & RT64::RenderTextureFlag::UNORDERED_ACCESS)
            usage |= MTL::TextureUsageShaderWrite;

        return usage;
    }

    MTL::TextureSwizzle mapTextureSwizzle(RT64::RenderSwizzle swizzle) {
        switch (swizzle) {
        case RT64::RenderSwizzle::ZERO:
            return MTL::TextureSwizzleZero;
        case RT64::RenderSwizzle::ONE:
            return MTL::TextureSwizzleOne;
        case RT64::RenderSwizzle::R:
            return MTL::TextureSwizzleRed;
        case RT64::RenderSwizzle::G:
            return MTL::TextureSwizzleGreen;
        case RT64::RenderSwizzle::B:
            return MTL::TextureSwizzleBlue;
        case RT64::RenderSwizzle::A:
            return MTL::TextureSwizzleAlpha;
        default:
            assert(false && "Unknown swizzle type.");
            return MTL::TextureSwizzleRed;
        }
    }

    MTL::TextureSwizzleChannels mapTextureSwizzleChannels(RT64::RenderComponentMapping mapping) {
    #define convert(v, d) \
        v == RT64::RenderSwizzle::IDENTITY ? MTL::TextureSwizzle##d : mapTextureSwizzle(v)
        return MTL::TextureSwizzleChannels(convert(mapping.r, Red), convert(mapping.g, Green), convert(mapping.b, Blue), convert(mapping.a, Alpha));
    #undef convert
    }

    MTL::ColorWriteMask mapColorWriteMask(uint8_t mask) {
        MTL::ColorWriteMask metalMask = MTL::ColorWriteMaskNone;
        
        if (mask & static_cast<uint8_t>(RT64::RenderColorWriteEnable::RED))
            metalMask |= MTL::ColorWriteMaskRed;
        if (mask & static_cast<uint8_t>(RT64::RenderColorWriteEnable::GREEN))
            metalMask |= MTL::ColorWriteMaskGreen;
        if (mask & static_cast<uint8_t>(RT64::RenderColorWriteEnable::BLUE))
            metalMask |= MTL::ColorWriteMaskBlue;
        if (mask & static_cast<uint8_t>(RT64::RenderColorWriteEnable::ALPHA))
            metalMask |= MTL::ColorWriteMaskAlpha;
        
        return metalMask;
    }
};
