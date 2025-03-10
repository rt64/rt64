#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

// MARK: - CocoaWindow

CocoaWindow::CocoaWindow(void* window) 
    : windowHandle(window),
      cachedX(0),
      cachedY(0),
      cachedWidth(0),
      cachedHeight(0),
      cachedRefreshRate(0) {

    // Synchronously get initial values - we're likely already on the main thread during init
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];
        cachedX.store(contentFrame.origin.x);
        cachedY.store(contentFrame.origin.y);
        cachedWidth.store(contentFrame.size.width);
        cachedHeight.store(contentFrame.size.height);
        
        NSScreen *screen = [nsWindow screen];
        if (@available(macOS 12.0, *)) {
            cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
        }
    } else {
        // Force sync update if not on main thread
        updateWindowAttributesInternal(true);
        updateRefreshRateInternal(true);
    }
}

CocoaWindow::~CocoaWindow() {
    // Nothing to clean up
}

void CocoaWindow::updateWindowAttributesInternal(bool forceSync) {
    auto updateBlock = ^{
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];
        
        cachedX.store(contentFrame.origin.x);
        cachedY.store(contentFrame.origin.y);
        cachedWidth.store(contentFrame.size.width);
        cachedHeight.store(contentFrame.size.height);
    };
    
    if (forceSync) {
        dispatch_sync(dispatch_get_main_queue(), updateBlock);
    } else {
        dispatch_async(dispatch_get_main_queue(), updateBlock);
    }
}

void CocoaWindow::updateRefreshRateInternal(bool forceSync) {
    auto updateBlock = ^{
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSScreen *screen = [nsWindow screen];
        if (@available(macOS 12.0, *)) {
            cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
        }
    };
    
    if (forceSync) {
        dispatch_sync(dispatch_get_main_queue(), updateBlock);
    } else {
        dispatch_async(dispatch_get_main_queue(), updateBlock);
    }
}

void CocoaWindow::getWindowAttributes(CocoaWindowAttributes* attributes) const {
    if ([NSThread isMainThread]) {
        // We're on the main thread, get fresh data directly and update cache
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];
        
        {
            const_cast<CocoaWindow*>(this)->cachedX.store(contentFrame.origin.x);
            const_cast<CocoaWindow*>(this)->cachedY.store(contentFrame.origin.y);
            const_cast<CocoaWindow*>(this)->cachedWidth.store(contentFrame.size.width);
            const_cast<CocoaWindow*>(this)->cachedHeight.store(contentFrame.size.height);
            
            // Return the fresh data
            attributes->x = contentFrame.origin.x;
            attributes->y = contentFrame.origin.y;
            attributes->width = contentFrame.size.width;
            attributes->height = contentFrame.size.height;
        }
    } else {
        // Not on main thread - return cached values and schedule async update
        attributes->x = cachedX.load();
        attributes->y = cachedY.load();
        attributes->width = cachedWidth.load();
        attributes->height = cachedHeight.load();
        
        // Schedule async update for next time
        const_cast<CocoaWindow*>(this)->updateWindowAttributesInternal(false);
    }
}

int CocoaWindow::getRefreshRate() const {
    if ([NSThread isMainThread]) {
        // We're on the main thread, get fresh data directly and update cache
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSScreen *screen = [nsWindow screen];
        
        if (@available(macOS 12.0, *)) {
            int freshRate = (int)[screen maximumFramesPerSecond];
            const_cast<CocoaWindow*>(this)->cachedRefreshRate.store(freshRate);
            return freshRate;
        }
        
        // Return cached if not available
        return cachedRefreshRate.load();
    } else {
        // Not on main thread - return cached value and schedule async update
        int rate = cachedRefreshRate.load();
        
        // Schedule async update for next time
        const_cast<CocoaWindow*>(this)->updateRefreshRateInternal(false);
        
        return rate;
    }
}

void CocoaWindow::toggleFullscreen() {
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        [nsWindow toggleFullScreen:NULL];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            [nsWindow toggleFullScreen:NULL];
        });
    }
}
