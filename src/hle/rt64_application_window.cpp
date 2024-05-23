//
// RT64
//

#include "rt64_application_window.h"

#include <cassert>
#include <stdio.h>

#if defined(_WIN32)
#   include <Windows.h>
#   include <ShellScalingAPI.h>
#elif defined(__linux__)
#   define Status int
#   include <X11/extensions/Xrandr.h>
#endif

#include "common/rt64_common.h"

namespace RT64 {
    // ApplicationWindow

    ApplicationWindow *ApplicationWindow::HookedApplicationWindow = nullptr;
    
    ApplicationWindow::ApplicationWindow() {
        windowHandle = {};
        sdlEventFilterUserdata = nullptr;
        sdlEventFilterStored = false;
        fullScreen = false;
        lastMaximizedState = false;
#   ifdef _WIN32
        windowHook = nullptr;
        windowMenu = nullptr;
#   endif
        usingSdl = false;
    }

    ApplicationWindow::~ApplicationWindow() {
        if (HookedApplicationWindow == this) {
            HookedApplicationWindow = nullptr;
        }

#   ifdef _WIN32
        if (windowHook != nullptr) {
            UnhookWindowsHookEx(windowHook);
        }
#   endif
    }
    
    void ApplicationWindow::setup(RenderWindow window, Listener *listener, uint32_t threadId) {
        assert(listener != nullptr);

        this->listener = listener;
        windowHandle = window;

#   ifdef _WIN32
        if (listener->usesWindowMessageFilter()) {
            assert(HookedApplicationWindow == nullptr);
            assert(threadId != 0);
            windowHook = SetWindowsHookEx(WH_GETMESSAGE, &windowHookCallback, NULL, threadId);
            HookedApplicationWindow = this;
        }
#   endif
    }

    void ApplicationWindow::setup(const char *windowTitle, Listener *listener) {
        assert(windowTitle != nullptr);

        // Find the right window dimension and placement.
        const int Width = 1280;
        const int Height = 720;
        struct {
            uint32_t left, top, width, height;
        } bounds {};

#   if defined(_WIN32)
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

        RECT rect;
        UINT dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        rect.left = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
        rect.top = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
        rect.right = rect.left + Width;
        rect.bottom = rect.top + Height;
        AdjustWindowRectEx(&rect, dwStyle, 0, 0);
        bounds.left = rect.left;
        bounds.top = rect.top;
        bounds.width = rect.right - rect.left;
        bounds.height = rect.bottom - rect.top;
#   elif defined(__ANDROID__)
        static_assert(false && "Android unimplemented");
#   elif defined(__linux__) || defined(__APPLE__)
        if (SDL_VideoInit(nullptr) != 0) {
            printf("Failed to init SDL2 video: %s\n", SDL_GetError());
            assert(false && "Failed to init SDL2 video");
            return;
        }
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
            printf("Failed to get SDL2 desktop display mode: %s\n", SDL_GetError());
            assert(false && "Failed to get SDL2 desktop display mode");
            return;
        }
        bounds.left = (dm.w - Width) / 2;
        bounds.top = (dm.h - Height) / 2;
        bounds.width = Width;
        bounds.height = Height;
#   else
        static_assert(false && "Unimplemented");
#   endif
        
