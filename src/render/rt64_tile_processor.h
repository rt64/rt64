//
// RT64
//

#pragma once

#include "rt64_buffer_uploader.h"

namespace RT64 {
    struct GameFrame;
    struct WorkloadQueue;

    struct TileProcessor {
        std::unique_ptr<BufferUploader> bufferUploader;
        std::vector<BufferUploader::Upload> uploads;

        struct ProcessParams {
            RenderWorker *worker = nullptr;
            WorkloadQueue *workloadQueue = nullptr;
            GameFrame *curFrame = nullptr;
            const GameFrame *prevFrame = nullptr;
            float curFrameWeight = 1.0f;
            float prevFrameWeight = 0.0f;
        };

        TileProcessor();
        ~TileProcessor();
        void setup(RenderWorker *worker);
        void process(const ProcessParams &p);
        void upload(const ProcessParams &p);
    };
};