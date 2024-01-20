//
// RT64
//

#include "rt64_rsp_processor.h"

#include "rt64_buffer_uploader.h"

namespace RT64 {
    // RSPProcessor

    RSPProcessor::RSPProcessor(RenderDevice *device) {
        processSet = std::make_unique<RSPProcessDescriptorSet>(device);
        modifySet = std::make_unique<RSPModifyDescriptorSet>(device);
    }

    RSPProcessor::~RSPProcessor() { }
    
    void RSPProcessor::process(const ProcessParams &p) {
        const DrawData &drawData = *p.drawData;
        const uint32_t drawVertexCount = drawData.vertexCount();
        processCB.vertexStart = uint32_t(p.outputBuffers->screenPosBuffer.computedSize / (sizeof(float) * 4));
        processCB.vertexCount = drawVertexCount - processCB.vertexStart;
        processCB.prevFrameWeight = p.prevFrameWeight;
        processCB.curFrameWeight = p.curFrameWeight;
        p.outputBuffers->screenPosBuffer.computedSize += processCB.vertexCount * sizeof(float) * 4;
        p.outputBuffers->genTexCoordBuffer.computedSize += processCB.vertexCount * sizeof(float) * 2;
        p.outputBuffers->shadedColBuffer.computedSize += processCB.vertexCount * sizeof(float) * 4;
        processSet->setBuffer(processSet->srcPos, p.drawBuffers->positionBuffer.get(), p.drawBuffers->positionBuffer.getView(0));
        processSet->setBuffer(processSet->srcVel, p.drawBuffers->velocityBuffer.get(), p.drawBuffers->velocityBuffer.getView(0));
        processSet->setBuffer(processSet->srcTc, p.drawBuffers->texcoordBuffer.get(), p.drawBuffers->texcoordBuffer.getView(0));
        processSet->setBuffer(processSet->srcCol, p.drawBuffers->normalColorBuffer.get(), p.drawBuffers->normalColorBuffer.getView(0));
        processSet->setBuffer(processSet->srcNorm, p.drawBuffers->normalColorBuffer.get(), p.drawBuffers->normalColorBuffer.getView(1));
        processSet->setBuffer(processSet->srcViewProjIndices, p.drawBuffers->viewProjIndicesBuffer.get(), p.drawBuffers->viewProjIndicesBuffer.getView(0));
        processSet->setBuffer(processSet->srcWorldIndices, p.drawBuffers->worldIndicesBuffer.get(), p.drawBuffers->worldIndicesBuffer.getView(0));
        processSet->setBuffer(processSet->srcFogIndices, p.drawBuffers->fogIndicesBuffer.get(), p.drawBuffers->fogIndicesBuffer.getView(0));
        processSet->setBuffer(processSet->srcLightIndices, p.drawBuffers->lightIndicesBuffer.get(), p.drawBuffers->lightIndicesBuffer.getView(0));
        processSet->setBuffer(processSet->srcLightCounts, p.drawBuffers->lightCountsBuffer.get(), p.drawBuffers->lightCountsBuffer.getView(0));
        processSet->setBuffer(processSet->srcLookAtIndices, p.drawBuffers->lookAtIndicesBuffer.get(), p.drawBuffers->lookAtIndicesBuffer.getView(0));
        processSet->setBuffer(processSet->rspViewportVector, p.drawBuffers->rspViewportsBuffer.get(), RenderBufferStructuredView(sizeof(interop::RSPViewport)));
        processSet->setBuffer(processSet->rspFogVector, p.drawBuffers->rspFogBuffer.get(), RenderBufferStructuredView(sizeof(interop::RSPFog)));
        processSet->setBuffer(processSet->rspLightVector, p.drawBuffers->rspLightsBuffer.get(), RenderBufferStructuredView(sizeof(interop::RSPLight)));
        processSet->setBuffer(processSet->rspLookAtVector, p.drawBuffers->rspLookAtBuffer.get(), RenderBufferStructuredView(sizeof(interop::RSPLookAt)));
        processSet->setBuffer(processSet->viewProjTransforms, p.drawBuffers->viewProjTransformsBuffer.get(), RenderBufferStructuredView(sizeof(interop::float4x4)));
        processSet->setBuffer(processSet->worldTransforms, p.drawBuffers->worldTransformsBuffer.get(), RenderBufferStructuredView(sizeof(interop::float4x4)));
        processSet->setBuffer(processSet->dstPos, p.outputBuffers->screenPosBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
        processSet->setBuffer(processSet->dstTc, p.outputBuffers->genTexCoordBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 2));
        processSet->setBuffer(processSet->dstCol, p.outputBuffers->shadedColBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));

