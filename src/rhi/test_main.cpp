#include <cassert>
#include "rhi/rt64_render_interface.h"

namespace RT64 {
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

int main() {
    std::unique_ptr<RT64::RenderInterface> renderInterface;

    // Create a render interface with the preferred backend.
    const bool useVulkan = true;
    if (useVulkan) {
        renderInterface = RT64::CreateVulkanInterface();
    }
    else {
#ifdef _WIN64
        renderInterface = RT64::CreateD3D12Interface();
#else
        assert(false);
#endif
    }

    // Execute a blocking test that creates a window and draws some geometry to test the render interface.
    RT64::RenderInterfaceTest(renderInterface.get());
}
