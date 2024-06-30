//
// RT64
//

#include "rt64_workload.h"

namespace RT64 {
    // Common functions.

    static uint64_t roundUp(uint64_t value, uint64_t powerOf2Alignment) {
        return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
    }

    // Workload

    void Workload::reset() {
        submissionFrame = 0;
        fbPairCount = 0;
        fbPairSubmitted = 0;
        gameCallCount = 0;
        viOriginalRate = 0;
        workloadId = 0;
        extended.testZIndexCount = 0;
        extended.ditherNoiseStrength = 1.0f;

        commandWarnings.clear();
        pointLights.clear();
        fbChangePool.reset();
        fbStorage.reset();
        physicalAddressTransformMap.clear();
        transformIdMap.clear();

        // Always make sure there's at least one framebuffer pair, even if it's not configured yet, since a game can technically send
        // information to the RSP before it draws anything to a color image.
        adjustVector(fbPairs, 1);
        fbPairs[0].reset();

        resetDrawData();
        resetDrawDataRanges();
        resetRSPOutputBuffers();
        resetWorldOutputBuffers();
    }

    void Workload::resetDrawData() {
        drawData.posShorts.clear();
        drawData.velShorts.clear();
        drawData.tcFloats.clear();
        drawData.normColBytes.clear();
        drawData.viewProjIndices.clear();
        drawData.worldIndices.clear();
        drawData.fogIndices.clear();
        drawData.lightIndices.clear();
        drawData.lightCounts.clear();
        drawData.lookAtIndices.clear();
        drawData.faceIndices.clear();
        drawData.modifyPosUints.clear();
        drawData.rdpParams.clear();
        drawData.extraParams.clear();
        drawData.renderParams.clear();
        drawData.posTransformed.clear();
        drawData.posScreen.clear();
        drawData.rdpTiles.clear();
        drawData.lerpRdpTiles.clear();
        drawData.gpuTiles.clear();
        drawData.callTiles.clear();
        drawData.rspViewports.clear();
        drawData.viewportOrigins.clear();
        drawData.rspFog.clear();
        drawData.rspLights.clear();
        drawData.rspLookAt.clear();
        drawData.loadOperations.clear();
        drawData.worldTransforms.clear();
        drawData.viewTransforms.clear();
        drawData.projTransforms.clear();
        drawData.viewProjTransforms.clear();
        drawData.modViewTransforms.clear();
        drawData.modProjTransforms.clear();
        drawData.modViewProjTransforms.clear();
        drawData.prevViewTransforms.clear();
        drawData.prevProjTransforms.clear();
        drawData.prevViewProjTransforms.clear();
        drawData.lerpWorldTransforms.clear();
        drawData.prevWorldTransforms.clear();
        drawData.invTWorldTransforms.clear();
        drawData.triPosFloats.clear();
        drawData.triTcFloats.clear();
        drawData.triColorFloats.clear();
        drawData.transformGroups.clear();
        drawData.worldTransformGroups.clear();
        drawData.viewProjTransformGroups.clear();
        drawData.worldTransformSegmentedAddresses.clear();
        drawData.worldTransformPhysicalAddresses.clear();
        drawData.worldTransformVertexIndices.clear();

        // Push an identity matrix into the transforms by default so rects can use them.
        drawData.rspViewports.push_back(interop::RSPViewport::identity());
        drawData.viewTransforms.push_back(interop::float4x4::identity());
        drawData.projTransforms.push_back(interop::float4x4::identity());
        drawData.viewProjTransforms.push_back(interop::float4x4::identity());
        drawData.worldTransforms.push_back(interop::float4x4::identity());
        drawData.transformGroups.push_back(TransformGroup());
        drawData.worldTransformGroups.push_back(0);
        drawData.viewProjTransformGroups.push_back(0);
        drawData.worldTransformSegmentedAddresses.push_back(0);
        drawData.worldTransformPhysicalAddresses.push_back(0);
        drawData.worldTransformVertexIndices.push_back(0);
        drawData.viewportOrigins.push_back(0);
    }

