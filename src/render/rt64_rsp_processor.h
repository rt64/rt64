//
// RT64
//

#pragma once

#include "hle/rt64_workload.h"

#include "rt64_descriptor_sets.h"

namespace RT64 {
    struct RSPProcessor {
        struct ProcessCB {
            uint32_t vertexStart;
            uint32_t vertexCount;
            float prevFrameWeight;
            float curFrameWeight;
        };

        struct ModifyCB {
            uint32_t modifyCount;
        };

        struct ProcessParams {
            RenderWorker *worker = nullptr;
            DrawData *drawData = nullptr;
            DrawBuffers *drawBuffers = nullptr;
            OutputBuffers *outputBuffers = nullptr;
            float prevFrameWeight = 0.0f;
            float curFrameWeight = 1.0f;
        };

        ProcessCB processCB;
        ModifyCB modifyCB;
        std::unique_ptr<RSPProcessDescriptorSet> processSet;
        std::unique_ptr<RSPModifyDescriptorSet> modifySet;

        RSPProcessor(RenderDevice *device);
        ~RSPProcessor();
        void process(const ProcessParams &p);
        void recordCommandList(RenderWorker *worker, const ShaderLibrary *shaderLibrary, const OutputBuffers *outputBuffers);
    };
};
