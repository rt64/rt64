
#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

void GetWindowAttributes(void* window, CocoaWindowAttributes *attributes) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSRect contentFrame = [[nsWindow contentView] frame];
    attributes->x = contentFrame.origin.x;
    attributes->y = contentFrame.origin.y;
    attributes->width = contentFrame.size.width;
    attributes->height = contentFrame.size.height;
}

int GetWindowRefreshRate(void* window) {
    NSWindow *nsWindow = (NSWindow *)window;
    NSScreen *screen = [nsWindow screen];

    if (@available(macOS 12.0, *)) {
        return (int)[screen maximumFramesPerSecond];
    }
    
    // TODO: Implement this.
    return 0;
}

void WindowToggleFullscreen(void* window) {
    // UI operations have to happen on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindow *nsWindow = (NSWindow *)window;
        [nsWindow toggleFullScreen:NULL];
    });
}
