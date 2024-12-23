#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes) {
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)window;
        NSRect contentFrame = [[nsWindow contentView] frame];
        attributes->x = contentFrame.origin.x;
        attributes->y = contentFrame.origin.y;
        attributes->width = contentFrame.size.width;
        attributes->height = contentFrame.size.height;
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)window;
            NSRect contentFrame = [[nsWindow contentView] frame];
            attributes->x = contentFrame.origin.x;
            attributes->y = contentFrame.origin.y;
            attributes->width = contentFrame.size.width;
            attributes->height = contentFrame.size.height;
        });
    }
}

int GetWindowRefreshRate(void* window) {
    __block int refreshRate = 0;
    
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)window;
        NSScreen *screen = [nsWindow screen];
        if (@available(macOS 12.0, *)) {
            refreshRate = (int)[screen maximumFramesPerSecond];
        }
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)window;
            NSScreen *screen = [nsWindow screen];
            if (@available(macOS 12.0, *)) {
                refreshRate = (int)[screen maximumFramesPerSecond];
            }
        });
    }
    
    return refreshRate;
}

void WindowToggleFullscreen(void* window) {
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)window;
        [nsWindow toggleFullScreen:NULL];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)window;
            [nsWindow toggleFullScreen:NULL];
        });
    }
}
