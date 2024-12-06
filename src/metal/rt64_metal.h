#pragma once

#include "rhi/rt64_render_interface.h"

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace RT64 {
    struct MetalInterface;
    struct MetalDevice;
    struct MetalCommandQueue;
    struct MetalTexture;
    struct MetalPipelineLayout;
    struct MetalGraphicsPipeline;
    struct MetalPool;

    enum class EncoderType {
        ClearColor,
        ClearDepth,
        Render,
        Compute,
        Blit,
        Resolve
    };

    static struct {
        MTL::ComputePipelineState *resolveTexturePipelineState;
        MTL::Library *clearColorShaderLibrary;
        MTL::Library *clearDepthShaderLibrary;
    } MetalContext;

    struct MetalDescriptorSetLayout {
        std::vector<MTL::SamplerState *> staticSamplers;
        std::vector<MTL::ArgumentDescriptor *> argumentDescriptors;
        MTL::ArgumentEncoder *argumentEncoder = nullptr;
        MTL::Buffer *descriptorBuffer = nullptr;
        std::vector<RenderDescriptorRangeType> descriptorTypes;
        std::vector<uint32_t> descriptorToRangeIndex;
        std::vector<uint32_t> descriptorIndexBases;
        std::vector<uint32_t> descriptorRangeBinding;
        std::vector<uint32_t> samplerIndices;
        uint32_t bufferOffset = 0;
        uint32_t entryCount = 0;
        uint32_t descriptorTypeMaxIndex = 0;
        uint32_t initialIndexOffset = 0;

        MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        ~MetalDescriptorSetLayout();
    };

    struct MetalComputeState {
        MTL::ComputePipelineState *pipelineState = nullptr;
        uint32_t threadGroupSizeX = 0;
        uint32_t threadGroupSizeY = 0;
        uint32_t threadGroupSizeZ = 0;
    };

    struct MetalRenderState {
        MTL::RenderPipelineState *renderPipelineState = nullptr;
        MTL::DepthStencilState *depthStencilState = nullptr;
        MTL::CullMode cullMode = MTL::CullModeNone;
        MTL::DepthClipMode depthClipMode = MTL::DepthClipModeClip;
        MTL::Winding winding = MTL::WindingClockwise;
        MTL::SamplePosition *samplePositions = nullptr;
        uint32_t sampleCount = 0;
    };

    struct MetalDescriptorSet : RenderDescriptorSet {
        MTL::Buffer *descriptorBuffer;
        std::vector<MTL::Buffer *> residentBuffers;
        std::unordered_map<uint32_t, MTL::Texture *> indicesToTextures;
        std::unordered_map<uint32_t, MTL::Buffer *> indicesToBuffers;
        std::unordered_map<uint32_t, MTL::SamplerState *> indicesToSamplers;
        std::vector<MTL::SamplerState *> staticSamplers;
        std::vector<MTL::ArgumentDescriptor *> argumentDescriptors;
        uint32_t bufferOffset = 0;
        uint32_t entryCount = 0;
        uint32_t descriptorTypeMaxIndex = 0;
        std::vector<RenderDescriptorRangeType> descriptorTypes;
        std::vector<uint32_t> samplerIndices;

        MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        MetalDescriptorSet(MetalDevice *device, uint32_t entryCount);
        ~MetalDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        virtual void setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
    };

    struct MetalSwapChain : RenderSwapChain {
        CA::MetalDrawable *drawable = nullptr;
        CA::MetalLayer *layer = nullptr;
        MetalCommandQueue *commandQueue = nullptr;
        RenderWindow renderWindow = {};
        uint32_t textureCount = 0;
        MetalTexture *proxyTexture;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;

        MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format);
        ~MetalSwapChain() override;
        bool present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) override;
        bool resize() override;
        bool needsResize() const override;
        virtual void setVsyncEnabled(bool vsyncEnabled) override;
        virtual bool isVsyncEnabled() const override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
        RenderTexture *getTexture(uint32_t textureIndex) override;
        uint32_t getTextureCount() const override;
        bool acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) override;
        RenderWindow getWindow() const override;
        bool isEmpty() const override;
        uint32_t getRefreshRate() const override;
        void getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const;
    };

    struct MetalFramebuffer : RenderFramebuffer {
        bool depthAttachmentReadOnly = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<const MetalTexture *> colorAttachments;
        const MetalTexture *depthAttachment = nullptr;

        MetalFramebuffer(MetalDevice *device, const RenderFramebufferDesc &desc);
        ~MetalFramebuffer() override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
    };

    struct MetalCommandList : RenderCommandList {
        MTL::ComputeCommandEncoder *activeComputeEncoder = nullptr;
        MTL::RenderCommandEncoder *activeRenderEncoder = nullptr;
        MTL::RenderCommandEncoder *activeClearColorRenderEncoder = nullptr;
        MTL::RenderCommandEncoder *activeClearDepthRenderEncoder = nullptr;
        MTL::BlitCommandEncoder *activeBlitEncoder = nullptr;
        MTL::ComputeCommandEncoder *activeResolveComputeEncoder = nullptr;
    
        MTL::ComputeCommandEncoder *computeEncoder = nullptr;

        MTL::PrimitiveType currentPrimitiveType = MTL::PrimitiveTypeTriangle;
        MTL::IndexType currentIndexType = MTL::IndexTypeUInt32;
        MTL::Buffer *indexBuffer = nullptr;

        uint32_t viewCount = 0;
        std::vector<MTL::Buffer *> vertexBuffers;
        std::vector<uint32_t> vertexBufferOffsets;
        std::vector<uint32_t> vertexBufferIndices;

        std::vector<MTL::Viewport> viewportVector;
        std::vector<MTL::ScissorRect> scissorVector;

        MTL::Buffer *graphicsPushConstantsBuffer = nullptr;
        MTL::Buffer *computePushConstantsBuffer = nullptr;
        
        MetalDevice *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;
        const MetalCommandQueue *queue = nullptr;
        const MetalFramebuffer *targetFramebuffer = nullptr;
        const MetalPipelineLayout *activeComputePipelineLayout = nullptr;
        const MetalPipelineLayout *activeGraphicsPipelineLayout = nullptr;
        const MetalGraphicsPipeline *activeGraphicsPipeline = nullptr;
        const MetalRenderState *activeRenderState = nullptr;
        const MetalComputeState *activeComputeState = nullptr;

        std::unordered_map<uint32_t, MetalDescriptorSet *> indicesToRenderDescriptorSets;
        std::unordered_map<uint32_t, MetalDescriptorSet *> indicesToComputeDescriptorSets;

        MetalCommandList(MetalCommandQueue *queue, RenderCommandListType type);
        ~MetalCommandList() override;
        void begin() override;
        void end() override;
        void endEncoder(bool clearDescs);
        void guaranteeRenderDescriptor(bool forClearColor);
        void guaranteeComputeEncoder();
        void clearDrawCalls();
        void barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) override;
        void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;
        void traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) override;
        void drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) override;
        void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
        void setPipeline(const RenderPipeline *pipeline) override;
        void setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setComputePushConstants(uint32_t rangeIndex, const void *data) override;
        void setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setGraphicsPushConstants(uint32_t rangeIndex, const void *data) override;
        void setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setRaytracingPushConstants(uint32_t rangeIndex, const void *data) override;
        void setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setIndexBuffer(const RenderIndexBufferView *view) override;
        void setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) override;
        void setViewports(const RenderViewport *viewports, uint32_t count) override;
        void setScissors(const RenderRect *scissorRects, uint32_t count) override;
        void setFramebuffer(const RenderFramebuffer *framebuffer) override;
        void encodeCommonClear(MTL::RenderCommandEncoder *encoder, const RenderRect *clearRects, uint32_t clearRectsCount);
        void clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) override;
        void clearDepth(bool clearDepth, float depthValue, const RenderRect *clearRects, uint32_t clearRectsCount) override;
        void copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) override;
        void copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) override;
        void copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) override;
        void copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) override;
        void resolveTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) override;
        void resolveTextureRegion(const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect) override;
        void buildBottomLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo &buildInfo) override;
        void buildTopLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo &buildInfo) override;
        void setDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex, bool setCompute);
        void bindDescriptorSetLayout(const MetalPipelineLayout* layout, MTL::CommandEncoder* encoder, const std::unordered_map<uint32_t, MetalDescriptorSet*>& descriptorSets, MTL::Buffer* pushConstantsBuffer, bool isCompute);
        void configureRenderDescriptor(MTL::RenderPassDescriptor *descriptor, EncoderType encoderType);
        void endOtherEncoders(EncoderType type);
        void checkActiveComputeEncoder();
        void endActiveComputeEncoder();
        void checkActiveRenderEncoder();
        void endActiveRenderEncoder();
        void checkActiveClearColorRenderEncoder();
        void endActiveClearColorRenderEncoder();
        void checkActiveBlitEncoder();
        void endActiveBlitEncoder();
        void checkActiveResolveTextureComputeEncoder();
        void endActiveResolveTextureComputeEncoder();
        void checkActiveClearDepthRenderEncoder();
        void endActiveClearDepthRenderEncoder();
    };

    struct MetalCommandFence : RenderCommandFence {
        MetalCommandFence(MetalDevice *device);
        ~MetalCommandFence() override;
    };

    struct MetalCommandSemaphore : RenderCommandSemaphore {
        MetalCommandSemaphore(MetalDevice *device);
        ~MetalCommandSemaphore() override;
    };

    struct MetalCommandQueue : RenderCommandQueue {
        MTL::CommandBuffer *buffer = nullptr;
        MTL::CommandQueue *mtl = nullptr;
        MetalDevice *device = nullptr;

        MetalCommandQueue(MetalDevice *device, RenderCommandListType type);
        ~MetalCommandQueue() override;
        std::unique_ptr<RenderCommandList> createCommandList(RenderCommandListType type) override;
        std::unique_ptr<RenderSwapChain> createSwapChain(RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) override;
        void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) override;
        void waitForCommandFence(RenderCommandFence *fence) override;
    };

    struct MetalBuffer : RenderBuffer {
        MTL::Buffer *mtl = nullptr;
        MetalPool *pool = nullptr;
        RenderBufferDesc desc;

        MetalBuffer() = default;
        MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc);
        ~MetalBuffer() override;
        void *map(uint32_t subresource, const RenderRange *readRange) override;
        void unmap(uint32_t subresource, const RenderRange *writtenRange) override;
        std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
        void setName(const std::string &name) override;
    };

    struct MetalBufferFormattedView : RenderBufferFormattedView {
        MetalBuffer *buffer = nullptr;

        MetalBufferFormattedView(MetalBuffer *buffer, RenderFormat format);
        ~MetalBufferFormattedView() override;
    };

    struct MetalTexture : RenderTexture {
        MTL::Texture *mtl = nullptr;
        RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
        MetalPool *pool = nullptr;
        RenderTextureDesc desc;
        MetalSwapChain *parentSwapChain = nullptr;

        MetalTexture() = default;
        MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc);
        ~MetalTexture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;
    };

    struct MetalTextureView : RenderTextureView {
        MTL::Texture *texture = nullptr;

        MetalTextureView(MetalTexture *texture, const RenderTextureViewDesc &desc);
        ~MetalTextureView() override;
    };

    struct MetalAccelerationStructure : RenderAccelerationStructure {
        MetalDevice *device = nullptr;
        const MetalBuffer *buffer = nullptr;
        uint64_t offset = 0;
        uint64_t size = 0;
        RenderAccelerationStructureType type = RenderAccelerationStructureType::UNKNOWN;

        MetalAccelerationStructure(MetalDevice *device, const RenderAccelerationStructureDesc &desc);
        ~MetalAccelerationStructure() override;
    };

    struct MetalPool : RenderPool {
        MTL::Heap *heap = nullptr;
        MetalDevice *device = nullptr;

        MetalPool(MetalDevice *device, const RenderPoolDesc &desc);
        ~MetalPool() override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
    };

    struct MetalShader : RenderShader {
        NS::String *functionName = nullptr;
        RenderShaderFormat format = RenderShaderFormat::UNKNOWN;
        MTL::Library *library = nullptr;

        MetalShader(MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
        ~MetalShader() override;
        MTL::Function* createFunction(const RenderSpecConstant *specConstants, uint32_t specConstantsCount) const;
    };

    struct MetalSampler : RenderSampler {
        MTL::SamplerState *state = nullptr;
        RenderBorderColor borderColor = RenderBorderColor::UNKNOWN;
        RenderShaderVisibility shaderVisibility = RenderShaderVisibility::UNKNOWN;

        MetalSampler(MetalDevice *device, const RenderSamplerDesc &desc);
        ~MetalSampler() override;
    };

    struct MetalPipeline : RenderPipeline {
        enum class Type {
            Unknown,
            Compute,
            Graphics,
            Raytracing
        };

        Type type = Type::Unknown;

        MetalPipeline(MetalDevice *device, Type type);
        virtual ~MetalPipeline() override;
    };

    struct MetalComputePipeline : MetalPipeline {
        MetalComputeState *state;
        MetalComputePipeline(MetalDevice *device, const RenderComputePipelineDesc &desc);
        ~MetalComputePipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalGraphicsPipeline : MetalPipeline {
        MetalRenderState *state;
        MetalGraphicsPipeline(MetalDevice *device, const RenderGraphicsPipelineDesc &desc);
        ~MetalGraphicsPipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalPipelineLayout : RenderPipelineLayout {
        MTL::Buffer *pushConstantsBuffer = nullptr;
        std::vector<RenderPushConstantRange> pushConstantRanges;
        uint32_t setCount = 0;
        std::vector<MetalDescriptorSetLayout *> setLayoutHandles;

        MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc);
        ~MetalPipelineLayout() override;
    };

    struct MetalDevice : RenderDevice {
        MTL::Device *mtl = nullptr;
        MetalInterface *renderInterface = nullptr;
        RenderDeviceCapabilities capabilities;
        RenderDeviceDescription description;

        explicit MetalDevice(MetalInterface *renderInterface);
        ~MetalDevice() override;
        std::unique_ptr<RenderDescriptorSet> createDescriptorSet(const RenderDescriptorSetDesc &desc) override;
        std::unique_ptr<RenderShader> createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) override;
        std::unique_ptr<RenderSampler> createSampler(const RenderSamplerDesc &desc) override;
        std::unique_ptr<RenderPipeline> createComputePipeline(const RenderComputePipelineDesc &desc) override;
        std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) override;
        std::unique_ptr<RenderPipeline> createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) override;
        std::unique_ptr<RenderCommandQueue> createCommandQueue(RenderCommandListType type) override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
        std::unique_ptr<RenderAccelerationStructure> createAccelerationStructure(const RenderAccelerationStructureDesc &desc) override;
        std::unique_ptr<RenderPool> createPool(const RenderPoolDesc &desc) override;
        std::unique_ptr<RenderPipelineLayout> createPipelineLayout(const RenderPipelineLayoutDesc &desc) override;
        std::unique_ptr<RenderCommandFence> createCommandFence() override;
        std::unique_ptr<RenderCommandSemaphore> createCommandSemaphore() override;
        std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc &desc) override;
        void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) override;
        void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) override;
        void setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) override;
        const RenderDeviceCapabilities &getCapabilities() const override;
        const RenderDeviceDescription &getDescription() const override;
        RenderSampleCounts getSampleCountsSupported(RenderFormat format) const override;
        void release();
        bool isValid() const;
    };

    struct MetalInterface : RenderInterface {
        MTL::Device* device;
        RenderInterfaceCapabilities capabilities;
        
        MetalInterface();
        ~MetalInterface() override;
        std::unique_ptr<RenderDevice> createDevice() override;
        const RenderInterfaceCapabilities &getCapabilities() const override;
        bool isValid() const;
        
        // Shader libraries and pipeline states used for emulated operations
        void createResolvePipelineState();
        void createClearColorShaderLibrary();
        void createClearDepthShaderLibrary();
    };
}