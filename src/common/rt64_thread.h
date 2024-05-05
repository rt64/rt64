//
// RT64
//

#pragma once

#include <cstdint>
#include <string>

namespace RT64 {
    struct Thread {
        enum class Priority {
            Idle,
            Lowest,
            Low,
            Normal,
            High,
            Highest
        };

        static void setCurrentThreadName(const std::string &str);
        static void setCurrentThreadPriority(Priority priority);
        static void sleepMilliseconds(uint32_t millis);
    };
};