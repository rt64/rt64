//
// RT64
//

#pragma once

#include "common/rt64_common.h"

namespace RT64 {
    struct CommandWarning {
        enum class IndexType {
            LoadIndex,
            TileIndex,
            CallIndex
        };

        IndexType indexType;
        std::string message;

        union {
            struct {
                uint32_t index;
            } load;

            struct {
                uint32_t index;
            } tile;

            struct {
                uint32_t fbPairIndex;
                uint32_t projIndex;
                uint32_t callIndex;
            } call;
        };

        static CommandWarning format(const char *msg, ...);
    };
};