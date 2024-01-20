//
// RT64
//

#include "rt64_profiling_timer.h"

#include <numeric>

namespace RT64 {
    // ProfilingTimer

    ProfilingTimer::ProfilingTimer(size_t historySize) {
        assert(historySize > 0);
        history.resize(historySize, 0.0);
        historyIndex = 0;
        accumulation = 0.0;
        startedTimestamp = {};
    }

    void ProfilingTimer::clear() {
        size_t previousSize = history.size();
        history.clear();
        history.resize(previousSize, 0);
        historyIndex = 0;
        accumulation = 0.0;
    }

    void ProfilingTimer::reset() {
        accumulation = 0.0;
    }

    void ProfilingTimer::start() {
        assert(startedTimestamp == Timestamp{});
        startedTimestamp = Timer::current();
    }

    void ProfilingTimer::end() {
        assert(startedTimestamp > Timestamp{});
        accumulation += Timer::deltaMicroseconds(startedTimestamp, Timer::current()) / 1000.0;
        startedTimestamp = {};
    }

    void ProfilingTimer::log() {
        history[historyIndex] = accumulation;
        historyIndex = (historyIndex + 1) % history.size();
    }

    void ProfilingTimer::logAndRestart() {
        if (startedTimestamp == Timestamp{}) {
            reset();
            start();
        }

        end();
        log();
        reset();
        start();
    }

    uint32_t ProfilingTimer::index() const {
        return historyIndex;
    }

    size_t ProfilingTimer::size() const {
        return history.size();
    }

    const double *ProfilingTimer::data() const {
        return history.data();
    }

    double ProfilingTimer::average() const {
        return std::accumulate(history.begin(), history.end(), 0.0) / history.size();
    }
};