        const uint32_t modifyCount = drawData.modifyCount();
        modifyCB.modifyCount = modifyCount;
        modifySet->setBuffer(modifySet->srcModifyPos, p.drawBuffers->modifyPosUintsBuffer.get(), p.drawBuffers->modifyPosUintsBuffer.getView(0));
        modifySet->setBuffer(modifySet->screenPos, p.outputBuffers->screenPosBuffer.buffer.get(), RenderBufferStructuredView(sizeof(float) * 4));
    }

    void RSPProcessor::recordCommandList(RenderWorker *worker, const ShaderLibrary *shaderLibrary, const OutputBuffers *outputBuffers) {
        const uint32_t ThreadGroupSize = 64;
        
        if (processCB.vertexCount > 0) {
            RenderBufferBarrier beforeBarriers[] = {
                RenderBufferBarrier(outputBuffers->screenPosBuffer.buffer.get(), RenderBufferAccess::WRITE),
                RenderBufferBarrier(outputBuffers->genTexCoordBuffer.buffer.get(), RenderBufferAccess::WRITE),
                RenderBufferBarrier(outputBuffers->shadedColBuffer.buffer.get(), RenderBufferAccess::WRITE)
            };

            RenderBufferBarrier afterBarriers[] = {
                RenderBufferBarrier(outputBuffers->screenPosBuffer.buffer.get(), RenderBufferAccess::READ),
                RenderBufferBarrier(outputBuffers->genTexCoordBuffer.buffer.get(), RenderBufferAccess::READ),
                RenderBufferBarrier(outputBuffers->shadedColBuffer.buffer.get(), RenderBufferAccess::READ)
            };

            const int dispatchCount = (processCB.vertexCount + ThreadGroupSize - 1) / ThreadGroupSize;
            worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeBarriers, uint32_t(std::size(beforeBarriers)));
            worker->commandList->setPipeline(shaderLibrary->rspProcess.pipeline.get());
            worker->commandList->setComputePipelineLayout(shaderLibrary->rspProcess.pipelineLayout.get());
            worker->commandList->setComputePushConstants(0, &processCB);
            worker->commandList->setComputeDescriptorSet(processSet->get(), 0);
            worker->commandList->dispatch(dispatchCount, 1, 1);
            worker->commandList->barriers(RenderBarrierStage::GRAPHICS, afterBarriers, uint32_t(std::size(afterBarriers)));
        }

        if (modifyCB.modifyCount > 0) {
            RenderBufferBarrier beforeBarrier = RenderBufferBarrier(outputBuffers->screenPosBuffer.buffer.get(), RenderBufferAccess::WRITE);
            RenderBufferBarrier afterBarrier = RenderBufferBarrier(outputBuffers->screenPosBuffer.buffer.get(), RenderBufferAccess::READ);
            const int dispatchCount = (modifyCB.modifyCount + ThreadGroupSize - 1) / ThreadGroupSize;
            worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeBarrier);
            worker->commandList->setPipeline(shaderLibrary->rspModify.pipeline.get());
            worker->commandList->setComputePipelineLayout(shaderLibrary->rspModify.pipelineLayout.get());
            worker->commandList->setComputePushConstants(0, &modifyCB);
            worker->commandList->setComputeDescriptorSet(modifySet->get(), 0);
            worker->commandList->dispatch(dispatchCount, 1, 1);
            worker->commandList->barriers(RenderBarrierStage::GRAPHICS, afterBarrier);
        }
    }
};
