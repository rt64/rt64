//
// RT64
//

#include <cmath>
#include <thread>

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

    void Timer::preciseSleepUntil(const Timestamp endTime) {
        auto startTime = std::chrono::high_resolution_clock::now();
        int64_t remainingNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
        if (remainingNanoseconds < 0) {
            return;
        }
        double remainingSeconds = remainingNanoseconds / 1'000'000'000.0;

        // Duration of the fixed sleep, in nanoseconds.
        constexpr int64_t fixedSleepDuration = 100'000;
        // Upper bounds of the fixed sleep. Any durations above this will be ignored when
        // updating the fixed sleep statistics.
        constexpr int64_t fixedSleepUpperBound = 2'000'000;
        // Number of standard deviations to use as the upper bounds for the duration estimate.
        constexpr double estimateStddevCount = 2.0;

        // Statistics to keep track of the actual fixed sleep performance.
        thread_local double fixedSleepEstimate = fixedSleepDuration / 1'000'000'000.0;
        thread_local double mean = fixedSleepDuration / 1'000'000'000.0;
        thread_local double m2 = 0;
        thread_local uint64_t fixedSleepTotalCount = 1;
        
        // If the duration left to sleep is at least twice that of the nanosleep interval, use nanosleeps until the duration is too short for them to fit.
        auto waitStart = std::chrono::high_resolution_clock::now();
        while (remainingSeconds > fixedSleepEstimate) {
            // Perform a fixed sleep and measure the time that actually passed.
            std::this_thread::sleep_for(std::chrono::nanoseconds(fixedSleepDuration));
            auto afterSleep = std::chrono::high_resolution_clock::now();
            double measuredDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(afterSleep - waitStart).count() / 1'000'000'000.0;

            // Adjust the remaining time based on the real duration of the fixed sleep.
            remainingSeconds -= measuredDuration;

            // Update the fixed sleep statistics if the sleep was within the expected bounds.
            if (measuredDuration < fixedSleepUpperBound / 1'000'000'000.0) {
                fixedSleepTotalCount++;

                double delta = measuredDuration - mean;
                mean += delta / fixedSleepTotalCount;
                m2 += delta * (measuredDuration - mean);
                double stddev = sqrt(m2 / (fixedSleepTotalCount - 1));
                fixedSleepEstimate = mean + estimateStddevCount * stddev;
            }

            waitStart = afterSleep;
        }

        // Spin until the end time is reached.
        while (std::chrono::high_resolution_clock::now() < endTime);
    }
};
