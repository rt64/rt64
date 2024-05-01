//
// RT64
//

#include <algorithm>
#include <cassert>
#include <memory.h>
#include <stdio.h>

#include "common/rt64_common.h"
#include "gbi/rt64_f3d.h"

#include "rt64_vi.h"

namespace RT64 {
    // VI
    
    const uint32_t VI::Width = 640;
    const uint32_t VI::Height = 480;

    RectI VI::viewRectangle() const {
        // TODO
        RectI rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = Width;
        rect.h = Height;
        return rect;
    }

    RectI VI::cropRectangle() const {
        // TODO
        RectI rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = Width;
        rect.h = Height;
        return rect;
    }

    float VI::gamma() const {
        const float GammaCorrection = 1.0f / 2.2f;
        return status.gammaEnable ? GammaCorrection : 1.0f;
    }

    bool VI::compatibleWith(const VI &vi) const {
        return
            (width == vi.width) &&
            (hRegion.hStart == vi.hRegion.hStart) &&
            (hRegion.hEnd == vi.hRegion.hEnd) &&
            (vRegion.vStart == vi.vRegion.vStart) &&
            (vRegion.vEnd == vi.vRegion.vEnd) &&
            (xTransform.xScale == vi.xTransform.xScale) &&
            (xTransform.xOffset == vi.xTransform.xOffset) &&
            (yTransform.yScale == vi.yTransform.yScale) &&
            (yTransform.yOffset == vi.yTransform.yOffset);
    }

    bool VI::visible() const {
        return (status.type != VI_STATUS_TYPE_BLANK) && (hRegion.hStart > 0);
    }

    bool VI::operator!=(const VI &rhs) const {
        return
            (status.word != rhs.status.word) ||
            (origin != rhs.origin) ||
            (width != rhs.width) ||
            (intr != rhs.intr) ||
            (vCurrentLine != rhs.vCurrentLine) ||
            (burst.word != rhs.burst.word) ||
            (vSync != rhs.vSync) ||
            (hSync.word != rhs.hSync.word) ||
            (leap.word != rhs.leap.word) ||
            (hRegion.word != rhs.hRegion.word) ||
            (vRegion.word != rhs.vRegion.word) ||
            (vBurst.word != rhs.vBurst.word) ||
            (xTransform.word != rhs.xTransform.word) ||
            (yTransform.word != rhs.yTransform.word);
    }

    uint8_t VI::fbSiz() const {
        switch (status.type) {
        case VI_STATUS_TYPE_16_BIT:
            return G_IM_SIZ_16b;
        case VI_STATUS_TYPE_32_BIT:
            return G_IM_SIZ_32b;
        case VI_STATUS_TYPE_BLANK:
        default:
            return 0;
        }
    }

    uint32_t VI::fbAddress() const {
        uint8_t siz = fbSiz();

        // Estimate the origin is off by one or two rows.
        if (siz >= G_IM_SIZ_16b) {
            const bool interlacedStep = status.serrate && (vCurrentLine & 0x1);
            const uint32_t rowBytes = width * (1U << (siz - 1));
            const uint32_t rowCount = interlacedStep ? 2 : 1;
            const uint32_t rowOffset = rowBytes * rowCount;
            if (origin >= rowOffset) {
                return origin - rowOffset;
            }
        }

        return origin;
    }

    hlslpp::uint2 VI::fbSize() const {
        hlslpp::uint2 size = { width, 0 };
        
        // In interlaced without deflickering, the stride of the framebuffer is usually double of 
        // what its actual row size is. We detect for such a case and return half the width.
        if (status.serrate) {
            const float estimatedWidth = (hRegion.hEnd - hRegion.hStart) / xScaleFloat();
            const float interlacedTolerance = 1.875f;
            if (estimatedWidth < (width / interlacedTolerance)) {
                size.x = width / 2;
            }
        }

        // We can make a close estimate of the height the framebuffer will use by using the width
        // that was just fixed to eliminate interlacing.
        size.y = lround(float(vRegion.vEnd - vRegion.vStart) / (2.0f * yScaleFloat() * (float(size.x) / float(width))));

        // Most of the time, the height is missing a few rows because the framebuffer is offset 
        // at the origin and an extra row is left at the end to account for filtering.
        // We add two extra rows to whatever result we get and try to get the closest clean 
        // multiplier of the specified Division factor.
        const uint32_t ExtraRows = 2;
        const uint32_t Divisor = 4;
        size.y += ExtraRows;
        size.y = lround(float(size.y) / Divisor) * Divisor;

        return size;
    }

    float VI::xScaleFloat() const {
        return (1024.0f / xTransform.xScale);
    }

    float VI::xOffsetFloat() const {
        return xTransform.xOffset / 1024.0f;
    }

    float VI::yScaleFloat() const {
        return (1024.0f / yTransform.yScale);
    }

    float VI::yOffsetFloat() const {
        return yTransform.yOffset / 1024.0f;
    }

    // VIHistory

    VIHistory::VIHistory() {
        historyCursor = 0;
        factorCursor = 0;
        history.fill({});
        factors.fill(0);
    }

    void VIHistory::pushVI(const VI &vi, uint32_t fbWidth) {
        historyCursor = (historyCursor + 1) % history.size();
        Present &entry = history[historyCursor];
        entry.vi = vi;
        entry.fbWidth = fbWidth;
    }

    void VIHistory::pushFactor(uint32_t factor) {
        factorCursor = (factorCursor + 1) % factors.size();
        factors[factorCursor] = factor;
    }

    uint32_t VIHistory::logicalRateFromFactors() {
        if ((factors[0] != 0) && std::all_of(factors.begin(), factors.end(), [&](uint32_t factor) { return factor == factors[0]; })) {
            const uint32_t FullRate = 60; // TODO: PAL support.
            return FullRate / factors[0];
        }
        else {
            return 0;
        }
    }

    const VIHistory::Present &VIHistory::top() const {
        return history[historyCursor];
    }
};