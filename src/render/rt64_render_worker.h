//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct RenderWorker {
        RenderDevice *device = nullptr;
        std::string name;
        std::unique_ptr<RenderCommandQueue> commandQueue;
        std::unique_ptr<RenderCommandList> commandList;
        std::unique_ptr<RenderCommandFence> commandFence;

        RenderWorker(RenderDevice *device, const std::string &name, RenderCommandListType commandListType);
        ~RenderWorker();
        void execute();
        void wait();
    };

    // RAII convenience class for aiding opening, closing and execution of command lists inside a scope.

    struct RenderWorkerExecution {
        RenderWorker *worker;

        RenderWorkerExecution(RenderWorker *worker);
        ~RenderWorkerExecution();
    };
};