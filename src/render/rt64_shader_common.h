//
// RT64
//

#pragma once

#include <stdint.h>
#include <string>
#include <sstream>

#include "gbi/rt64_f3d.h"

#include "rhi/rt64_render_interface.h"
#include "shared/rt64_color_combiner.h"
#include "shared/rt64_other_mode.h"
#include "shared/rt64_render_params.h"

namespace RT64 {
#pragma pack(push,1)
    struct ShaderDescription {
        interop::ColorCombiner colorCombiner;
        interop::OtherMode otherMode;
        interop::RenderFlags flags;

        void maskUnusedParameters();
        uint64_t hash() const;
        std::string toShader() const;
    };
#pragma pack(pop)
};