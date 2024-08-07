//
// RT64
//

#include <cassert>

#include "rt64_upscaler.h"

// Upscaler

RT64::Upscaler::QualityMode RT64::Upscaler::getQualityAuto(int displayWidth, int displayHeight) {
    assert(displayWidth > 0);
    assert(displayHeight > 0);

    // Get the most appropriate quality level for the target resolution.
    const uint64_t PixelsDisplay = displayWidth * displayHeight;
    const uint64_t Pixels720p = 1280 * 720;
    const uint64_t Pixels1080p = 1920 * 1080;
    const uint64_t Pixels1440p = 2560 * 1440;
    const uint64_t Pixels4K = 3840 * 2160;
    if (PixelsDisplay <= Pixels720p) {
        return QualityMode::UltraQuality;
    }
    else if (PixelsDisplay <= Pixels1080p) {
        return QualityMode::Quality;
    }
    else if (PixelsDisplay <= Pixels1440p) {
        return QualityMode::Balanced;
    }
    else if (PixelsDisplay <= Pixels4K) {
        return QualityMode::Performance;
    }
    else {
        return QualityMode::UltraPerformance;
    }
}