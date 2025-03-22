//
// RT64
//

#pragma once

#include "common/rt64_profiling_timer.h"
#include "gui/rt64_inspector.h"
#include "render/rt64_vi_renderer.h"

#include "rt64_application_window.h"
#include "rt64_present.h"
#include "rt64_shared_queue_resources.h"

#define PRESENT_QUEUE_SIZE 4

namespace RT64 {
    struct WorkloadQueue;

    struct PresentQueue {
        struct External {
            ApplicationWindow *appWindow = nullptr;
            RenderDevice *device = nullptr;
            RenderSwapChain *swapChain = nullptr;
            RenderWorker *presentGraphicsWorker = nullptr;
            WorkloadQueue *workloadQueue = nullptr;
            SharedQueueResources *sharedResources = nullptr;
            const ShaderLibrary *shaderLibrary = nullptr;
            UserConfiguration::GraphicsAPI createdGraphicsAPI = UserConfiguration::GraphicsAPI::OptionCount;
        };

        External ext;
        std::array<Present, PRESENT_QUEUE_SIZE> presents;
        int threadCursor;
        int writeCursor;
        int barrierCursor;
        std::mutex cursorMutex;
        std::condition_variable cursorCondition;
        uint64_t presentId;
        std::mutex presentIdMutex;
        std::condition_variable presentIdCondition;
        std::thread *presentThread = nullptr;
        std::mutex threadMutex;
        std::atomic<bool> presentThreadRunning = false;
        std::recursive_mutex inspectorMutex;
        std::mutex screenFbChangePoolMutex;
        Framebuffer scratchFb;
        FramebufferChangePool scratchFbChangePool;
        FramebufferChangePool screenFbChangePool;
        std::atomic<bool> viewRDRAM = false;
        std::vector<std::unique_ptr<RenderFramebuffer>> swapChainFramebuffers;
        std::unique_ptr<RenderCommandSemaphore> acquiredSemaphore;
        std::unique_ptr<RenderCommandSemaphore> drawSemaphore;
        std::unique_ptr<VIRenderer> viRenderer;
        std::unique_ptr<Inspector> inspector;
        ProfilingTimer presentProfiler = ProfilingTimer(120);
        Timestamp presentTimestamp;
        VIHistory viHistory;
        bool presentWaitEnabled = false;

        PresentQueue();
        ~PresentQueue();
        void reset();
        void advanceToNextPresent();
        void repeatLastPresent();
        uint32_t previousWriteCursor() const;
        void waitForIdle();
        void waitForPresentId(uint64_t waitId);
        void setup(const External &ext);
        void threadPresent(const Present &present, bool &swapChainValid);
        void skipInterpolation();
        void notifyPresentId(const Present &present);
        bool detectPresentWait();
        void threadAdvanceBarrier();
        void threadLoop();
    };
};