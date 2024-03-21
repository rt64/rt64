//
// RT64
//

#pragma once

#include <vector>

#include "rt64_elapsed_timer.h"

namespace RT64 {
    struct ProfilingTimer {
        std::vector<double> history;
        uint32_t historyIndex;
        double accumulation;
        Timestamp startedTimestamp;

        ProfilingTimer();
        ProfilingTimer(size_t historyCount);
        void setCount(size_t historyCount);
        void clear();
        void reset();
        void start();
        void end();
        void log();

        // Convenience function for logging the time between each call to it.
        void logAndRestart();
        uint32_t index() const;
        size_t size() const;
        const double *data() const;
        double average() const;
    };
};