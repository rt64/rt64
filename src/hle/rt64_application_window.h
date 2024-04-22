//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"
#ifdef _WIN32
#include <Windows.h>
#endif

#define RT64_SDL_WINDOW

#ifdef RT64_SDL_WINDOW
#   include "SDL_events.h"
#   include "SDL_system.h"
#   include "SDL_syswm.h"
#   include "SDL_video.h"
#endif

namespace RT64 {
    struct ApplicationWindow {
        static ApplicationWindow *HookedApplicationWindow;

        struct Listener {
            // Return true if the listener should accept and handle the message.
            // Return false otherwise for the default message handler to take over.
#       ifdef _WIN32
            virtual bool windowMessageFilter(unsigned int message, WPARAM wParam, LPARAM lParam) = 0;
            virtual bool usesWindowMessageFilter() = 0;
#       endif
        };

        RenderWindow windowHandle;
#   ifdef _WIN32
        HHOOK windowHook;
        HMENU windowMenu;
        RECT lastWindowRect;
#   endif
        Listener *listener;
        uint32_t refreshRate = 0;
        bool fullScreen;
        bool lastMaximizedState;
        bool usingSdl;
        int32_t windowLeft = INT32_MAX;
        int32_t windowTop = INT32_MAX;

        ApplicationWindow();
        ~ApplicationWindow();
        void setup(RenderWindow window, Listener *listener, uint32_t threadId);
        void setup(const char *windowTitle, Listener *listener);
        void setFullScreen(bool newFullScreen);
        void makeResizable();
        void detectRefreshRate();
        uint32_t getRefreshRate() const;
        bool detectWindowMoved();

#   ifdef _WIN32
        void windowMessage(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT windowHookCallback(int nCode, WPARAM wParam, LPARAM lParam);
#   endif

#   ifdef RT64_SDL_WINDOW
        SDL_EventFilter sdlEventFilter;
        void *sdlEventFilterUserdata;
        bool sdlEventFilterStored;

        void blockSdlKeyboard();
        void unblockSdlKeyboard();
        static int sdlEventKeyboardFilter(void *userdata, SDL_Event *event);
#   endif
    };
};