    void Workload::resetDrawDataRanges() {
        auto &r = drawRanges;
        r.posShorts = { 0, 0 };
        r.velShorts = { 0, 0 };
        r.tcFloats = { 0, 0 };
        r.normColBytes = { 0, 0 };
        r.viewProjIndices = { 0, 0 };
        r.worldIndices = { 0, 0 };
        r.fogIndices = { 0, 0 };
        r.lightIndices = { 0, 0 };
        r.lightCounts = { 0, 0 };
        r.lookAtIndices = { 0, 0 };
        r.faceIndices = { 0, 0 };
        r.modifyPosUints = { 0, 0 };
        r.rdpParams = { 0, 0 };
        r.extraParams = { 0, 0 };
        r.renderParams = { 0, 0 };
        r.viewProjTransforms = { 0, 0 };
        r.worldTransforms = { 0, 0 };
        r.rdpTiles = { 0, 0 };
        r.gpuTiles = { 0, 0 };
        r.callTiles = { 0, 0 };
        r.rspViewports = { 0, 0 };
        r.rspFog = { 0, 0 };
        r.rspLights = { 0, 0 };
        r.rspLookAt = { 0, 0 };
        r.loadOperations = { 0, 0 };
        r.triPosFloats = { 0, 0 };
        r.triTcFloats = { 0, 0 };
        r.triColorFloats = { 0, 0 };
    }

    void Workload::resetRSPOutputBuffers() {
        outputBuffers.screenPosBuffer.computedSize = 0;
        outputBuffers.genTexCoordBuffer.computedSize = 0;
        outputBuffers.shadedColBuffer.computedSize = 0;
    }

    void Workload::resetWorldOutputBuffers() {
        outputBuffers.worldPosBuffer.computedSize = 0;
        outputBuffers.worldNormBuffer.computedSize = 0;
        outputBuffers.worldVelBuffer.computedSize = 0;
    }

    void Workload::updateDrawDataRanges() {
        auto &r = drawRanges;
        r.posShorts.second = drawData.posShorts.size();
        r.velShorts.second = drawData.velShorts.size();
        r.tcFloats.second = drawData.tcFloats.size();
        r.normColBytes.second = drawData.normColBytes.size();
        r.viewProjIndices.second = drawData.viewProjIndices.size();
        r.worldIndices.second = drawData.worldIndices.size();
        r.fogIndices.second = drawData.fogIndices.size();
        r.lightIndices.second = drawData.lightIndices.size();
        r.lightCounts.second = drawData.lightCounts.size();
        r.lookAtIndices.second = drawData.lookAtIndices.size();
        r.faceIndices.second = drawData.faceIndices.size();
        r.modifyPosUints.second = drawData.modifyPosUints.size();
        r.rdpParams.second = drawData.rdpParams.size();
        r.extraParams.second = drawData.extraParams.size();
        r.renderParams.second = drawData.renderParams.size();
        r.viewProjTransforms.second = drawData.viewProjTransforms.size();
        r.worldTransforms.second = drawData.worldTransforms.size();
        r.rdpTiles.second = drawData.rdpTiles.size();
        r.gpuTiles.second = drawData.gpuTiles.size();
        r.callTiles.second = drawData.callTiles.size();
        r.rspViewports.second = drawData.rspViewports.size();
        r.rspFog.second = drawData.rspFog.size();
        r.rspLights.second = drawData.rspLights.size();
        r.rspLookAt.second = drawData.rspLookAt.size();
        r.loadOperations.second = drawData.loadOperations.size();
        r.triPosFloats.second = drawData.triPosFloats.size();
        r.triTcFloats.second = drawData.triTcFloats.size();
        r.triColorFloats.second = drawData.triColorFloats.size();
    }
    
