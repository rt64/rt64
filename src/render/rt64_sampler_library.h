//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct SamplerSet {
        std::unique_ptr<RenderSampler> wrapWrap;
        std::unique_ptr<RenderSampler> wrapMirror;
        std::unique_ptr<RenderSampler> wrapClamp;
        std::unique_ptr<RenderSampler> mirrorWrap;
        std::unique_ptr<RenderSampler> mirrorMirror;
        std::unique_ptr<RenderSampler> mirrorClamp;
        std::unique_ptr<RenderSampler> clampWrap;
        std::unique_ptr<RenderSampler> clampMirror;
        std::unique_ptr<RenderSampler> clampClamp;
        std::unique_ptr<RenderSampler> borderBorder;
    };

    struct SamplerLibrary {
        SamplerSet linear;
        SamplerSet nearest;
    };
};