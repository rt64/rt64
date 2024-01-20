//
// RT64
//

#pragma once

#include <mutex>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "common/rt64_common.h"
#include "common/rt64_user_configuration.h"
#include "render/rt64_render_worker.h"
#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct VulkanContext;

    struct Inspector {
        RenderDevice *device = nullptr;
        const RenderSwapChain *swapChain = nullptr;
        std::unique_ptr<RenderDescriptorSet> descriptorSet;
        std::unique_ptr<VulkanContext> vulkanContext;
        UserConfiguration::GraphicsAPI graphicsAPI;
        std::mutex frameMutex;

        Inspector(RenderDevice *device, const RenderSwapChain *swapChain, UserConfiguration::GraphicsAPI graphicsAPI);
        ~Inspector();
        void setIniPath(const std::filesystem::path &path);
        void newFrame(RenderWorker *worker);
        void endFrame();
        void draw(RenderCommandList *commandList);
#   ifdef _WIN32
        bool handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
#   endif
    };
};