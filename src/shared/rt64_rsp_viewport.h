//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
#include "common/rt64_common.h"

namespace interop {
#endif
    struct RSPViewport {
        float3 scale;
        float3 translate;

        static RSPViewport identity() {
            RSPViewport viewport;
            viewport.scale = float3(1.0f, 1.0f, 1.0f);
            viewport.translate = float3(0.0f, 0.0f, 0.0f);
            return viewport;
        }

#ifdef HLSL_CPU
        RT64::FixedRect rect(const int16_t *clipRatios) const {
            int32_t x1 = int32_t(lround((translate.x - scale.x * float(clipRatios[0])) * 4.0f));
            int32_t y1 = int32_t(lround((translate.y - scale.y * float(clipRatios[1])) * 4.0f));
            int32_t x2 = int32_t(lround((translate.x - scale.x * float(clipRatios[2])) * 4.0f));
            int32_t y2 = int32_t(lround((translate.y - scale.y * float(clipRatios[3])) * 4.0f));
            return RT64::FixedRect(std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2));
        }

        float minDepth() const {
            return translate.z - scale.z;
        }

        float maxDepth() const {
            return translate.z + scale.z;
        }
#endif
    };
#ifdef HLSL_CPU
};
#endif