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
            RenderDevice *device = nullptr;
            RenderCommandList *commandList = nullptr;
            RenderTexture *texture = nullptr;
            const RenderSwapChain *swapChain = nullptr;
            const ShaderLibrary *shaderLibrary = nullptr;
            RenderFormat textureFormat = RenderFormat::UNKNOWN;
            hlslpp::float2 resolutionScale;
            uint32_t downsamplingScale = 0;
            uint32_t textureWidth = 0;
            uint32_t textureHeight = 0;
            UserConfiguration::Filtering filtering = UserConfiguration::Filtering::Linear;
            const VI *vi = nullptr;
        };

        VIRenderer();
        ~VIRenderer();
        void render(const RenderParams &p);
        static void getViewportAndScissor(const RenderSwapChain *swapChain, const VI &vi, hlslpp::float2 resolutionScale, uint32_t downsamplingScale, RenderViewport &viewport, RenderRect &scissor, hlslpp::float2 &fbHdRegion);
    };
};