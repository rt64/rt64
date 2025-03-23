//
// RT64
//

#pragma once

#include <stdint.h>

#include "common/rt64_emulator_configuration.h"
#include "common/rt64_user_configuration.h"
#include "hle/rt64_framebuffer_manager.h"
#include "hle/rt64_workload.h"
#include "preset/rt64_preset_scene.h"
#include "shared/rt64_frame_params.h"
#include "shared/rt64_gpu_tile.h"
#include "shared/rt64_render_indices.h"
#include "shared/rt64_render_params.h"

#include "rt64_buffer_uploader.h"
#include "rt64_descriptor_sets.h"
#include "rt64_framebuffer_renderer_call.h"
#include "rt64_raster_shader_cache.h"
#include "rt64_render_target.h"
#include "rt64_rsp_processor.h"
#include "rt64_vertex_processor.h"

#if RT_ENABLED
#   include "rt64_raytracing_resources.h"
#   include "rt64_raytracing_shader_cache.h"
#endif

namespace RT64 {
    struct DynamicTextureView {
        const RenderTexture *texture = nullptr;
        uint32_t dstIndex = 0;
        const RenderTextureView *textureView = nullptr;
    };

    struct RasterScene {
        std::vector<uint32_t> instanceIndices;

        RasterScene();
    };

    struct RenderTargetDrawCall {
        typedef std::pair<uint32_t, bool> SceneIndexPair;

        RenderFramebufferStorage *fbStorage = nullptr;
        std::vector<RasterScene> rasterScenes;
        std::vector<SceneIndexPair> sceneIndices;
#   if RT_ENABLED
        std::vector<RaytracingScene> rtScenes;
#   endif
    };

    struct RSPSmoothNormalGenerationCB {
        uint32_t indexStart;
        uint32_t indexCount;
    };

    struct FramebufferRenderer {
        std::vector<uint32_t> textureCacheVersions;
        std::vector<Texture *> textureCacheTextures;
        std::vector<Texture *> textureCacheTextureReplacements;
        std::vector<uint32_t> textureCacheFreeSpaces;
        uint32_t textureCacheSize = 0;
        uint32_t textureCacheGlobalVersion = 0;
        bool textureCacheReplacementMapEnabled = false;
        std::vector<InstanceDrawCall> instanceDrawCallVector;
        std::vector<RenderPipelineProgram> hitGroupVector;
        std::vector<interop::RenderIndices> renderIndicesVector;
        std::vector<DynamicTextureView> dynamicTextureViewVector;
        std::vector<RenderTextureBarrier> dynamicTextureBarrierVector;
        std::unique_ptr<BufferUploader> shaderUploader;
        std::vector<RSPSmoothNormalGenerationCB> rspSmoothNormalVector;
        std::array<RenderInputSlot, 3> vertexInputSlots;
        std::array<RenderVertexBufferView, 3> indexedVertexViews;
        std::array<RenderVertexBufferView, 3> rawVertexViews;
        RenderIndexBufferView indexBufferView;
        RenderBuffer *testZIndexBuffer = nullptr;
        RenderIndexBufferView testZIndexBufferView;
        BufferPair renderIndicesBuffer;
        BufferPair interleavedRastersBuffer;
        uint32_t interleavedRastersCount = 0;
        BufferPair frameParamsBuffer;
        RenderPipelineLayout *rendererPipelineLayout = nullptr;
        RenderPipeline *postBlendDitherNoiseAddPipeline = nullptr;
        RenderPipeline *postBlendDitherNoiseSubPipeline = nullptr;
        RenderPipeline *postBlendDitherNoiseSubNegativePipeline = nullptr;
        std::unique_ptr<FramebufferRendererDescriptorCommonSet> descCommonSet;
        std::unique_ptr<FramebufferRendererDescriptorTextureSet> descTextureSet;
        std::unique_ptr<RenderTexture> dummyColorTarget;
        std::unique_ptr<RenderTexture> dummyDepthTarget;
        std::unique_ptr<RenderTextureView> dummyColorTargetView;
        std::unique_ptr<RenderTextureView> dummyDepthTargetView;
        bool dummyColorTargetTransitioned = false;
        bool dummyDepthTargetTransitioned = false;
        std::vector<uint32_t> descriptorTextureVersions;
        uint32_t descriptorTextureGlobalVersion = 0;
        bool descriptorTextureReplacementMapEnabled = false;
        std::unique_ptr<RSPSmoothNormalDescriptorSet> smoothDescSet;
        std::unique_ptr<RSPVertexTestZDescriptorSet> vertexTestZSet;
        interop::FrameParams frameParams;
        const ShaderLibrary *shaderLibrary = nullptr;

#   if RT_ENABLED
        const RenderTexture *blueNoiseTexture = nullptr;
        const RaytracingState *rtState = nullptr;
        const RenderPipelineLayout *rtPipelineLayout = nullptr;
        std::unique_ptr<RaytracingResources> rtResources;
        bool rtSupport = false;
#   endif