    void Workload::uploadDrawData(RenderWorker *worker, BufferUploader *bufferUploader) {
        const RenderBufferFlags rtInputFlag = worker->device->getCapabilities().raytracing ? RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT : RenderBufferFlag::NONE;
        bufferUploader->submit(worker, {
            { drawData.posShorts.data(), drawRanges.posShorts, sizeof(int16_t), RenderBufferFlag::FORMATTED, { RenderFormat::R16_SINT }, &drawBuffers.positionBuffer },
            { drawData.velShorts.data(), drawRanges.velShorts, sizeof(int16_t), RenderBufferFlag::FORMATTED, { RenderFormat::R16_SINT }, &drawBuffers.velocityBuffer },
            { drawData.tcFloats.data(), drawRanges.tcFloats, sizeof(float), RenderBufferFlag::FORMATTED | RenderBufferFlag::VERTEX, { RenderFormat::R32_FLOAT }, &drawBuffers.texcoordBuffer },
            { drawData.normColBytes.data(), drawRanges.normColBytes, sizeof(uint8_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R8_UINT, RenderFormat::R8_SINT }, &drawBuffers.normalColorBuffer },
            { drawData.viewProjIndices.data(), drawRanges.viewProjIndices, sizeof(uint16_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R16_UINT }, &drawBuffers.viewProjIndicesBuffer },
            { drawData.worldIndices.data(), drawRanges.worldIndices, sizeof(uint16_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R16_UINT }, &drawBuffers.worldIndicesBuffer },
            { drawData.fogIndices.data(), drawRanges.fogIndices, sizeof(uint16_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R16_UINT }, &drawBuffers.fogIndicesBuffer },
            { drawData.lightIndices.data(), drawRanges.lightIndices, sizeof(uint16_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R16_UINT }, &drawBuffers.lightIndicesBuffer },
            { drawData.lightCounts.data(), drawRanges.lightCounts, sizeof(uint8_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R8_UINT }, &drawBuffers.lightCountsBuffer },
            { drawData.lookAtIndices.data(), drawRanges.lookAtIndices, sizeof(uint16_t), RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE, { RenderFormat::R16_UINT }, &drawBuffers.lookAtIndicesBuffer },
            { drawData.faceIndices.data(), drawRanges.faceIndices, sizeof(uint32_t), RenderBufferFlag::INDEX | RenderBufferFlag::STORAGE | rtInputFlag, { }, &drawBuffers.faceIndicesBuffer },
            { drawData.modifyPosUints.data(), drawRanges.modifyPosUints, sizeof(uint32_t), RenderBufferFlag::FORMATTED, { RenderFormat::R32_UINT }, &drawBuffers.modifyPosUintsBuffer },
            { drawData.rdpParams.data(), drawRanges.rdpParams, sizeof(interop::RDPParams), RenderBufferFlag::STORAGE, { }, &drawBuffers.rdpParamsBuffer },
            { drawData.renderParams.data(), drawRanges.renderParams, sizeof(interop::RenderParams), RenderBufferFlag::STORAGE, { }, &drawBuffers.renderParamsBuffer },
            { drawData.rdpTiles.data(), drawRanges.rdpTiles, sizeof(interop::RDPTile), RenderBufferFlag::STORAGE, {}, &drawBuffers.rdpTilesBuffer },
            { drawData.rspViewports.data(), drawRanges.rspViewports, sizeof(interop::RSPViewport), RenderBufferFlag::STORAGE, { }, &drawBuffers.rspViewportsBuffer },
            { drawData.rspFog.data(), drawRanges.rspFog, sizeof(interop::RSPFog), RenderBufferFlag::STORAGE, { }, &drawBuffers.rspFogBuffer },
            { drawData.rspLights.data(), drawRanges.rspLights, sizeof(interop::RSPLight), RenderBufferFlag::STORAGE, { }, &drawBuffers.rspLightsBuffer },
            { drawData.rspLookAt.data(), drawRanges.rspLookAt, sizeof(interop::RSPLookAt), RenderBufferFlag::STORAGE, { }, &drawBuffers.rspLookAtBuffer },
            { drawData.triPosFloats.data(), drawRanges.triPosFloats, sizeof(float), RenderBufferFlag::VERTEX, { }, &drawBuffers.triPosBuffer },
            { drawData.triTcFloats.data(), drawRanges.triTcFloats, sizeof(float), RenderBufferFlag::VERTEX, { }, &drawBuffers.triTcBuffer },
            { drawData.triColorFloats.data(), drawRanges.triColorFloats, sizeof(float), RenderBufferFlag::VERTEX, { }, &drawBuffers.triColorBuffer }
        });
    }

    void updateOutputBuffer(RenderWorker *worker, ComputedBuffer &computedBuffer, uint64_t requiredSize, RenderBufferFlags flags = RenderBufferFlag::NONE) {
        if (computedBuffer.allocatedSize >= requiredSize) {
            return;
        }

        // Recreate the buffer.
        computedBuffer.allocatedSize = (requiredSize * 3) / 2;
        computedBuffer.allocatedSize = roundUp(computedBuffer.allocatedSize, 256);
        computedBuffer.buffer = worker->device->createBuffer(RenderBufferDesc::DefaultBuffer(computedBuffer.allocatedSize, flags | RenderBufferFlag::STORAGE | RenderBufferFlag::UNORDERED_ACCESS));

        // Set the computed size to 0 if it was recreated.
        computedBuffer.computedSize = 0;
    }

