
#include "rt64_vulkan_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

void CocoaGetWindowAttributes(void* window, CocoaWindowAttributes *attributes) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSRect frame = [nsWindow frame];
    attributes->x = frame.origin.x;
    attributes->y = frame.origin.y;
    attributes->width = frame.size.width;
    attributes->height = frame.size.height;
}

const char* GetMainBundlePath() {
    return strdup([[NSBundle mainBundle].bundlePath UTF8String]);
}