//
// RT64
//

#pragma once

#include <array>
#include <stdint.h>

#include "common/rt64_common.h"
#include "common/rt64_timer.h"

#define VI_STATUS_TYPE_BLANK 0
#define VI_STATUS_TYPE_RESERVED 1
#define VI_STATUS_TYPE_16_BIT 2
#define VI_STATUS_TYPE_32_BIT 3

#define VI_STATUS_AA_MODE_RESAMP_ALWAYS_FETCH 0
#define VI_STATUS_AA_MODE_RESAMP_FETCH_IF_NEEDED 1
#define VI_STATUS_AA_MODE_RESAMP_ONLY 2
#define VI_STATUS_AA_MODE_NONE 3

namespace RT64 {
    struct VI {
        static const uint32_t Width;
        static const uint32_t Height;

        union Status {
            struct {
                // Refer to VI_STATUS_TYPE_ values.
                unsigned type : 2;
                unsigned gammaDitherEnable : 1;
                // Use linear color space if disabled.
                unsigned gammaEnable : 1;
                unsigned divotEnable : 1;
                unsigned vbusClockEnable : 1;
                // Always on if interlaced.
                unsigned serrate : 1;
                unsigned testMode : 1;
                unsigned aaMode : 2;
                unsigned reserved : 1;
                unsigned diagnostics : 1;
                unsigned pixelAdvance : 4;
                unsigned ditherFilter : 1;
                unsigned padding : 15;
            };

            unsigned word;
        };

        union Burst {
            struct {
                unsigned hSyncWidth : 8;
                unsigned colorWidth : 8;
                unsigned vSyncWidth : 4;
                unsigned colorStart : 10;
                unsigned padding : 2;
            };

            unsigned word;
        };

        union HSync {
            struct {
                unsigned hSync : 12;
                unsigned padding0 : 4;
                unsigned leap : 5;
                unsigned padding1 : 11;
            };

            unsigned word;
        };

        union Leap {
            struct {
                unsigned leapB : 12;
                unsigned padding0 : 4;
                unsigned leapA : 12;
                unsigned padding1 : 4;
            };

            unsigned word;
        };

        union HRegion {
            struct {
                unsigned hEnd : 10;
                unsigned padding0 : 6;
                unsigned hStart : 10;
                unsigned padding1 : 6;
            };

            unsigned word;
        };

        union VRegion {
            struct {
                unsigned vEnd : 10;
                unsigned padding0 : 6;
                unsigned vStart : 10;
                unsigned padding1 : 6;
            };

            unsigned word;
        };

        union XTransform {
            struct {
                unsigned xScale : 12;
                unsigned padding0 : 4;
                unsigned xOffset : 12;
                unsigned padding1 : 4;
            };

            unsigned word;
        };

        union YTransform {
            struct {
                unsigned yScale : 12;
                unsigned padding0 : 4;
                unsigned yOffset : 12;
                unsigned padding1 : 4;
            };

            unsigned word;
        };

        Status status;
        unsigned origin;
        unsigned width;
        unsigned intr;
        unsigned vCurrentLine;
        Burst burst;
        unsigned vSync;
        HSync hSync;
        Leap leap;
        HRegion hRegion;
        VRegion vRegion;
        Burst vBurst;
        XTransform xTransform;
        YTransform yTransform;

        uint8_t fbSiz() const;
        uint32_t fbAddress() const;
        hlslpp::uint2 fbSize() const;
        float xScaleFloat() const;
        float xOffsetFloat() const;
        float yScaleFloat() const;
        float yOffsetFloat() const;
        RectI viewRectangle() const;
        RectI cropRectangle() const;
        float gamma() const;
        bool compatibleWith(const VI &vi) const;
        bool visible() const;
        bool operator!=(const VI &rhs) const;
    };

    struct VIHistory {
        struct Present {
            VI vi;
            uint32_t fbWidth;
        };

        std::array<Present, 3> history;
        std::array<uint32_t, 3> factors;
        int historyCursor;
        int factorCursor;

        VIHistory();
        void pushVI(const VI &vi, uint32_t fbWidth);
        void pushFactor(uint32_t factor);
        uint32_t logicalRateFromFactors();
        const Present &top() const;
    };
};