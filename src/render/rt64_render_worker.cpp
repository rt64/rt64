//
// RT64
//

#include "rt64_render_worker.h"

namespace RT64 {
    // RenderWorker

    RenderWorker::RenderWorker(RenderDevice *device, const std::string &name, RenderCommandListType commandListType) {
        assert(device != nullptr);

        this->device = device;
        this->name = name;

        commandQueue = device->createCommandQueue(commandListType);
        commandList = commandQueue->createCommandList(commandListType);
        commandFence = device->createCommandFence();
    }

    RenderWorker::~RenderWorker() { }

    void RenderWorker::execute() {
        commandQueue->executeCommandLists(commandList.get(), commandFence.get());
    }

    void RenderWorker::wait() {
        commandQueue->waitForCommandFence(commandFence.get());
    }

    // RenderWorkerExecution

    RenderWorkerExecution::RenderWorkerExecution(RenderWorker *worker) {
        assert(worker != nullptr);

        this->worker = worker;
        worker->commandList->begin();
    }

    RenderWorkerExecution::~RenderWorkerExecution() {
        worker->commandList->end();
        worker->execute();
        worker->wait();
    }
};