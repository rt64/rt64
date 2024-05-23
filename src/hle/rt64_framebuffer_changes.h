//
// RT64
//

// The framebuffer changes pool stores a group of 2D textures that are used to draw whatever changes 
// the CPU did to a tracked framebuffer. A change is usually represented by two textures: a color/depth
// texture and a boolean texture indicating which pixels have changed.
//
// This design allows both renderers to efficiently draw changes over the existing render targets and
// replicate them even at high resolution.

#pragma once

#include <map>

#include "common/rt64_common.h"
#include "render/rt64_descriptor_sets.h"
#include "render/rt64_render_worker.h"

namespace RT64 {
    struct FramebufferChange {
        enum class Type {
            Color,
            Depth
        };

        uint64_t id;
        Type type;
        uint32_t width;
        uint32_t height;
        std::unique_ptr<RenderTexture> pixelTexture;
        std::unique_ptr<RenderTexture> booleanTexture;
        std::unique_ptr<FramebufferDrawChangesDescriptorSet> drawDescSet;
        std::unique_ptr<FramebufferReadChangesDescriptorChangesSet> readChangesSet;
        bool used;

        FramebufferChange();
        ~FramebufferChange();
    };

    struct FramebufferChangePool {
        uint64_t newId;
        std::map<uint64_t, FramebufferChange> changesMap;

        FramebufferChangePool();
        ~FramebufferChangePool();
        void reset();
        FramebufferChange &use(RenderWorker *renderWorker, FramebufferChange::Type type, uint32_t width, uint32_t height, bool usesHDR);
        const FramebufferChange *get(uint64_t id) const;
        void release(uint64_t id);
    };
};