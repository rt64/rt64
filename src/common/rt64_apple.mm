
#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSRect contentFrame = [[nsWindow contentView] frame];
    attributes->x = contentFrame.origin.x;
    attributes->y = contentFrame.origin.y;
    attributes->width = contentFrame.size.width;
    attributes->height = contentFrame.size.height;
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
