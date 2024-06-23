//
// RT64
//

#include "rt64_inspector.h"

#include <algorithm>
#include <iomanip>

#include "im3d/im3d.h"
#include "im3d/im3d_math.h"
#include "imgui/imgui.h"
#include "implot/implot.h"

#if defined(_WIN32)
#   include "imgui/backends/imgui_impl_dx12.h"
#   include "imgui/backends/imgui_impl_win32.h"
#   include "utf8conv/utf8conv.h"
#endif

#if defined(_WIN32)
#   include "d3d12/rt64_d3d12.h"
#endif

#if defined(RT64_BUILD_PLUGIN)
#   include "api/rt64_api_common.h"
#endif

// Volk must be included before the ImGui Vulkan backend.
#include "vulkan/rt64_vulkan.h"
#include "imgui/backends/imgui_impl_vulkan.h"

static std::string IniFilenameUTF8;

#ifdef _WIN32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

static void checkVulkanResult(VkResult res) {
    if (res == 0) {
        return;
    }

    fprintf(stderr, "ImGui Vulkan Backend failed with error code 0x%X.\n", res);
}

namespace RT64 {
    struct VulkanContext {
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        ~VulkanContext() {
            if (renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, renderPass, nullptr);
            }

            if (descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            }
        }
    };

    // Inspector

    Inspector::Inspector(RenderDevice *device, const RenderSwapChain *swapChain, UserConfiguration::GraphicsAPI graphicsAPI) {
        assert(device != nullptr);
        assert(swapChain != nullptr);

        this->device = device;
        this->swapChain = swapChain;
        this->graphicsAPI = graphicsAPI;

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();

#   if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
        RenderWindow renderWindow = swapChain->getWindow();
        ImGui_ImplWin32_Init(renderWindow);
#   endif

        switch (graphicsAPI) {
        case UserConfiguration::GraphicsAPI::D3D12: {
#       if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
            D3D12Device *interfaceDevice = static_cast<D3D12Device *>(device);
            RenderDescriptorRange descriptorRange(RenderDescriptorRangeType::TEXTURE, 0, 1);
            descriptorSet = interfaceDevice->createDescriptorSet(RenderDescriptorSetDesc(&descriptorRange, 1));

            D3D12DescriptorSet *interfaceDescriptorSet = static_cast<D3D12DescriptorSet *>(descriptorSet.get());
            const D3D12SwapChain *interfaceSwapChain = static_cast<const D3D12SwapChain *>(swapChain);
            const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = interfaceDevice->descriptorHeapAllocator->getShaderCPUHandleAt(interfaceDescriptorSet->allocatorOffset);
            const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = interfaceDevice->descriptorHeapAllocator->getShaderGPUHandleAt(interfaceDescriptorSet->allocatorOffset);
            ImGui_ImplDX12_Init(interfaceDevice->d3d, 2, interfaceSwapChain->nativeFormat, interfaceDevice->descriptorHeapAllocator->shaderHeap, cpuHandle, gpuHandle);
#       else
            assert(false && "Unsupported Graphics API.");
            return;
#       endif
            break;
        }
        case UserConfiguration::GraphicsAPI::Vulkan: {
            VulkanDevice *interfaceDevice = static_cast<VulkanDevice *>(device);
            const VulkanSwapChain *interfaceSwapChain = static_cast<const VulkanSwapChain *>(swapChain);
            ImGui_ImplVulkan_LoadFunctions([](const char *functionName, void *vulkanInstance) {
                return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkanInstance)), functionName);
            }, &interfaceDevice->renderInterface->instance);

            std::unordered_map<VkDescriptorType, uint32_t> typeCounts;
            typeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 1;
            
            vulkanContext = std::make_unique<VulkanContext>();
            vulkanContext->device = interfaceDevice->vk;
            vulkanContext->renderPass = VulkanGraphicsPipeline::createRenderPass(interfaceDevice, &interfaceSwapChain->pickedSurfaceFormat.format, 1, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT);
            vulkanContext->descriptorPool = VulkanDescriptorSet::createDescriptorPool(interfaceDevice, typeCounts);

            ImGui_ImplVulkan_InitInfo initInfo = {};
            initInfo.Instance = interfaceDevice->renderInterface->instance;
            initInfo.PhysicalDevice = interfaceDevice->physicalDevice;
            initInfo.Device = vulkanContext->device;
            initInfo.QueueFamily = interfaceSwapChain->commandQueue->familyIndex;
            initInfo.Queue = interfaceSwapChain->commandQueue->queue->vk;
            initInfo.DescriptorPool = vulkanContext->descriptorPool;
            initInfo.RenderPass = vulkanContext->renderPass;
            initInfo.Subpass = 0;
            initInfo.MinImageCount = 2;
            initInfo.ImageCount = 2;
            initInfo.CheckVkResultFn = &checkVulkanResult;

            ImGui_ImplVulkan_Init(&initInfo);
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            return;
        }
    }

    Inspector::~Inspector() {
        switch (graphicsAPI) {
        case UserConfiguration::GraphicsAPI::D3D12: {
#       if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
            ImGui_ImplDX12_Shutdown();
#       else
            assert(false && "Unsupported Graphics API.");
#       endif
            break;
        }
        case UserConfiguration::GraphicsAPI::Vulkan: {
            ImGui_ImplVulkan_Shutdown();
            vulkanContext.reset(nullptr);
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
        }

#   if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
        ImGui_ImplWin32_Shutdown();
#   else
        // if mupen64plus, dont assert...
        //assert(false && "Unimplemented.");
#   endif

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void Inspector::setIniPath(const std::filesystem::path &path) {
        ImGuiIO &io = ImGui::GetIO();
        IniFilenameUTF8 = path.u8string();
        io.IniFilename = IniFilenameUTF8.c_str();
    }
    
    void Inspector::newFrame(RenderWorker *worker) {
        assert(worker != nullptr);

        frameMutex.lock();

#   if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
        ImGui_ImplWin32_NewFrame();
#   elif defined(RT64_BUILD_PLUGIN)
        // ????????
        // if mupen64plus == true, dont assert....
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)window_width, (float)window_height);
#   else
        //assert(false && "Unimplemented.");
#   endif

        switch (graphicsAPI) {
        case UserConfiguration::GraphicsAPI::D3D12: {
#       if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
            ImGui_ImplDX12_NewFrame();
#       else
            assert(false && "Unsupported Graphics API.");
#       endif
            break;
        }
        case UserConfiguration::GraphicsAPI::Vulkan: {
            ImGui_ImplVulkan_NewFrame();
            break;
        }
        default:
            assert(false && "Unknown Graphics API.");
            break;
        }

        ImGui::NewFrame();
        Im3d::NewFrame();
    }

    void Inspector::endFrame() {
        ImGui::Render();
        Im3d::EndFrame();
        frameMutex.unlock();
    }

    void Inspector::draw(RenderCommandList *commandList) {
        const std::lock_guard<std::mutex> frameLock(frameMutex);
        ImDrawData *drawData = ImGui::GetDrawData();
        if (drawData != nullptr) {
            switch (graphicsAPI) {
            case UserConfiguration::GraphicsAPI::D3D12: {
#       if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)
                D3D12CommandList *interfaceCommandList = static_cast<D3D12CommandList *>(commandList);
                interfaceCommandList->checkDescriptorHeaps();
                ImGui_ImplDX12_RenderDrawData(drawData, interfaceCommandList->d3d);
#       else
                assert(false && "Unsupported Graphics API.");
#       endif
                break;
            }
            case UserConfiguration::GraphicsAPI::Vulkan: {
                VulkanCommandList *interfaceCommandList = static_cast<VulkanCommandList *>(commandList);
                ImGui_ImplVulkan_RenderDrawData(drawData, interfaceCommandList->vk);
                break;
            }
            default:
                assert(false && "Unknown Graphics API.");
                break;
            }
        }
    }

#ifdef _WIN32
    bool Inspector::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        return ImGui_ImplWin32_WndProcHandler(swapChain->getWindow(), msg, wParam, lParam);
    }
#endif
};