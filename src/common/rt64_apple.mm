
#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSRect frame = [nsWindow frame];
    attributes->x = frame.origin.x;
    attributes->y = frame.origin.y;
    attributes->width = frame.size.width;
    attributes->height = frame.size.height;
}

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

int GetWindowRefreshRate(void* window) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSScreen *screen = [nsWindow screen];
    return (int)[screen maximumFramesPerSecond];
}

// takes a c++ lambda and dispatches it on the main thread
void DispatchOnMainThread(std::function<void()> func) {
    dispatch_async(dispatch_get_main_queue(), ^{
        func();
    });
}
