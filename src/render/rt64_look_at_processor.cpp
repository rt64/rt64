//
// RT64
//

#include "rt64_look_at_processor.h"

#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload_queue.h"

namespace RT64 {
    // LookAtProcessor

    LookAtProcessor::LookAtProcessor() { }

    LookAtProcessor::~LookAtProcessor() { }

    void LookAtProcessor::setup(RenderWorker *worker) {
        bufferUploader = std::make_unique<BufferUploader>(worker->device);
    }

    void LookAtProcessor::process(const ProcessParams &p) {
        for (uint32_t w : p.curFrame->workloads) {
            Workload &workload = p.workloadQueue->workloads[w];
            DrawData &drawData = workload.drawData;
            const bool prevFrameValid = (p.prevFrame != nullptr) && p.curFrame->frameMap.workloads[w].mapped;
            const auto &curLookAts = drawData.rspLookAt;
            if (prevFrameValid) {
                const GameFrameMap::WorkloadMap &workloadMap = p.curFrame->frameMap.workloads[w];
                auto &lerpRspLookAts = drawData.lerpRspLookAt;
                lerpRspLookAts = curLookAts;

                for (size_t l = 0; l < curLookAts.size(); l++) {
                    const GameFrameMap::LookAtMap &lookAtMap = workloadMap.lookAt[l];
                    if (!lookAtMap.mapped) {
                        continue;
                    }

                    const interop::RSPLookAt &curLookAt = curLookAts[l];
                    interop::RSPLookAt &lerpLookAt = lerpRspLookAts[l];
                    lerpLookAt.x = curLookAt.x - lookAtMap.deltaX * (1.0f - p.curFrameWeight);
                    lerpLookAt.y = curLookAt.y - lookAtMap.deltaY * (1.0f - p.curFrameWeight);
                }
            }
        }
    }

    void LookAtProcessor::upload(const ProcessParams &p) {
        uploads.clear();

        for (uint32_t w : p.curFrame->workloads) {
            const bool prevFrameValid = (p.prevFrame != nullptr) && p.curFrame->frameMap.workloads[w].mapped;
            if (prevFrameValid) {
                Workload &workload = p.workloadQueue->workloads[w];
                DrawBuffers &drawBuffers = workload.drawBuffers;
                const DrawData &drawData = workload.drawData;
                std::pair<size_t, size_t> uploadRange = { 0, drawData.rspLookAt.size() };
                assert(drawData.rspLookAt.size() == drawData.lerpRspLookAt.size());
                uploads.emplace_back(BufferUploader::Upload{ drawData.lerpRspLookAt.data(), uploadRange, sizeof(interop::RSPLookAt), RenderBufferFlag::STORAGE, {}, &drawBuffers.rspLookAtBuffer });
            }
        }

        if (!uploads.empty()) {
            bufferUploader->submit(p.worker, uploads);
        }
    }
};