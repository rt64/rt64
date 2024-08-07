//
// RT64
//

#pragma once

#include "render/rt64_buffer_uploader.h"
#include "shared/rt64_extra_params.h"
#include "shared/rt64_gpu_tile.h"
#include "shared/rt64_rdp_params.h"
#include "shared/rt64_render_params.h"
#include "shared/rt64_rsp_fog.h"
#include "shared/rt64_rsp_light.h"
#include "shared/rt64_rsp_lookat.h"
#include "shared/rt64_rsp_viewport.h"

#include "rt64_command_warning.h"
#include "rt64_draw_call.h"
#include "rt64_framebuffer_changes.h"
#include "rt64_framebuffer_manager.h"
#include "rt64_framebuffer_pair.h"
#include "rt64_framebuffer_storage.h"
#include "rt64_transform_group.h"

namespace RT64 {
    struct DrawData {
        std::vector<int16_t> posShorts;
        std::vector<int16_t> velShorts;
        std::vector<float> tcFloats;
        std::vector<uint8_t> normColBytes;
        std::vector<uint16_t> viewProjIndices;
        std::vector<uint16_t> worldIndices;
        std::vector<uint16_t> fogIndices;
        std::vector<uint16_t> lightIndices;
        std::vector<uint8_t> lightCounts;
        std::vector<uint16_t> lookAtIndices;
        std::vector<uint32_t> faceIndices;
        std::vector<uint32_t> modifyPosUints;
        std::vector<hlslpp::float4> posTransformed;
        std::vector<hlslpp::float3> posScreen;
        std::vector<interop::RDPParams> rdpParams;
        std::vector<interop::ExtraParams> extraParams;
        std::vector<interop::RenderParams> renderParams;
        std::vector<interop::float4x4> viewTransforms;
        std::vector<interop::float4x4> projTransforms;
        std::vector<interop::float4x4> viewProjTransforms;
        std::vector<interop::float4x4> modViewTransforms;
        std::vector<interop::float4x4> modProjTransforms;
        std::vector<interop::float4x4> modViewProjTransforms;
        std::vector<interop::float4x4> prevViewTransforms;
        std::vector<interop::float4x4> prevProjTransforms;
        std::vector<interop::float4x4> prevViewProjTransforms;
        std::vector<interop::float4x4> worldTransforms;
        std::vector<interop::float4x4> prevWorldTransforms;
        std::vector<interop::float4x4> invTWorldTransforms;
        std::vector<interop::float4x4> lerpWorldTransforms;
        std::vector<interop::RDPTile> rdpTiles;
        std::vector<interop::RDPTile> lerpRdpTiles;
        std::vector<interop::GPUTile> gpuTiles;
        std::vector<DrawCallTile> callTiles;
        std::vector<interop::RSPViewport> rspViewports;
        std::vector<uint16_t> viewportOrigins;
        std::vector<interop::RSPFog> rspFog;
        std::vector<interop::RSPLight> rspLights;
        std::vector<interop::RSPLookAt> rspLookAt;
        std::vector<LoadOperation> loadOperations;
        std::vector<float> triPosFloats;
        std::vector<float> triTcFloats;
        std::vector<float> triColorFloats;
        std::vector<TransformGroup> transformGroups;
        std::vector<uint32_t> worldTransformGroups;
        std::vector<uint32_t> viewProjTransformGroups;
        std::vector<uint32_t> worldTransformSegmentedAddresses;
        std::vector<uint32_t> worldTransformPhysicalAddresses;
        std::vector<uint32_t> worldTransformVertexIndices;

        uint32_t vertexCount() const {
            return uint32_t(worldIndices.size());
        }

        uint32_t modifyCount() const {
            return uint32_t(modifyPosUints.size()) / 2;
        }

        uint32_t rawTriVertexCount() const {
            return uint32_t(triPosFloats.size()) / 4;
        }

        uint32_t worldTransformVertexCount(uint32_t i) const {
            if (i < (worldTransformVertexIndices.size() - 1)) {
                return worldTransformVertexIndices[i + 1] - worldTransformVertexIndices[i];
            }
            else {
                return vertexCount() - worldTransformVertexIndices[i];
            }
        }
    };

    struct DrawRanges {
        typedef std::pair<size_t, size_t> Range;

