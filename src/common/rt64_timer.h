//
// RT64
//

#pragma once

#include <cassert>
#include <chrono>
#include <stdint.h>

namespace RT64 {
    typedef std::chrono::high_resolution_clock::time_point Timestamp;

    struct Timer {
        static void initialize();
        static Timestamp current();
        static int64_t deltaMicroseconds(const Timestamp t1, const Timestamp t2);
    };
};