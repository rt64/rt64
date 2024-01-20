//
// RT64
//

#pragma once

#include "rt64_timer.h"

namespace RT64 {
    struct ElapsedTimer {
        Timestamp startTime;

        ElapsedTimer();
        void reset();
        int64_t elapsedMicroseconds() const;
        double elapsedMilliseconds() const;
        double elapsedSeconds() const;
    };
};