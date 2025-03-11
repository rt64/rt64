//
// RT64
//

#pragma once

#include "plume_render_interface.h"

namespace RT64 {
    struct RenderWorker {
        plume::RenderDevice *device = nullptr;
        std::string name;
        std::unique_ptr<plume::RenderCommandQueue> commandQueue;
        std::unique_ptr<plume::RenderCommandList> commandList;
        std::unique_ptr<plume::RenderCommandFence> commandFence;

        RenderWorker(plume::RenderDevice *device, const std::string &name, plume::RenderCommandListType commandListType);
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
