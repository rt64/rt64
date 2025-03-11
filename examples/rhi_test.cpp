#include "rhi/rt64_render_interface.h"

namespace RT64 {
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
    extern std::unique_ptr<RenderInterface> CreateMetalInterface();
}

std::unique_ptr<RT64::RenderInterface> CreateRenderInterface() {
#if defined(_WIN32)
    const bool useVulkan = true; // Or derive this from configuration or runtime check.
    if (!useVulkan) {
        return RT64::CreateD3D12Interface();
    }
    // Fallback to Vulkan if D3D12 is not chosen or available.
    return RT64::CreateVulkanInterface();
#elif defined(__APPLE__)
    const bool useMVK = false; // Or derive this from configuration or runtime check.
    if (useMVK) {
        return RT64::CreateVulkanInterface();
    }

    return RT64::CreateMetalInterface();
#else
    return RT64::CreateVulkanInterface();
#endif
}

int main(int argc, char** argv) {
    auto renderInterface = CreateRenderInterface();
    // Execute a blocking test that creates a window and draws some geometry to test the render interface.
    RT64::RenderInterfaceTest(renderInterface.get());
}
