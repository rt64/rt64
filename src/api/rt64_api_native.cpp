//
// RT64
//

#include <cassert>
#include <stdint.h>

#include "m64p_common.h"
#include "m64p_plugin.h"
#include "m64p_types.h"

#include "rt64_api_common.h"

#if defined(_WIN32)
DLLEXPORT RT64::Application *CALL InitiateGFXWindows(PluginGraphicsInfo graphicsInfo, HWND hwnd, DWORD threadId, uint8_t developerMode) {
    RT64::API.apiType = RT64::APIType::Native;

    // Store core information in application.
    RT64::Application::Core appCore;
    appCore.window = hwnd;

    RT64::InitiateGFXCore<MupenGraphicsInfo>(appCore, graphicsInfo.mupen64plus);

    // Make new application with core information and set it up.
    RT64::ApplicationConfiguration appConfig;
    appConfig.useConfigurationFile = false;
    RT64::API.app = std::make_unique<RT64::Application>(appCore, appConfig);
    RT64::API.app->userConfig.developerMode = developerMode;
    if (!RT64::API.app->setup(threadId)) {
        return nullptr;
    }

    return RT64::API.app.get();
}
#elif defined(__ANDROID__)
static_assert(false && "Unimplemented");
#elif defined(__linux__)
DLLEXPORT RT64::Application *CALL InitiateGFXLinux(PluginGraphicsInfo graphicsInfo, Window window, Display *display, uint8_t developerMode) {
    RT64::API.apiType = RT64::APIType::Native;

    // Store core information in application.
    RT64::Application::Core appCore;
    appCore.window = {
        .display = display,
        .window = window
    };

    RT64::InitiateGFXCore<MupenGraphicsInfo>(appCore, graphicsInfo.mupen64plus);

    // Make new application with core information and set it up.
    RT64::ApplicationConfiguration appConfig;
    appConfig.useConfigurationFile = false;
    RT64::API.app = std::make_unique<RT64::Application>(appCore, appConfig);
    RT64::API.app->userConfig.developerMode = developerMode;
    if (!RT64::API.app->setup(0)) {
        return nullptr;
    }
    return RT64::API.app.get();
}
#else
static_assert(false && "Unimplemented");
#endif

