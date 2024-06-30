//
// RT64
//

#pragma once

#include <stdint.h>

#include "render/rt64_shader_common.h"
#include "shared/rt64_rsp_vertex_test_z.h"

namespace RT64 {
    struct InstanceDrawCall {
        enum class Type {
            Unknown,
            Raytracing,
            IndexedTriangles,
            RawTriangles,
            RegularRect,
            FillRect,
            VertexTestZ
        };

        Type type;

        union {
            struct {
                uint32_t hitGroupIndex;
                uint32_t queryMask;
                bool cullDisable;
            } raytracing;
            
            struct {
                ShaderDescription shaderDesc;
                const RenderPipeline *pipeline;
                RenderRect scissor;
                RenderViewport viewport;
                uint32_t indexStart;
                uint32_t faceCount;
                bool vertexTestZ;
                bool postBlendDitherNoise;
            } triangles;

            struct {
                RenderRect rect;
                RenderColor color;
            } clearRect;

            interop::RSPVertexTestZCB vertexTestZ;
        };

        InstanceDrawCall() {
            type = Type::Unknown;
        }

        ~InstanceDrawCall() { }
    };
};