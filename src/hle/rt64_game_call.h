//
// RT64
//

#pragma once

#include "render/rt64_shader_common.h"

#if SCRIPT_ENABLED
#include "script/rt64_script.h"
#endif

#include "rt64_draw_call.h"

namespace RT64 {
    struct GameCall {
        DrawCall callDesc;
        ShaderDescription shaderDesc;

        struct {
            // Only applies to raw triangle geometry from LLE triangle commands.
            uint32_t rawVertexStart;

            // Only applies to indexed geometry from the RSP.
            uint32_t faceIndicesStart;
        } meshDesc;

        struct {
            uint32_t highlightColor;
        } debuggerDesc;

        struct {
            bool enabled;
#       if SCRIPT_ENABLED
            CallMatchCallback *matchCallback; // FIXME: This only supports one match callback active at a time per draw call.
#       endif
        } lerpDesc;
    };
};