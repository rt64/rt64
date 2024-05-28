//
// RT64
//

#pragma once

#include <functional>

struct CocoaWindowAttributes {
    int x, y;
    int width, height;
};

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes);
const char* GetHomeDirectory();
int GetWindowRefreshRate(void* window);

void DispatchOnMainThread(std::function<void()> func);
