#ifndef RT64_RT64_METAL_H
#define RT64_RT64_METAL_H

#include "rhi/rt64_render_interface.h"
#import <Metal/Metal.h>

namespace RT64 {
    struct MetalBuffer;
    struct MetalCommandQueue;
    struct MetalDevice;
    struct MetalGraphicsPipeline;
    struct MetalPipeline;
    struct MetalInterface;
    struct MetalPool;
    struct MetalPipelineLayout;
    struct MetalTexture;

    struct MetalDescriptorSet : RenderDescriptorSet {
        MetalDevice *device = nullptr;

        MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        MetalDescriptorSet(MetalDevice *device, uint32_t entryCount);
        ~MetalDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
    };

    struct MetalSwapChain : RenderSwapChain {
        MetalCommandQueue *commandQueue = nullptr;
        RenderWindow renderWindow = {};
        std::vector<MetalTexture> textures;
        uint32_t textureCount = 0;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;

        MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format);
        ~MetalSwapChain() override;
        bool present() override;
        bool resize() override;
        bool needsResize() const override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
        uint32_t getTextureIndex() const override;
        uint32_t getTextureCount() const override;
        RenderTexture *getTexture(uint32_t index) override;
        RenderWindow getWindow() const override;
        bool isEmpty() const override;
        uint32_t getRefreshRate() const override;
        void getWindowSize(uint32_t &dstWidth, uint32_t &dstHeight) const;
        void setTextures();
    };

    struct MetalFramebuffer : RenderFramebuffer {
        MetalDevice *device = nullptr;
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
        id<MTLRenderCommandEncoder> renderEncoder = nil;
        id<MTLComputeCommandEncoder> computeEncoder = nil;
        id<MTLBlitCommandEncoder> blitEncoder = nil;

        MTLPrimitiveType currentPrimitiveType = MTLPrimitiveTypeTriangle;
        MTLIndexType currentIndexType = MTLIndexTypeUInt32;
        id<MTLBuffer> indexBuffer = nil;
        MetalDevice *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;
        const MetalFramebuffer *targetFramebuffer = nullptr;
        const MetalPipelineLayout *activeComputePipelineLayout = nullptr;
        const MetalPipelineLayout *activeGraphicsPipelineLayout = nullptr;
        const MetalGraphicsPipeline *activeGraphicsPipeline = nullptr;

        MetalCommandList(MetalDevice *device, RenderCommandListType type);
        ~MetalCommandList() override;
        void begin() override;
        void end() override;
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
        void setDescriptorSet(const MetalPipelineLayout *activePipelineLayout, RenderDescriptorSet *descriptorSet, uint32_t setIndex, bool setCompute);
    };

    struct MetalCommandFence : RenderCommandFence {
        MetalDevice *device = nullptr;

        MetalCommandFence(MetalDevice *device);
        ~MetalCommandFence() override;
    };

    struct MetalCommandQueue : RenderCommandQueue {
        MetalCommandQueue *queue = nullptr;
        MetalDevice *device = nullptr;

        MetalCommandQueue(MetalDevice *device, RenderCommandListType type);
        ~MetalCommandQueue() override;
        std::unique_ptr<RenderSwapChain> createSwapChain(RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) override;
        void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandFence *signalFence) override;
        void waitForCommandFence(RenderCommandFence *fence) override;
    };

    struct MetalBuffer : RenderBuffer {
        id<MTLBuffer> buffer = nil;
        MetalDevice *device = nullptr;
        MetalPool *pool = nullptr;
        RenderBufferDesc desc;

        MetalBuffer() = default;
        MetalBuffer(MetalDevice *device, MetalPool *pool, const RenderBufferDesc &desc);
        ~MetalBuffer() override;
        void *map(uint32_t subresource, const RenderRange *readRange) override;
        void unmap(uint32_t subresource, const RenderRange *writtenRange) override;
        std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
    };

    struct MetalBufferFormattedView : RenderBufferFormattedView {
        MetalBuffer *buffer = nullptr;

        MetalBufferFormattedView(MetalBuffer *buffer, RenderFormat format);
        ~MetalBufferFormattedView() override;
    };

    struct MetalTexture : RenderTexture {
        MTLTextureDescriptor *descriptor = nullptr;
        RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
        MetalDevice *device = nullptr;
        MetalPool *pool = nullptr;
        RenderTextureDesc desc;

        MetalTexture() = default;
        MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc);
        ~MetalTexture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;
    };

    struct MetalTextureView : RenderTextureView {
        id<MTLTexture> mtlTexture = nil;
        MetalTexture *texture = nullptr;

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
        id<MTLHeap> heap = nil;
        MetalDevice *device = nullptr;

        MetalPool(MetalDevice *device, const RenderPoolDesc &desc);
        ~MetalPool() override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
    };

    struct MetalShader : RenderShader {
        std::string entryPointName;
        MetalDevice *device = nullptr;
        RenderShaderFormat format = RenderShaderFormat::UNKNOWN;

        MetalShader(MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
        ~MetalShader() override;
    };

    struct MetalSampler : RenderSampler {
        id<MTLSamplerState> samplerState = nil;
        MetalDevice *device = nullptr;
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

        MetalDevice *device = nullptr;
        Type type = Type::Unknown;

        MetalPipeline(MetalDevice *device, Type type);
        virtual ~MetalPipeline() override;
    };

    struct MetalComputePipeline : MetalPipeline {
        id<MTLComputePipelineState> state = nil;

        MetalComputePipeline(MetalDevice *device, const RenderComputePipelineDesc &desc);
        ~MetalComputePipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalGraphicsPipeline : MetalPipeline {
        id<MTLRenderPipelineState> state = nil;

        MetalGraphicsPipeline(MetalDevice *device, const RenderGraphicsPipelineDesc &desc);
        ~MetalGraphicsPipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalPipelineLayout : RenderPipelineLayout {
        MetalDevice *device = nullptr;
        std::vector<RenderPushConstantRange> pushConstantRanges;
        uint32_t setCount = 0;

        MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc);
        ~MetalPipelineLayout() override;
    };

//    struct MetalRaytracingPipeline : MetalPipeline {
//        std::vector<void *> programShaderIdentifiers;
//        std::unordered_map<std::string, RenderPipelineProgram> nameProgramMap;
//        uint32_t descriptorSetCount = 0;
//
//        MetalRaytracingPipeline(MetalDevice *device, const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline);
//        ~MetalRaytracingPipeline() override;
//        virtual RenderPipelineProgram getProgram(const std::string &name) const override;
//    };

    struct MetalDevice : RenderDevice {
        id<MTLDevice> device;
        id<MTLCommandQueue> queue;
        id<MTLCommandBuffer> buffer;
        MetalInterface *renderInterface = nullptr;
        RenderDeviceCapabilities capabilities;

        explicit MetalDevice(MetalInterface *renderInterface);
        ~MetalDevice() override;
        std::unique_ptr<RenderCommandList> createCommandList(RenderCommandListType type) override;
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
        std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc &desc) override;
        void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) override;
        void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) override;
        void setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) override;
        const RenderDeviceCapabilities &getCapabilities() const override;
        RenderSampleCounts getSampleCountsSupported(RenderFormat format) const override;
        void release();
        bool isValid() const;
    };

    struct MetalInterface : RenderInterface {
        RenderInterfaceCapabilities capabilities;
        id<MTLDevice> device;

        MetalInterface();
        ~MetalInterface() override;
        std::unique_ptr<RenderDevice> createDevice() override;
        const RenderInterfaceCapabilities &getCapabilities() const override;
        bool isValid() const;
    };
}

#endif //RT64_RT64_METAL_H
