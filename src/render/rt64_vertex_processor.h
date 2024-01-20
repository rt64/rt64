//
// RT64
//

#pragma once

#include "hle/rt64_workload.h"

#include "rt64_descriptor_sets.h"

namespace RT64 {
    struct VertexProcessor {
        struct WorldCB {
            uint32_t vertexStart;
            uint32_t vertexCount;
            float prevFrameWeight;
            float curFrameWeight;
        };

        struct ProcessParams {
            RenderWorker *worker = nullptr;
            DrawData *drawData = nullptr;
            DrawBuffers *drawBuffers = nullptr;
            OutputBuffers *outputBuffers = nullptr;
            float curFrameWeight = 1.0f;
            float prevFrameWeight = 0.0f;
        };

        WorldCB worldCB;
        std::unique_ptr<RSPWorldDescriptorSet> descriptorSet;

        VertexProcessor(RenderDevice *device);
        ~VertexProcessor();
        void process(const ProcessParams &p);
        void recordCommandList(RenderWorker *worker, const ShaderLibrary *shaderLibrary, const OutputBuffers *outputBuffers);
    };
};
