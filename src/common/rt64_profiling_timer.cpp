//
// RT64
//

#include "rt64_profiling_timer.h"

#include <numeric>

namespace RT64 {
    // ProfilingTimer

    ProfilingTimer::ProfilingTimer() {
        historyIndex = 0;
        accumulation = 0.0;
        startedTimestamp = {};
    }

    ProfilingTimer::ProfilingTimer(size_t historySize) : ProfilingTimer() {
        setCount(historySize);
    }

    void ProfilingTimer::setCount(size_t historyCount) {
        history.clear();
        history.resize(historyCount, 0);
    }

    void ProfilingTimer::clear() {
        setCount(history.size());
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
        assert(!history.empty());
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
        assert(!history.empty());
        return std::accumulate(history.begin(), history.end(), 0.0) / history.size();
    }
};