        Range posShorts;
        Range velShorts;
        Range tcFloats;
        Range normColBytes;
        Range viewProjIndices;
        Range worldIndices;
        Range fogIndices;
        Range lightIndices;
        Range lightCounts;
        Range lookAtIndices;
        Range faceIndices;
        Range modifyPosUints;
        Range rdpParams;
        Range extraParams;
        Range renderParams;
        Range viewProjTransforms;
        Range worldTransforms;
        Range rdpTiles;
        Range gpuTiles;
        Range callTiles;
        Range rspViewports;
        Range rspFog;
        Range rspLights;
        Range rspLookAt;
        Range loadOperations;
        Range triPosFloats;
        Range triTcFloats;
        Range triColorFloats;
    };

    struct DrawBuffers {
        BufferPair positionBuffer;
        BufferPair velocityBuffer;
        BufferPair texcoordBuffer;
        BufferPair normalColorBuffer;
        BufferPair viewProjIndicesBuffer;
        BufferPair worldIndicesBuffer;
        BufferPair fogIndicesBuffer;
        BufferPair lightIndicesBuffer;
        BufferPair lightCountsBuffer;
        BufferPair lookAtIndicesBuffer;
        BufferPair faceIndicesBuffer;
        BufferPair modifyPosUintsBuffer;
        BufferPair rdpParamsBuffer;
        BufferPair rspParamsBuffer;
        BufferPair extraParamsBuffer;
        BufferPair renderParamsBuffer;
        BufferPair rdpTilesBuffer;
        BufferPair gpuTilesBuffer;
        BufferPair rspViewportsBuffer;
        BufferPair rspFogBuffer;
        BufferPair rspLightsBuffer;
        BufferPair rspLookAtBuffer;
        BufferPair worldTransformsBuffer;
        BufferPair viewProjTransformsBuffer;
        BufferPair prevWorldTransformsBuffer;
        BufferPair invTWorldTransformsBuffer;
        BufferPair triPosBuffer;
        BufferPair triTcBuffer;
        BufferPair triColorBuffer;
    };

    struct ComputedBuffer {
        std::unique_ptr<RenderBuffer> buffer;
        uint64_t allocatedSize = 0;
        uint64_t computedSize = 0;
    };

    struct OutputBuffers {
        ComputedBuffer screenPosBuffer;
        ComputedBuffer genTexCoordBuffer;
        ComputedBuffer shadedColBuffer;
        ComputedBuffer worldPosBuffer;
        ComputedBuffer worldNormBuffer;
        ComputedBuffer worldVelBuffer;
        ComputedBuffer testZIndexBuffer;
    };

    struct DebuggerRenderer {
        bool framebufferDepth;
        int32_t framebufferIndex;
        uint32_t framebufferAddress;
        int32_t globalDrawCallIndex;
        float interpolationWeight;
    };

    struct DebuggerCamera {
        bool enabled;
        uint32_t sceneIndex;
        hlslpp::float4x4 viewMatrix;
        hlslpp::float4x4 invViewMatrix;
        hlslpp::float4x4 projMatrix;
        float nearPlane;
        float farPlane;
        float fov;
    };

    struct Workload {
        uint64_t submissionFrame;
        DrawData drawData;
        DrawRanges drawRanges;
        DrawBuffers drawBuffers;
        OutputBuffers outputBuffers;
        std::vector<FramebufferPair> fbPairs;
        std::vector<CommandWarning> commandWarnings;
        std::vector<interop::PointLight> pointLights;
        uint32_t fbPairCount;
        uint32_t fbPairSubmitted;
        uint32_t gameCallCount;
        FramebufferChangePool fbChangePool;
        FramebufferStorage fbStorage;
        uint32_t viOriginalRate;
        DebuggerRenderer debuggerRenderer;
        DebuggerCamera debuggerCamera;
        std::multimap<uint32_t, uint32_t> transformIdMap;
        std::multimap<uint32_t, uint32_t> physicalAddressTransformMap;
        std::vector<uint32_t> transformIgnoredIds;
        uint64_t workloadId = 0;
        uint64_t presentId = 0;
        bool paused = false;

        struct {
            uint32_t testZIndexCount = 0;
            float ditherNoiseStrength = 1.0f;
        } extended;

        void reset();
        void resetDrawData();
        void resetDrawDataRanges();
        void resetRSPOutputBuffers();
        void resetWorldOutputBuffers();
        void updateDrawDataRanges();
        void uploadDrawData(RenderWorker *worker, BufferUploader *bufferUploader);
        void updateOutputBuffers(RenderWorker *worker);
        void nextDrawDataRanges();
        void begin(uint64_t submissionFrame);
        bool addFramebufferPair(uint32_t colorAddress, uint8_t colorFmt, uint8_t colorSiz, uint16_t colorWidth, uint32_t depthAddress);
        int currentFramebufferPairIndex() const;
    };
};