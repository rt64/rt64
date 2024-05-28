//
// RT64
//

#pragma once

struct CocoaWindowAttributes {
    int x, y;
    int width, height;
};

void CocoaGetWindowAttributes(void* window, CocoaWindowAttributes *attributes);

const char* GetMainBundlePath();

int GetRefreshRate(void* window);