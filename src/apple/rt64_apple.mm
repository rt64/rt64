#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

// MARK: - CocoaWindow

CocoaWindow::CocoaWindow(void* window)
    : windowHandle(window), cachedRefreshRate(0) {
    cachedAttributes = {0, 0, 0, 0};

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
        updateWindowAttributesInternal(true);
        updateRefreshRateInternal(true);
    }
}

CocoaWindow::~CocoaWindow() {}

void CocoaWindow::updateWindowAttributesInternal(bool forceSync) {
    auto updateBlock = ^{
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];
        
        std::lock_guard<std::mutex> lock(attributesMutex);
        cachedAttributes.x = contentFrame.origin.x;
        cachedAttributes.y = contentFrame.origin.y;
        cachedAttributes.width = contentFrame.size.width;
        cachedAttributes.height = contentFrame.size.height;
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
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSRect contentFrame = [[nsWindow contentView] frame];

        {
            std::lock_guard<std::mutex> lock(attributesMutex);
            const_cast<CocoaWindow*>(this)->cachedAttributes.x = contentFrame.origin.x;
            const_cast<CocoaWindow*>(this)->cachedAttributes.y = contentFrame.origin.y;
            const_cast<CocoaWindow*>(this)->cachedAttributes.width = contentFrame.size.width;
            const_cast<CocoaWindow*>(this)->cachedAttributes.height = contentFrame.size.height;

            *attributes = cachedAttributes;
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(attributesMutex);
            *attributes = cachedAttributes;
        }

        const_cast<CocoaWindow*>(this)->updateWindowAttributesInternal(false);
    }
}

int CocoaWindow::getRefreshRate() const {
    if ([NSThread isMainThread]) {
        NSWindow *nsWindow = (__bridge NSWindow *)windowHandle;
        NSScreen *screen = [nsWindow screen];

        if (@available(macOS 12.0, *)) {
            int freshRate = (int)[screen maximumFramesPerSecond];
            const_cast<CocoaWindow*>(this)->cachedRefreshRate.store(freshRate);
            return freshRate;
        }

        return cachedRefreshRate.load();
    } else {
        int rate = cachedRefreshRate.load();

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
