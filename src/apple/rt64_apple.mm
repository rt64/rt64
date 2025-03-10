#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

// MARK: - CocoaWindow

CocoaWindow::CocoaWindow(void* window)
    : windowHandle(window), cachedRefreshRate(0) {
    // Initialize with zeros
    cachedAttributes = {0, 0, 0, 0};

    // Synchronously get initial values - we're likely already on the main thread during init
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];
        cachedAttributes.x = contentFrame.origin.x;
        cachedAttributes.y = contentFrame.origin.y;
        cachedAttributes.width = contentFrame.size.width;
        cachedAttributes.height = contentFrame.size.height;

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
    if (forceSync) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSRect contentFrame = [[nsWindow contentView] frame];

            std::lock_guard<std::mutex> lock(attributesMutex);
            cachedAttributes.x = contentFrame.origin.x;
            cachedAttributes.y = contentFrame.origin.y;
            cachedAttributes.width = contentFrame.size.width;
            cachedAttributes.height = contentFrame.size.height;
        });
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSRect contentFrame = [[nsWindow contentView] frame];

            std::lock_guard<std::mutex> lock(attributesMutex);
            cachedAttributes.x = contentFrame.origin.x;
            cachedAttributes.y = contentFrame.origin.y;
            cachedAttributes.width = contentFrame.size.width;
            cachedAttributes.height = contentFrame.size.height;
        });
    }
}

void CocoaWindow::updateRefreshRateInternal(bool forceSync) {
    if (forceSync) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSScreen *screen = [nsWindow screen];
            if (@available(macOS 12.0, *)) {
                cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
            }
        });
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
            NSScreen *screen = [nsWindow screen];
            if (@available(macOS 12.0, *)) {
                cachedRefreshRate.store((int)[screen maximumFramesPerSecond]);
            }
        });
    }
}

void CocoaWindow::getWindowAttributes(CocoaWindowAttributes* attributes) const {
    if ([NSThread isMainThread]) {
        // We're on the main thread, get fresh data directly and update cache
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];

        {
            std::lock_guard<std::mutex> lock(attributesMutex);
            const_cast<CocoaWindow*>(this)->cachedAttributes.x = contentFrame.origin.x;
            const_cast<CocoaWindow*>(this)->cachedAttributes.y = contentFrame.origin.y;
            const_cast<CocoaWindow*>(this)->cachedAttributes.width = contentFrame.size.width;
            const_cast<CocoaWindow*>(this)->cachedAttributes.height = contentFrame.size.height;

            // Return the fresh data
            *attributes = cachedAttributes;
        }
    } else {
        // Not on main thread - return cached values and schedule async update
        {
            std::lock_guard<std::mutex> lock(attributesMutex);
            *attributes = cachedAttributes;
        }

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
