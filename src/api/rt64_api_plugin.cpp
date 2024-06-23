//
// RT64
//

#include "rt64_api_common.h"

#define PLUGIN_NAME                 "RT64 Video Plugin"
#define PLUGIN_VERSION              0x020509
#define VIDEO_PLUGIN_API_VERSION    0x020200
#define CONFIG_API_VERSION          0x020300

#ifndef NDEBUG
#   define LOG_PLUGIN_API_CALLS
#endif

DLLEXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context, void (*DebugCallback)(void *, int, const char *)) {
    return M64ERR_SUCCESS;
}

DLLEXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities) {
    /* set version info */
    if (PluginType != NULL)
        *PluginType = M64PLUGIN_GFX;

    if (PluginVersion != NULL)
        *PluginVersion = PLUGIN_VERSION;

    if (APIVersion != NULL)
        *APIVersion = VIDEO_PLUGIN_API_VERSION;
    
    if (PluginNamePtr != NULL)
        *PluginNamePtr = PLUGIN_NAME;

    if (Capabilities != NULL)
    {
        *Capabilities = 0;
    }
                    
    return M64ERR_SUCCESS;
}

#ifdef _WIN32
DLLEXPORT void CALL GetDllInfo(pj64::PLUGIN_INFO *PluginInfo) {
    PluginInfo->Version = 0x103;
    PluginInfo->Type = pj64::PLUGIN_TYPE_VIDEO;
    strncpy(PluginInfo->Name, PLUGIN_NAME, sizeof(PluginInfo->Name));
}
#endif

DLLEXPORT void CALL MoveScreen(int xpos, int ypos) {
    // Do nothing.
}

DLLEXPORT void CALL RomClosed(void) {
    // Do nothing.
}

DLLEXPORT int CALL RomOpen(void) {
    const bool isPJ64 = (RT64::API.apiType == RT64::APIType::Project64);
    if (isPJ64) {
        RT64::ApplicationWindow *appWindow = RT64::API.app->appWindow.get();
        appWindow->makeResizable();
    }

    return 1;
}

DLLEXPORT void CALL ViStatusChanged(void) { }

DLLEXPORT void CALL ViWidthChanged(void) { }

DLLEXPORT int CALL InitiateGFX(PluginGraphicsInfo graphicsInfo) {
    // Determine if this is a PJ64 plugin by checking if the HWND element is actually valid.
#ifdef _WIN32
    const bool isPJ64 = IsWindow(HWND(graphicsInfo.project64.hWnd));
#else
    const bool isPJ64 = false;
#endif
    if (isPJ64) {
        RT64::API.apiType = RT64::APIType::Project64;
    }
    else {
        RT64::API.apiType = RT64::APIType::Mupen64Plus;
    }

    // Store core information in application.
    RT64::Application::Core appCore;
    appCore.window = {};

    switch (RT64::API.apiType) {
    case RT64::APIType::Mupen64Plus:
        RT64::InitiateGFXCore<MupenGraphicsInfo>(appCore, graphicsInfo.mupen64plus);
        break;
#ifdef _WIN32
    case RT64::APIType::Project64:
        RT64::InitiateGFXCore<Project64GraphicsInfo>(appCore, graphicsInfo.project64);
        appCore.window = RT64::RenderWindow(graphicsInfo.project64.hWnd);
        break;
#endif
    default:
        break;
    }

    // Make new application with core information and set it up.
    RT64::API.app = std::make_unique<RT64::Application>(appCore, RT64::ApplicationConfiguration());
    return (RT64::API.app->setup(0) == RT64::Application::SetupResult::Success);
}

DLLEXPORT void CALL ResizeVideoOutput(int width, int height) {
    // Do nothing.
}

DLLEXPORT void CALL FBRead(uint32_t addr) {
    // Unused.
}

DLLEXPORT void CALL FBWrite(uint32_t addr, uint32_t size) {
    // Unused.
}

DLLEXPORT void CALL FBGetFrameBufferInfo(void *p) {
    // Unused.
}

DLLEXPORT void CALL ShowCFB(void) {
    // Unused.
}

DLLEXPORT void CALL ReadScreen2(void *dest, int *width, int *height, int bFront) {
    // Unused.
}
    
DLLEXPORT void CALL SetRenderingCallback(void (*callback)(int)) {
    // Unused.
}

DLLEXPORT void CALL CaptureScreen(const char *Directory) {
    // Unused.
}