        struct Framebuffer {
            std::unique_ptr<RenderBuffer> paramsBuffer;
            std::unique_ptr<FramebufferRendererDescriptorFramebufferSet> descRealFbSet;
            std::unique_ptr<FramebufferRendererDescriptorFramebufferSet> descDummyFbSet;
            std::set<RenderTarget *> transitionRenderTargetSet;
            RenderTargetDrawCall renderTargetDrawCall;
        };

        std::vector<Framebuffer> framebufferVector;
        std::vector<BufferUploader *> pendingUploaders;
        uint32_t framebufferCount = 0;

        struct DrawParams {
            RenderWorker *worker;
            RenderFramebufferStorage *fbStorage;
            const Workload *curWorkload;
            uint32_t fbPairIndex;
            uint32_t fbWidth;
            uint32_t fbHeight;
            uint32_t targetWidth;
            uint32_t targetHeight;
            RasterShaderCache *rasterShaderCache;
            hlslpp::float2 resolutionScale;
            float aspectRatioSource;
            float aspectRatioTarget;
            float extAspectPercentage;
            float horizontalMisalignment;
            PresetScene presetScene;
            bool rtEnabled;
            uint64_t submissionFrame;
            float deltaTimeMs;
            bool ubershadersOnly;
            bool fixRectLR;
            bool postBlendNoise;
            bool postBlendNoiseNegative;
            uint32_t maxGameCall;
        };

        FramebufferRenderer(RenderWorker *worker, bool rtSupport, UserConfiguration::GraphicsAPI graphicsAPI, const ShaderLibrary *shaderLibrary);
        ~FramebufferRenderer();
        void resetFramebuffers(RenderWorker *worker, bool ubershadersVisible, float ditherNoiseStrength, const RenderMultisampling &multisampling);
        void updateTextureCache(TextureCache *textureCache);
        void createGPUTiles(const DrawCallTile *callTiles, uint32_t callTileCount, interop::GPUTile *dstGPUTiles, const FramebufferManager *fbManager, TextureCache *textureCache, uint64_t submissionFrame);
        uint32_t getDestinationIndex();
        uint32_t getTextureIndex(RenderTarget *renderTarget);
        uint32_t getTextureIndex(const FramebufferManager::TileCopy &tileCopy);
        void updateMultisampling();
        void updateShaderDescriptorSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, bool raytracingEnabled);
        void updateRSPSmoothNormalSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers);
        void updateRSPVertexTestZSet(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers);
        void updateShaderViews(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, bool raytracingEnabled);
        void submitRSPSmoothNormalCompute(RenderWorker *worker, const OutputBuffers *outputBuffers);
        bool submitDepthAccess(RenderWorker *worker, RenderFramebufferStorage *fbStorage, bool readOnly, bool &depthState);
        void submitRasterScene(RenderWorker *worker, const Framebuffer &framebuffer, RenderFramebufferStorage *fbStorage, const RasterScene &rasterScene, bool &depthState);
        void addFramebuffer(const DrawParams &p);
        void endFramebuffers(RenderWorker *worker, const DrawBuffers *drawBuffers, const OutputBuffers *outputBuffers, bool rtEnabled);
        void recordSetup(RenderWorker *worker, std::vector<BufferUploader *> bufferUploaders, RSPProcessor *rspProcessor, VertexProcessor *vertexProcessor, const OutputBuffers *outputBuffers, bool rtEnabled);
        void recordFramebuffer(RenderWorker *worker, uint32_t framebufferIndex);
        void waitForUploaders();
        void advanceFrame(bool rtEnabled);

#   if RT_ENABLED
        void resetRaytracing(RaytracingShaderCache *rtShaderCache, const RenderTexture *blueNoiseTexture);
        void updateRaytracingScene(RenderWorker *worker, const RaytracingScene &rtScene);
        void submitRaytracingScene(RenderWorker *worker, RenderTarget *colorTarget, const RaytracingScene &rtScene);
        void setRaytracingConfig(const RaytracingConfiguration &rtConfig, bool resolutionChanged);
#   endif
    };
};