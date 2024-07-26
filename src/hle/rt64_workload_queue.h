//
// RT64
//

#pragma once

#include <array>

#include "common/rt64_enhancement_configuration.h"
#include "common/rt64_profiling_timer.h"
#include "common/rt64_user_configuration.h"
#include "render/rt64_framebuffer_renderer.h"
#include "render/rt64_projection_processor.h"
#include "render/rt64_raster_shader_cache.h"
#include "render/rt64_tile_processor.h"
#include "render/rt64_transform_processor.h"

#include "rt64_shared_queue_resources.h"
#include "rt64_workload.h"

#if RT_ENABLED
#   include "render/rt64_raytracing_shader_cache.h"
#endif

#define WORKLOAD_QUEUE_SIZE 4

namespace RT64 {
    struct PresentQueue;

    struct WorkloadQueue {
        struct External {
            RenderDevice *device = nullptr;
            RenderWorker *workloadGraphicsWorker = nullptr;
            BufferUploader *workloadExtrasUploader = nullptr;
            BufferUploader *workloadVelocityUploader = nullptr;
            BufferUploader *workloadTilesUploader = nullptr;
            PresentQueue *presentQueue = nullptr;
            SharedQueueResources *sharedResources = nullptr;
            RasterShaderCache *rasterShaderCache = nullptr;
            TextureCache *textureCache = nullptr;
            const ShaderLibrary *shaderLibrary = nullptr;
            UserConfiguration::GraphicsAPI createdGraphicsAPI = UserConfiguration::GraphicsAPI::OptionCount;
#       if RT_ENABLED
            const RenderTexture *blueNoiseTexture = nullptr;
            RaytracingShaderCache *rtShaderCache = nullptr;
#       endif
        };

        struct WorkloadConfiguration {
            hlslpp::float2 resolutionScale = 1.0f;
            uint32_t downsampleMultiplier = 1;
            bool raytracingEnabled = false;
            float aspectRatioSource = 1.0f;
            float aspectRatioTarget = 1.0f;
            float aspectRatioScale = 1.0f;
            float extAspectPercentage = 1.0f;
            uint32_t targetRate = 0;
            bool fixRectLR = false;
            bool postBlendNoise = false;
        };

        External ext;
        std::array<Workload, WORKLOAD_QUEUE_SIZE> workloads;
        int threadCursor;
        int writeCursor;
        int barrierCursor;
        std::mutex cursorMutex;
        std::condition_variable cursorCondition;
        uint64_t workloadId;
        uint64_t lastPresentId;
        std::mutex workloadIdMutex;
        std::condition_variable workloadIdCondition;
        std::thread *renderThread = nullptr;
        std::thread *idleThread = nullptr;
        bool idleActive = false;
        std::mutex idleMutex;
        std::condition_variable idleCondition;
        std::mutex workerMutex;
        std::mutex threadMutex;
        std::atomic<bool> threadsRunning = false;
        std::atomic<bool> rtEnabled = false;
        std::atomic<bool> ubershadersOnly = false;
        std::atomic<bool> ubershadersVisible = false;
        std::unique_ptr<FramebufferRenderer> framebufferRenderer;
        std::unique_ptr<RenderFramebufferManager> renderFramebufferManager;
        TileProcessor tileProcessor;
        TransformProcessor transformProcessor;
        ProjectionProcessor projectionProcessor;
        std::unique_ptr<RSPProcessor> rspProcessor;
        std::unique_ptr<VertexProcessor> vertexProcessor;
        std::unique_ptr<RenderTarget> dummyDepthTarget;
        FramebufferChangePool scratchFbChangePool;
        ProfilingTimer rendererProfiler = ProfilingTimer(120);
        ProfilingTimer matchingProfiler = ProfilingTimer(120);
        ProfilingTimer workloadProfiler = ProfilingTimer(120);
        std::array<GameFrame, 2> gameFrames;
        uint32_t prevFrameIndex = uint32_t(gameFrames.size()) - 1;
        uint32_t curFrameIndex = 0;

        WorkloadQueue();
        ~WorkloadQueue();
        void reset();
        void advanceToNextWorkload();
        void repeatLastWorkload();
        uint32_t previousWriteCursor() const;
        void waitForIdle();
        void waitForWorkloadId(uint64_t waitId);
        void setup(const External &ext);
        void updateMultisampling();
        void threadConfigurationUpdate(WorkloadConfiguration &workloadConfig);
        void threadConfigurationValidate();
        void threadRenderFrame(GameFrame &curFrame, const GameFrame &prevFrame, const WorkloadConfiguration &workloadConfig,
            const DebuggerRenderer &debuggerRenderer, const DebuggerCamera &debuggerCamera, float curFrameWeight, float prevFrameWeight,
            float deltaTimeMs, RenderTargetKey overrideTargetKey, int32_t overrideTargetFbPairIndex, RenderTarget *overrideTarget,
            uint32_t overrideTargetModifier, bool uploadVelocity, bool uploadExtras, bool interpolateTiles);

        void threadAdvanceBarrier();
        void threadAdvanceWorkloadId(uint64_t newWorkloadId);
        void renderThreadLoop();
        void idleThreadLoop();
    };
};