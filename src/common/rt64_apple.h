//
// RT64
//

#pragma once

#include <functional>

struct CocoaWindowAttributes {
    int x, y;
    int width, height;
};

const char* GetHomeDirectory();

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes);
int GetWindowRefreshRate(void* window);
void WindowToggleFullscreen(void* window);

void DispatchOnMainThread(std::function<void()> func);
