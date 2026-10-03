#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char *GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}

namespace apple {
    void dispatchOnMainThread(std::function<void()> func) {
        dispatch_async(dispatch_get_main_queue(), ^{
            func();
        });
    }
}