    void Workload::updateOutputBuffers(RenderWorker *worker) {
        const uint32_t vertexCount = drawData.vertexCount();
        const RenderBufferFlags rtInputFlag = worker->device->getCapabilities().raytracing ? RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT : RenderBufferFlag::NONE;
        updateOutputBuffer(worker, outputBuffers.screenPosBuffer, vertexCount * sizeof(float) * 4, RenderBufferFlag::VERTEX);
        updateOutputBuffer(worker, outputBuffers.genTexCoordBuffer, vertexCount * sizeof(float) * 2, RenderBufferFlag::VERTEX);
        updateOutputBuffer(worker, outputBuffers.shadedColBuffer, vertexCount * sizeof(float) * 4, RenderBufferFlag::VERTEX);
        updateOutputBuffer(worker, outputBuffers.worldPosBuffer, vertexCount * sizeof(float) * 4, rtInputFlag);
        updateOutputBuffer(worker, outputBuffers.worldNormBuffer, vertexCount * sizeof(float) * 4);
        updateOutputBuffer(worker, outputBuffers.worldVelBuffer, vertexCount * sizeof(float) * 4);
        updateOutputBuffer(worker, outputBuffers.testZIndexBuffer, extended.testZIndexCount * sizeof(uint32_t), RenderBufferFlag::INDEX | RenderBufferFlag::STORAGE);
    }

    void nextDrawDataRange(DrawRanges::Range &range) {
        range.first = range.second;
    }

    void Workload::nextDrawDataRanges() {
        auto &r = drawRanges;
        nextDrawDataRange(r.posShorts);
        nextDrawDataRange(r.velShorts);
        nextDrawDataRange(r.tcFloats);
        nextDrawDataRange(r.normColBytes);
        nextDrawDataRange(r.viewProjIndices);
        nextDrawDataRange(r.worldIndices);
        nextDrawDataRange(r.fogIndices);
        nextDrawDataRange(r.lightIndices);
        nextDrawDataRange(r.lightCounts);
        nextDrawDataRange(r.lookAtIndices);
        nextDrawDataRange(r.faceIndices);
        nextDrawDataRange(r.modifyPosUints);
        nextDrawDataRange(r.rdpParams);
        nextDrawDataRange(r.extraParams);
        nextDrawDataRange(r.renderParams);
        nextDrawDataRange(r.viewProjTransforms);
        nextDrawDataRange(r.worldTransforms);
        nextDrawDataRange(r.rdpTiles);
        nextDrawDataRange(r.gpuTiles);
        nextDrawDataRange(r.callTiles);
        nextDrawDataRange(r.rspViewports);
        nextDrawDataRange(r.rspFog);
        nextDrawDataRange(r.rspLights);
        nextDrawDataRange(r.rspLookAt);
        nextDrawDataRange(r.loadOperations);
        nextDrawDataRange(r.triPosFloats);
        nextDrawDataRange(r.triTcFloats);
        nextDrawDataRange(r.triColorFloats);
    }

    void Workload::begin(uint64_t submissionFrame) {
        reset();

        this->submissionFrame = submissionFrame;
    }

    bool Workload::addFramebufferPair(uint32_t colorAddress, uint8_t colorFmt, uint8_t colorSiz, uint16_t colorWidth, uint32_t depthAddress) {
        uint32_t fbPairIndex;
        bool addedPair = false;
        if ((fbPairCount == 0) || !fbPairs[fbPairCount - 1].isEmpty()) {
            fbPairIndex = fbPairCount++;
            adjustVector(fbPairs, fbPairCount);
            addedPair = true;
        }
        else {
            fbPairIndex = fbPairCount - 1;
            addedPair = false;
        }

        auto &fbPair = fbPairs[fbPairIndex];
        fbPair.reset();
        fbPair.colorImage.address = colorAddress;
        fbPair.colorImage.fmt = colorFmt;
        fbPair.colorImage.siz = colorSiz;
        fbPair.colorImage.width = colorWidth;
        fbPair.depthImage.address = depthAddress;

        return addedPair;
    }

    int Workload::currentFramebufferPairIndex() const {
        if (fbPairCount > 0) {
            return fbPairCount - 1;
        }
        else {
            return 0;
        }
    }
};