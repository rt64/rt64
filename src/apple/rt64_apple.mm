#include "rt64_apple.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

const char* GetHomeDirectory() {
    return strdup([NSHomeDirectory() UTF8String]);
}
