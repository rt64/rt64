//
// RT64
//

#include "rt64_fog_processor.h"

#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload_queue.h"

namespace RT64 {
    // FogProcessor

    FogProcessor::FogProcessor() {}

    FogProcessor::~FogProcessor() {}

    void FogProcessor::setup(RenderWorker *worker) {
        bufferUploader = std::make_unique<BufferUploader>(worker->device);
    }

    void FogProcessor::process(const ProcessParams &p) {
        for (uint32_t w : p.curFrame->workloads) {
            Workload &workload = p.workloadQueue->workloads[w];
            DrawData &drawData = workload.drawData;
            const bool prevFrameValid = (p.prevFrame != nullptr) && p.curFrame->frameMap.workloads[w].mapped;
            const auto &curFogs = drawData.rspFog;
            if (prevFrameValid) {
                const GameFrameMap::WorkloadMap &workloadMap = p.curFrame->frameMap.workloads[w];
                auto &lerpRspFogs = drawData.lerpRspFog;
                lerpRspFogs = curFogs;

                for (size_t l = 0; l < curFogs.size(); l++) {
                    const GameFrameMap::FogMap &fogMap = workloadMap.fog[l];
                    if (!fogMap.mapped) {
                        continue;
                    }

                    const interop::RSPFog &curFog = curFogs[l];
                    interop::RSPFog &lerpFog = lerpRspFogs[l];
                    lerpFog.mul = curFog.mul - fogMap.deltaMul * (1.0f - p.curFrameWeight);
                    lerpFog.offset = curFog.offset - fogMap.deltaOffset * (1.0f - p.curFrameWeight);
                }
            }
        }
    }

    void FogProcessor::upload(const ProcessParams &p) {
        uploads.clear();

        for (uint32_t w : p.curFrame->workloads) {
            const bool prevFrameValid = (p.prevFrame != nullptr) && p.curFrame->frameMap.workloads[w].mapped;
            if (prevFrameValid) {
                Workload &workload = p.workloadQueue->workloads[w];
                DrawBuffers &drawBuffers = workload.drawBuffers;
                const DrawData &drawData = workload.drawData;
                std::pair<size_t, size_t> uploadRange = { 0, drawData.rspFog.size() };
                assert(drawData.rspFog.size() == drawData.lerpRspFog.size());
                uploads.emplace_back(BufferUploader::Upload{ drawData.lerpRspFog.data(), uploadRange, sizeof(interop::RSPFog), RenderBufferFlag::STORAGE, {}, &drawBuffers.rspFogBuffer });
            }
        }

        if (!uploads.empty()) {
            bufferUploader->submit(p.worker, uploads);
        }
    }
};