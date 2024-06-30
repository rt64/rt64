//
// RT64
//

#pragma once

#include "common/rt64_user_configuration.h"
#include "hle/rt64_vi.h"

#include "rt64_descriptor_sets.h"
#include "rt64_shader_library.h"

namespace RT64 {
    struct VIRenderer {
        std::unique_ptr<VideoInterfaceDescriptorSet> descriptorSet;
        const RenderSampler *descriptorSetSampler = nullptr;

        struct RenderParams {
            RenderDevice *device;
            RenderCommandList *commandList;
            RenderTexture *texture;
            const RenderSwapChain *swapChain;
            const ShaderLibrary *shaderLibrary;
            RenderFormat textureFormat;
            hlslpp::float2 resolutionScale;
            uint32_t downsamplingScale;
            uint32_t textureWidth;
            uint32_t textureHeight;
            UserConfiguration::Filtering filtering;
            const VI *vi;
        };

        VIRenderer();
        ~VIRenderer();
        void render(const RenderParams &p);
        static void getViewportAndScissor(const RenderSwapChain *swapChain, const VI &vi, hlslpp::float2 resolutionScale, uint32_t downsamplingScale, RenderViewport &viewport, RenderRect &scissor, hlslpp::float2 &fbHdRegion);
    };
};