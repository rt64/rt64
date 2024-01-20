//
// RT64
//

#include "rt64_timer.h"

namespace RT64 {
    // Timer

    void Timer::initialize() {
    }

    Timestamp Timer::current() {
        return std::chrono::high_resolution_clock::now();
    }

    int64_t Timer::deltaMicroseconds(const Timestamp t1, const Timestamp t2) {
        return std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    }
};
