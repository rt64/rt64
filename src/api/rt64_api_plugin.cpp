//
// RT64
//

#include "rt64_api_common.h"

#include <SDL.h>
#include "imgui/backends/imgui_impl_sdl2.h"

#define PLUGIN_NAME                 "RT64 Video Plugin"
#define PLUGIN_VERSION              0x020509
#define VIDEO_PLUGIN_API_VERSION    0x020200
#define CONFIG_API_VERSION          0x020300

#ifndef NDEBUG
#   define LOG_PLUGIN_API_CALLS
#endif

/* definitions of pointers to Core video extension functions */
ptr_VidExt_InitWithRenderMode CoreVideo_InitWithRenderMode = NULL;
ptr_VidExt_Quit CoreVideo_Quit = NULL;
ptr_VidExt_SetCaption CoreVideo_SetCaption = NULL;
ptr_VidExt_ToggleFullScreen CoreVideo_ToggleFullScreen = NULL;
ptr_VidExt_ResizeWindow CoreVideo_ResizeWindow = NULL;
ptr_VidExt_VK_GetSurface CoreVideo_VK_GetSurface = NULL;
ptr_VidExt_VK_GetInstanceExtensions CoreVideo_VK_GetInstanceExtensions = NULL;
ptr_VidExt_SetVideoMode CoreVideo_SetVideoMode = NULL;

#ifdef _WIN32
#define DLSYM(a, b) GetProcAddress(a, b)
#else
#include <dlfcn.h>
#define DLSYM(a, b) dlsym(a, b)
#endif

DLLEXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context, void (*DebugCallback)(void *, int, const char *)) {
    CoreVideo_InitWithRenderMode = (ptr_VidExt_InitWithRenderMode)DLSYM(CoreLibHandle, "VidExt_InitWithRenderMode");
    CoreVideo_Quit = (ptr_VidExt_Quit)DLSYM(CoreLibHandle, "VidExt_Quit");
    CoreVideo_SetCaption = (ptr_VidExt_SetCaption)DLSYM(CoreLibHandle, "VidExt_SetCaption");
    CoreVideo_ToggleFullScreen = (ptr_VidExt_ToggleFullScreen)DLSYM(CoreLibHandle, "VidExt_ToggleFullScreen");
    CoreVideo_ResizeWindow = (ptr_VidExt_ResizeWindow)DLSYM(CoreLibHandle, "VidExt_ResizeWindow");
    CoreVideo_VK_GetSurface = (ptr_VidExt_VK_GetSurface)DLSYM(CoreLibHandle, "VidExt_VK_GetSurface");
    CoreVideo_VK_GetInstanceExtensions = (ptr_VidExt_VK_GetInstanceExtensions)DLSYM(CoreLibHandle, "VidExt_VK_GetInstanceExtensions");
    CoreVideo_SetVideoMode = (ptr_VidExt_SetVideoMode)DLSYM(CoreLibHandle, "VidExt_SetVideoMode");
    if (CoreVideo_InitWithRenderMode == NULL ||
        CoreVideo_Quit == NULL ||
        CoreVideo_SetCaption == NULL ||
        CoreVideo_ToggleFullScreen == NULL ||
        CoreVideo_ResizeWindow == NULL ||
        CoreVideo_VK_GetSurface == NULL ||
        CoreVideo_VK_GetInstanceExtensions == NULL ||
        CoreVideo_SetVideoMode == NULL) {
        return M64ERR_SYSTEM_FAIL;
    }
    return M64ERR_SUCCESS;
}

DLLEXPORT m64p_error CALL PluginShutdown(void) {   
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
        *Capabilities = 0;
                    
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
    if (RT64::API.app != nullptr) {
        RT64::API.app->end();
        RT64::API.app.reset();
    }
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

int window_width  = 640;
int window_height = 480;

DLLEXPORT void CALL ResizeVideoOutput(int width, int height) {
    window_width  = width;
    window_height = height;
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

// TODO: PR this to mupen64plus....
DLLEXPORT void CALL SDL_KeyDown(int keymod, int keysym)
{
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.mod = keymod;
    event.key.keysym.sym = keysym;
    if (RT64::API.app->presentQueue->inspector != nullptr) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

DLLEXPORT void CALL SDL_KeyUp(int keymod, int keysym)
{
    SDL_Event event;
    event.type = SDL_KEYUP;
    event.key.keysym.mod = SDL_SCANCODE_TO_KEYCODE(keymod);
    event.key.keysym.sym = SDL_SCANCODE_TO_KEYCODE(keysym);

    if (keysym == SDL_SCANCODE_F10)
    {
        if (RT64::API.app->userConfig.developerMode) {
            fprintf(stderr, "creating inspector...\n");
            const std::lock_guard<std::mutex> lock(RT64::API.app->presentQueue->inspectorMutex);
            if (RT64::API.app->presentQueue->inspector == nullptr) {
                RT64::API.app->presentQueue->inspector = std::make_unique<RT64::Inspector>(RT64::API.app->device.get(), RT64::API.app->swapChain.get(), RT64::API.app->createdGraphicsAPI);
                if (!RT64::API.app->userPaths.isEmpty()) {
                    RT64::API.app->presentQueue->inspector->setIniPath(RT64::API.app->userPaths.imguiPath);
                }

                RT64::API.app->freeCamClearQueued = true;
                ImGui_ImplSDL2_InitForVulkan(nullptr); // TODO: move this elsewhere...
                //appWindow->blockSdlKeyboard();
            }
            else if (RT64::API.app->presentQueue->inspector != nullptr) {
                RT64::API.app->presentQueue->inspector.reset(nullptr);
                //appWindow->unblockSdlKeyboard();
            }
        }
        else {
            fprintf(stdout, "Inspector is not available: developer mode is not enabled in the configuration.\n");
        }
        return;
    }

    if (RT64::API.app->presentQueue->inspector != nullptr) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

DLLEXPORT void CALL MouseMove(int x, int y)
{
    SDL_Event event;
    event.type = SDL_MOUSEMOTION;
    event.motion.which = 1;
    event.motion.x = x;
    event.motion.y = y;

    if (RT64::API.app->presentQueue->inspector != nullptr) {
        //printf("MouseMove x = %i, y = %i\n", x,y);
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

DLLEXPORT void CALL MouseButton(int left, int right)
{
    if (RT64::API.app->presentQueue->inspector != nullptr) {
        printf("MouseButton left = %i\n", left);
        SDL_Event event;

        event.type = left ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.button = SDL_BUTTON_LEFT;
        ImGui_ImplSDL2_ProcessEvent(&event);

        event.type = right ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.button = SDL_BUTTON_RIGHT;
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}