        // Create window.
#ifdef RT64_SDL_WINDOW
        SDL_Window *sdlWindow = SDL_CreateWindow(windowTitle, bounds.left, bounds.top, bounds.width, bounds.height, SDL_WINDOW_RESIZABLE);
        SDL_SysWMinfo wmInfo;
        assert(sdlWindow && "Failed to open window with SDL");
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(sdlWindow, &wmInfo);
#   if defined(_WIN32)
        windowHandle = wmInfo.info.win.window;
#   elif defined(__ANDROID__)
        static_assert(false && "Android unimplemented");
#   elif defined(__linux__)
        windowHandle.display = wmInfo.info.x11.display;
        windowHandle.window = wmInfo.info.x11.window;
#   elif defined(__APPLE__)
        windowHandle.window = wmInfo.info.cocoa.window;
#   else
        static_assert(false && "Unimplemented");
#   endif
        usingSdl = true;
#else
        static_assert(false && "Unimplemented");
#endif

#   ifdef _WIN32
        setup(windowHandle, listener, GetCurrentThreadId());
#   elif defined(__APPLE__)
        uint64_t tid;
        pthread_threadid_np(nullptr, &tid);
        setup(windowHandle, listener, tid);
#   else
        setup(windowHandle, listener, pthread_self());
#   endif
    }

    void ApplicationWindow::setFullScreen(bool newFullScreen) {
        if (newFullScreen == fullScreen) {
            return;
        }

#   ifdef _WIN32
        if (newFullScreen) {
            // Save if window is maximized or not
            WINDOWPLACEMENT windowPlacement;
            windowPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(windowHandle, &windowPlacement);
            lastMaximizedState = windowPlacement.showCmd == SW_SHOWMAXIMIZED;

            // Save window position and size if the window is not maximized
            GetWindowRect(windowHandle, &lastWindowRect);

            // Get in which monitor the window is
            HMONITOR hmonitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);

            // Get info from that monitor
            MONITORINFOEX monitorInfo;
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(hmonitor, &monitorInfo);
            RECT r = monitorInfo.rcMonitor;

            // Set borderless full screen to that monitor
            LONG_PTR lStyle = GetWindowLongPtr(windowHandle, GWL_STYLE);
            lStyle = (lStyle | WS_VISIBLE | WS_POPUP) & ~WS_OVERLAPPEDWINDOW;
            SetWindowLongPtr(windowHandle, GWL_STYLE, lStyle);
            SetWindowPos(windowHandle, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
            if (windowMenu != nullptr) {
                SetMenu(windowHandle, nullptr);
            }
        }
        else {
            // Set in window mode with the last saved position and size.
            LONG_PTR lStyle = GetWindowLongPtr(windowHandle, GWL_STYLE);
            lStyle = (lStyle | WS_VISIBLE | WS_OVERLAPPEDWINDOW) & ~WS_POPUP;
            SetWindowLongPtr(windowHandle, GWL_STYLE, lStyle);

            if (lastMaximizedState) {
                SetWindowPos(windowHandle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
                ShowWindow(windowHandle, SW_MAXIMIZE);
            }
            else {
                const RECT &r = lastWindowRect;
                SetWindowPos(windowHandle, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
                ShowWindow(windowHandle, SW_RESTORE);
            }

            if (windowMenu != nullptr) {
                SetMenu(windowHandle, windowMenu);
            }
        }

        fullScreen = newFullScreen;
#   endif
    }
    
    void ApplicationWindow::makeResizable() {
#   ifdef _WIN32
        LONG_PTR lStyle = GetWindowLongPtr(windowHandle, GWL_STYLE);
        windowMenu = GetMenu(windowHandle);
        lStyle |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        SetWindowLongPtr(windowHandle, GWL_STYLE, lStyle);
#   endif
    }

    void ApplicationWindow::detectRefreshRate() {
#   if defined(_WIN32)
        HMONITOR monitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX info = {};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfo(monitor, &info)) {
            fprintf(stderr, "GetMonitorInfo failed.\n");
            return;
        }

        DEVMODE displayMode = {};
        displayMode.dmSize = sizeof(DEVMODE);
        if (!EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &displayMode)) {
            fprintf(stderr, "EnumDisplaySettings failed.\n");
            return;
        }

        refreshRate = displayMode.dmDisplayFrequency;

        // FIXME: This function truncates refresh rates that'd otherwise round to the correct rate in most cases.
        // This hack will fix most common cases where refresh rates divisble by 10 are truncated to the wrong value.
        // This can be removed when a more accurate way to query the refresh rate of the monitor is found.
        if ((refreshRate % 10) == 9) {
            refreshRate++;
        }
#   elif defined(__linux__)
        // Sourced from: https://stackoverflow.com/a/66865623
        XRRScreenResources *screenResources = XRRGetScreenResources(windowHandle.display, windowHandle.window);
        if (screenResources == nullptr) {
            fprintf(stderr, "XRRGetScreenResources failed.\n");
            return;
        }

        RRMode activeModeID = 0;
        for (int i = 0; i < screenResources->ncrtc; ++i) {
            XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(windowHandle.display, screenResources, screenResources->crtcs[i]);
            if ((crtcInfo != nullptr) && (crtcInfo->mode != 0L)) {
                activeModeID = crtcInfo->mode; 
            }
        }

        if (activeModeID == 0L) {
            fprintf(stderr, "Unable to find active mode through XRRGetScreenResources and XRRGetCrtcInfo.\n");
            return;
        }

        for (int i = 0; i < screenResources->nmode; ++i) {
            XRRModeInfo modeInfo = screenResources->modes[i];
            if (modeInfo.id == activeModeID) {
                refreshRate = std::lround(modeInfo.dotClock / double(modeInfo.hTotal * modeInfo.vTotal));
                break;
            }
        }
#   endif
    }

    uint32_t ApplicationWindow::getRefreshRate() const {
        return refreshRate;
    }

    bool ApplicationWindow::detectWindowMoved() {
        int32_t newWindowLeft = INT32_MAX;
        int32_t newWindowTop = INT32_MAX;

#   if defined(_WIN64)
        RECT rect;
        GetWindowRect(windowHandle, &rect);
        newWindowLeft = rect.left;
        newWindowTop = rect.top;
#   elif defined(__linux__)
        XWindowAttributes attributes;
        XGetWindowAttributes(windowHandle.display, windowHandle.window, &attributes);
        newWindowLeft = attributes.x;
        newWindowTop = attributes.y;
#   endif

        if ((windowLeft != newWindowLeft) || (windowTop != newWindowTop)) {
            windowLeft = newWindowLeft;
            windowTop = newWindowTop;
            return true;
        }
        else {
            return false;
        }
    }

