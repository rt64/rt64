//
// RT64
//

#include "rt64_elapsed_timer.h"

namespace RT64 {
    // ElapsedTimer

    ElapsedTimer::ElapsedTimer() {
        reset();
    }

    void ElapsedTimer::reset() {
        startTime = Timer::current();
    }

    int64_t ElapsedTimer::elapsedMicroseconds() const {
        return Timer::deltaMicroseconds(startTime, Timer::current());
    }

    double ElapsedTimer::elapsedMilliseconds() const {
        return static_cast<double>(elapsedMicroseconds()) / 1000.0;
    }

    double ElapsedTimer::elapsedSeconds() const {
        return static_cast<double>(elapsedMicroseconds()) / 1000000.0;
    }
};