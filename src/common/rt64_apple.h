//
// RT64
//

#pragma once

#include <atomic>

struct CocoaWindowAttributes {
    int x, y;
    int width, height;
};

const char* GetHomeDirectory();

class CocoaWindow {
private:
    void* windowHandle;
    std::atomic<int> cachedX;
    std::atomic<int> cachedY;
    std::atomic<int> cachedWidth;
    std::atomic<int> cachedHeight;
    std::atomic<int> cachedRefreshRate;

    // Helper methods to update cached values
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
