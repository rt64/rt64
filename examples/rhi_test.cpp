#include "plume_render_interface.h"

namespace plume {
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
    extern std::unique_ptr<RenderInterface> CreateMetalInterface();
}

std::unique_ptr<plume::RenderInterface> CreateRenderInterface() {
#if defined(_WIN32)
    const bool useVulkan = false; // Or derive this from configuration or runtime check.
    if (!useVulkan) {
        return plume::CreateD3D12Interface();
    }
    // Fallback to Vulkan if D3D12 is not chosen or available.
    return plume::CreateVulkanInterface();
#elif defined(__APPLE__)
    const bool useMVK = false; // Or derive this from configuration or runtime check.
    if (useMVK) {
        return plume::CreateVulkanInterface();
    }

    return plume::CreateMetalInterface();
#else
    return plume::CreateVulkanInterface();
#endif
}

int main(int argc, char** argv) {
    auto renderInterface = CreateRenderInterface();
    // Execute a blocking test that creates a window and draws some geometry to test the render interface.
    plume::RenderInterfaceTest(renderInterface.get());
}
