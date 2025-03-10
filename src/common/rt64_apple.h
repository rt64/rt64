//
// RT64
//

#pragma once

#include <atomic>
#include <mutex>

struct CocoaWindowAttributes {
    int x, y;
    int width, height;
};

const char* GetHomeDirectory();

class CocoaWindow {
private:
    void* windowHandle;
    CocoaWindowAttributes cachedAttributes;
    std::atomic<int> cachedRefreshRate;
    mutable std::mutex attributesMutex;

    void updateWindowAttributesInternal(bool forceSync = false);
    void updateRefreshRateInternal(bool forceSync = false);
public:
    CocoaWindow(void* window);
    ~CocoaWindow();

    // Get cached window attributes, may trigger async update
    void getWindowAttributes(CocoaWindowAttributes* attributes) const;

    // Get cached refresh rate, may trigger async update
    int getRefreshRate() const;

    // Toggle fullscreen
    void toggleFullscreen();
};
