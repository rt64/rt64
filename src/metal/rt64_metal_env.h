//
// RT64
//

#pragma once

#include <TargetConditionals.h>

/// macOS
#ifndef RT64_MACOS
#    define RT64_MACOS                (TARGET_OS_OSX || TARGET_OS_MACCATALYST)
#endif

/// iOS
#ifndef RT64_IOS
#    define RT64_IOS                    (TARGET_OS_IOS && !TARGET_OS_MACCATALYST)
#endif

/// Apple Silicon (iOS, tvOS, macOS)
#ifndef RT64_APPLE_SILICON
#    define RT64_APPLE_SILICON        TARGET_CPU_ARM64
#endif

/// Apple Silicon on macOS
#ifndef RT64_MACOS_APPLE_SILICON
#    define RT64_MACOS_APPLE_SILICON    (RT64_MACOS && RT64_APPLE_SILICON)
#endif
