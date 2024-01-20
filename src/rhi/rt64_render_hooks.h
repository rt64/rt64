//
// RT64
//

#pragma once

#include "rt64_render_interface.h"

#ifdef _WIN32
#   define RT64_EXPORT __declspec(dllexport)  
#else
#   define RT64_EXPORT __attribute__((visibility("default")))
#endif

namespace RT64 {
    using RenderHookInit = void(RenderInterface *rhi, RenderDevice *device);
    using RenderHookDraw = void(RenderCommandList *list, RenderFramebuffer *swapChainFramebuffer);
    using RenderHookDeinit = void();

    RenderHookInit *GetRenderHookInit();
    RenderHookDraw *GetRenderHookDraw();
    RenderHookDeinit *GetRenderHookDeinit();

    RT64_EXPORT void SetRenderHooks(RenderHookInit *init, RenderHookDraw *draw, RenderHookDeinit *deinit);
};