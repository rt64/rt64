#include "rhi/rt64_render_interface.h"

namespace RT64 {
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

int main(int argc, char** argv) {
    std::unique_ptr<RT64::RenderInterface> renderInterface = RT64::CreateVulkanInterface();

#ifdef _WIN32
    // Windows only: Can also use D3D12.
    const bool useVulkan = true;
    if (!useVulkan) {
        renderInterface = RT64::CreateD3D12Interface();
    }
#endif

    // Execute a blocking test that creates a window and draws some geometry to test the render interface.
    RT64::RenderInterfaceTest(renderInterface.get());
}
