//
// RT64
//

#include "rt64_vertex_processor.h"

#include "rt64_buffer_uploader.h"

namespace RT64 {
    // VertexProcessor

    VertexProcessor::VertexProcessor(RenderDevice *device) {
        descriptorSet = std::make_unique<RSPWorldDescriptorSet>(device);
    }

    VertexProcessor::~VertexProcessor() { }

    void VertexProcessor::process(const ProcessParams &p) {
        const DrawData &drawData = *p.drawData;
        const uint32_t drawVertexCount = drawData.vertexCount();
        worldCB.vertexStart = uint32_t(p.outputBuffers->worldPosBuffer.computedSize / (sizeof(float) * 4));
        worldCB.vertexCount = drawVertexCount - worldCB.vertexStart;
        worldCB.prevFrameWeight = p.prevFrameWeight;
        worldCB.curFrameWeight = p.curFrameWeight;
        p.outputBuffers->worldPosBuffer.computedSize += worldCB.vertexCount * sizeof(float) * 4;
        p.outputBuffers->worldNormBuffer.computedSize += worldCB.vertexCount * sizeof(float) * 4;
        p.outputBuffers->worldVelBuffer.computedSize += worldCB.vertexCount * sizeof(float) * 4;
        
        descriptorSet->setBuffer(descriptorSet->srcPos, p.drawBuffers->positionBuffer.get(), p.drawBuffers->positionBuffer.getView(0));
        descriptorSet->setBuffer(descriptorSet->srcVel, p.drawBuffers->velocityBuffer.get(), p.drawBuffers->velocityBuffer.getView(0));
        descriptorSet->setBuffer(descriptorSet->srcNorm, p.drawBuffers->normalColorBuffer.get(), p.drawBuffers->normalColorBuffer.getView(1));
        descriptorSet->setBuffer(descriptorSet->srcIndices, p.drawBuffers->worldIndicesBuffer.get(), p.drawBuffers->worldIndicesBuffer.getView(0));
        descriptorSet->setBuffer(descriptorSet->worldMats, p.drawBuffers->worldTransformsBuffer.get(), RenderBufferStructuredView(sizeof(interop::float4x4)));
        descriptorSet->setBuffer(descriptorSet->invTWorldMats, p.drawBuffers->invTWorldTransformsBuffer.get(), RenderBufferStructuredView(sizeof(interop::float4x4)));
        descriptorSet->setBuffer(descriptorSet->prevWorldMats, p.drawBuffers->prevWorldTransformsBuffer.get(), RenderBufferStructuredView(sizeof(interop::float4x4)));
        descriptorSet->setBuffer(descriptorSet->dstPos, p.outputBuffers->worldPosBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
        descriptorSet->setBuffer(descriptorSet->dstNorm, p.outputBuffers->worldNormBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
        descriptorSet->setBuffer(descriptorSet->dstVel, p.outputBuffers->worldVelBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
    }

    void VertexProcessor::recordCommandList(RenderWorker *worker, const ShaderLibrary *shaderLibrary, const OutputBuffers *outputBuffers) {
        if (worldCB.vertexCount == 0) {
            return;
        }

        RenderBufferBarrier beforeBarriers[] = {
            RenderBufferBarrier(outputBuffers->worldPosBuffer.buffer.get(), RenderBufferAccess::WRITE),
            RenderBufferBarrier(outputBuffers->worldNormBuffer.buffer.get(), RenderBufferAccess::WRITE),
            RenderBufferBarrier(outputBuffers->worldVelBuffer.buffer.get(), RenderBufferAccess::WRITE)
        };

        RenderBufferBarrier afterBarriers[] = {
            RenderBufferBarrier(outputBuffers->worldPosBuffer.buffer.get(), RenderBufferAccess::READ),
            RenderBufferBarrier(outputBuffers->worldNormBuffer.buffer.get(), RenderBufferAccess::READ),
            RenderBufferBarrier(outputBuffers->worldVelBuffer.buffer.get(), RenderBufferAccess::READ)
        };

        const uint32_t ThreadGroupSize = 64;
        const uint32_t dispatchCount = (worldCB.vertexCount + ThreadGroupSize - 1) / ThreadGroupSize;
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeBarriers, uint32_t(std::size(beforeBarriers)));
        worker->commandList->setPipeline(shaderLibrary->rspWorld.pipeline.get());
        worker->commandList->setComputePipelineLayout(shaderLibrary->rspWorld.pipelineLayout.get());
        worker->commandList->setComputePushConstants(0, &worldCB);
        worker->commandList->setComputeDescriptorSet(descriptorSet->get(), 0);
        worker->commandList->dispatch(dispatchCount, 1, 1);
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterBarriers, uint32_t(std::size(afterBarriers)));
    }
};
