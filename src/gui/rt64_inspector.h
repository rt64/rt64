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
#include "hle/rt64_application_window.h"
#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct VulkanContext;

    struct Inspector {
        RenderDevice *device = nullptr;
        const RenderSwapChain *swapChain = nullptr;
        UserConfiguration::GraphicsAPI graphicsAPI;
        SDL_Window *sdlWindow = nullptr;
        std::unique_ptr<RenderDescriptorSet> descriptorSet;
        std::unique_ptr<VulkanContext> vulkanContext;
        std::mutex frameMutex;

        Inspector(RenderDevice *device, const RenderSwapChain *swapChain, UserConfiguration::GraphicsAPI graphicsAPI, SDL_Window *sdlWindow);
        ~Inspector();
        void setIniPath(const std::filesystem::path &path);
        void newFrame(RenderWorker *worker);
        void endFrame();
        void draw(RenderCommandList *commandList);
#   ifdef _WIN32
        bool handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
#   endif
        bool handleSdlEvent(SDL_Event *event);
    };
};