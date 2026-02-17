#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

std::string GetHomeDirectory() {
    return std::string([NSHomeDirectory() UTF8String]);
}
