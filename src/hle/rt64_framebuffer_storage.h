//
// RT64
//

// The framebuffer storage can hold the state of the tracked RAM by all framebuffers
// at the start of a frame. Sometimes it's necessary for the high resolution renderer
// to reload the contents from RDRAM in case the resources get resized and discarded.
// The storage is the resource it can use to fix that.

#pragma once

#include "common/rt64_common.h"

#include <map>

namespace RT64 {
    struct FramebufferStorage {
        struct Handle {
            uint32_t fbPairIndex;
            uint32_t address;
            uint32_t rdramIndex;
            uint32_t size;
        };

        uint32_t rdramUsed;
        std::vector<uint8_t> rdramData;
        std::vector<Handle> handleVector;

        FramebufferStorage();
        void reset();
        void store(uint32_t fbPairIndex, uint32_t address, const uint8_t *data, uint32_t size);
        const Handle *get(uint32_t maxFbPairIndex, uint32_t address) const;
        const uint8_t *getRDRAM(const Handle &handle) const;
    };
};