#   ifdef _WIN32
    void ApplicationWindow::windowMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        if (listener->windowMessageFilter(message, wParam, lParam)) {
            return;
        }

        switch (message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        };
    }

    LRESULT CALLBACK ApplicationWindow::windowHookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
        if ((nCode >= 0) && (wParam == PM_REMOVE)) {
            const MSG &msg = *reinterpret_cast<MSG *>(lParam);
            HookedApplicationWindow->windowMessage(msg.message, msg.wParam, msg.lParam);
        }

        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
#   endif

#ifdef RT64_SDL_WINDOW
    void ApplicationWindow::blockSdlKeyboard() {
        if (!usingSdl) {
            return;
        }

        sdlEventFilterStored = SDL_GetEventFilter(&sdlEventFilter, &sdlEventFilterUserdata);
        if (sdlEventFilterStored) {
            SDL_SetEventFilter(&ApplicationWindow::sdlEventKeyboardFilter, this);
        }
    }

    void ApplicationWindow::unblockSdlKeyboard() {
        if (!usingSdl) {
            return;
        }

        if (sdlEventFilterStored) {
            SDL_SetEventFilter(sdlEventFilter, sdlEventFilterUserdata);
            sdlEventFilterStored = false;
        }
    }

    int ApplicationWindow::sdlEventKeyboardFilter(void *userdata, SDL_Event *event) {
        ApplicationWindow *appWindow = reinterpret_cast<ApplicationWindow *>(userdata);

        switch (event->type) {
            // Ignore all keyboard events.
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return 0;
            // Pass through to the original event filter.
        default:
            return appWindow->sdlEventFilter(userdata, event);
        }
    }


#else
    static_assert(false && "Unimplemented");
#endif
};