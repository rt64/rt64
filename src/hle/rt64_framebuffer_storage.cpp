//
// RT64
//

#include "rt64_framebuffer_storage.h"

#include <cassert>
#include <cstring>

namespace RT64 {
    // FramebufferStorage

    FramebufferStorage::FramebufferStorage() {
        reset();
    }

    void FramebufferStorage::reset() {
        rdramUsed = 0;
        handleVector.clear();
    }

    void FramebufferStorage::store(uint32_t fbPairIndex, uint32_t address, const uint8_t *data, uint32_t size) {
        uint32_t dstIndex = rdramUsed;
        rdramUsed += size;
        if (rdramUsed > rdramData.size()) {
            const uint32_t newSize = (rdramUsed * 3) / 2;
            rdramData.resize(newSize, 0);
        }

        memcpy(rdramData.data() + dstIndex, data, size);

        Handle handle;
        handle.fbPairIndex = fbPairIndex;
        handle.address = address;
        handle.rdramIndex = dstIndex;
        handle.size = size;
        handleVector.emplace_back(handle);
    }

    const FramebufferStorage::Handle *FramebufferStorage::get(uint32_t maxFbPairIndex, uint32_t address) const {
        const FramebufferStorage::Handle *maxHandle = nullptr;
        for (const auto &handle : handleVector) {
            if (handle.fbPairIndex > maxFbPairIndex) {
                continue;
            }

            if (handle.address != address) {
                continue;
            }

            maxHandle = &handle;
        }

        return maxHandle;
    }

    const uint8_t *FramebufferStorage::getRDRAM(const Handle &handle) const {
        assert((handle.rdramIndex + handle.size) <= rdramData.size());
        return rdramData.data() + handle.rdramIndex;
    }
};