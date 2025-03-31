#pragma once

#include <set>
#include <unordered_set>

#include "rhi/rt64_render_interface.h"
#include "apple/rt64_apple.h"

#include <simd/simd.h>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <TargetConditionals.h>

/// macOS
#ifndef RT64_MACOS
#    define RT64_MACOS                (TARGET_OS_OSX || TARGET_OS_MACCATALYST)
#endif

/// iOS
#ifndef RT64_IOS
#    define RT64_IOS                    (TARGET_OS_IOS && !TARGET_OS_MACCATALYST)
#endif

/// Apple Silicon (iOS, tvOS, macOS)
#ifndef RT64_APPLE_SILICON
#    define RT64_APPLE_SILICON        TARGET_CPU_ARM64
#endif

/// Apple Silicon on macOS
#ifndef RT64_MACOS_APPLE_SILICON
#    define RT64_MACOS_APPLE_SILICON    (RT64_MACOS && RT64_APPLE_SILICON)
#endif

namespace RT64 {
    static constexpr size_t MAX_CLEAR_RECTS = 16;
    static constexpr uint32_t MAX_BINDING_NUMBER = 128;
    static constexpr size_t DESCRIPTOR_SET_MAX_INDEX = 8;

    struct MetalInterface;
    struct MetalDevice;
    struct MetalCommandQueue;
    struct MetalTexture;
    struct MetalTextureView;
    struct MetalBuffer;
    struct MetalBufferFormattedView;
    struct MetalPipelineLayout;
    struct MetalGraphicsPipeline;
    struct MetalPool;
    struct MetalDrawable;

    enum class EncoderType {
        None,
        Render,
        Compute,
        Blit,
        Resolve
    };

    struct ComputeStateFlags {
        uint32_t pipelineState : 1;
        uint32_t descriptorSets : 1;
        uint32_t pushConstants : 1;

        // marks from which descriptor set we'll invalidate from
        uint32_t descriptorSetDirtyIndex : 5;

        void setAll() {
            pipelineState = 1;
            descriptorSets = 1;
            pushConstants = 1;

            descriptorSetDirtyIndex = 0;
        }
    };

    struct GraphicsStateFlags {
        uint32_t pipelineState : 1;
        uint32_t descriptorSets : 1;
        uint32_t pushConstants : 1;
        uint32_t viewports : 1;
        uint32_t scissors : 1;
        uint32_t vertexBuffers : 1;
        uint32_t indexBuffer : 1;

        // marks from which descriptor set we'll invalidate from
        uint32_t descriptorSetDirtyIndex : 5;

        void setAll() {
            pipelineState = 1;
            descriptorSets = 1;
            pushConstants = 1;
            viewports = 1;
            scissors = 1;
            vertexBuffers = 1;
            indexBuffer = 1;

            descriptorSetDirtyIndex = 0;
        }
    };

    struct MetalArgumentBuffer {
        MTL::Buffer *mtl;
        MTL::ArgumentEncoder *argumentEncoder;
        uint32_t offset;
    };

    struct Descriptor {};

    struct BufferDescriptor: Descriptor {
        MTL::Buffer *buffer;
        uint32_t offset = 0;
    };

    struct TextureDescriptor: Descriptor {
        MTL::Texture *texture;
    };

    struct SamplerDescriptor: Descriptor {
        MTL::SamplerState *state;
    };

    struct MetalDescriptorSetLayout {
        struct DescriptorSetLayoutBinding {
            uint32_t binding;
            uint32_t descriptorCount;
            RenderDescriptorRangeType descriptorType;
            std::vector<MTL::SamplerState *> immutableSamplers;
        };

        MetalDevice *device = nullptr;
        std::vector<DescriptorSetLayoutBinding> setBindings;
        std::vector<int32_t> bindingToIndex;
        MTL::ArgumentEncoder *argumentEncoder = nullptr;
        std::vector<MTL::ArgumentDescriptor *> argumentDescriptors;

        std::vector<uint32_t> descriptorIndexBases;
        std::vector<uint32_t> descriptorBindingIndices;
        uint32_t descriptorTypeMaxIndex = 0;

        MetalDescriptorSetLayout(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        ~MetalDescriptorSetLayout();

        DescriptorSetLayoutBinding* getBinding(uint32_t binding, uint32_t bindingIndexOffset = 0);
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
    };

    struct ExtendedRenderTexture: RenderTexture {
        RenderTextureDesc desc;
        virtual MTL::Texture* getTexture() const = 0;
    };

    struct MetalDescriptorSet : RenderDescriptorSet {
        struct ResourceEntry {
            MTL::Resource* resource = nullptr;
            RenderDescriptorRangeType type = RenderDescriptorRangeType::UNKNOWN;
        };

        MetalDevice *device = nullptr;
        std::unique_ptr<MetalDescriptorSetLayout> setLayout;
        std::vector<Descriptor> descriptors;

        MetalArgumentBuffer argumentBuffer;

        std::vector<ResourceEntry> resourceEntries;

        MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        MetalDescriptorSet(MetalDevice *device, uint32_t entryCount);
        ~MetalDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        void setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;

        void setDescriptor(uint32_t descriptorIndex, const Descriptor *descriptor);
        void bindImmutableSamplers() const;
        RenderDescriptorRangeType getDescriptorType(uint32_t binding) const;
    };

    struct MetalSwapChain : RenderSwapChain {
        CA::MetalLayer *layer = nullptr;
        MetalCommandQueue *commandQueue = nullptr;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;
        std::vector<MetalDrawable> drawables;
        uint32_t currentAvailableDrawableIndex = 0;

        RenderWindow renderWindow = {};
        std::unique_ptr<CocoaWindow> windowWrapper;

        MetalSwapChain(MetalCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format);
        ~MetalSwapChain() override;
        bool present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) override;
        void wait() override;
        bool resize() override;
        bool needsResize() const override;
        void setVsyncEnabled(bool vsyncEnabled) override;
        bool isVsyncEnabled() const override;
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
        std::vector<const ExtendedRenderTexture *> colorAttachments;
        const MetalTexture *depthAttachment = nullptr;

        MTL::SamplePosition samplePositions[16] = {};
        uint32_t sampleCount = 0;

        MetalFramebuffer(const MetalDevice *device, const RenderFramebufferDesc &desc);
        ~MetalFramebuffer() override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;

        // this comparison is tailored towards whether we'll require a new encoder
        bool operator==(const MetalFramebuffer& other) const {
            if (colorAttachments.size() != other.colorAttachments.size()) {
                return false;
            }

            for (size_t i = 0; i < colorAttachments.size(); i++) {
                if (colorAttachments[i]->getTexture() != other.colorAttachments[i]->getTexture() ||
                    colorAttachments[i]->desc.multisampling.sampleCount != other.colorAttachments[i]->desc.multisampling.sampleCount) {
                    return false;
                }

                // Compare individual sample locations if multisampling is enabled
                if (colorAttachments[i]->desc.multisampling.sampleCount > 1) {
                    for (uint32_t s = 0; s < colorAttachments[i]->desc.multisampling.sampleCount; s++) {
                        const auto& loc1 = colorAttachments[i]->desc.multisampling.sampleLocations[s];
                        const auto& loc2 = other.colorAttachments[i]->desc.multisampling.sampleLocations[s];
                        if (loc1 != loc2) {
                            return false;
                        }
                    }
                }
            }

            return depthAttachment == other.depthAttachment;
        }

        bool operator!=(const MetalFramebuffer& other) const {
            return !(*this == other);
        }

    };

    struct MetalCommandList : RenderCommandList {
        struct PushConstantData : RenderPushConstantRange {
            std::vector<uint8_t> data;

            bool operator==(const PushConstantData& other) const {
                return offset == other.offset && size == other.size && stageFlags == other.stageFlags && data == other.data;
            }

            bool operator!=(const PushConstantData& other) const {
                return !(*this == other);
            }
        };

        MTL::CommandBuffer *mtl = nullptr;
        EncoderType activeType = EncoderType::None;
        MTL::RenderCommandEncoder *activeRenderEncoder = nullptr;
        MTL::ComputeCommandEncoder *activeComputeEncoder = nullptr;
        MTL::BlitCommandEncoder *activeBlitEncoder = nullptr;
        MTL::ComputeCommandEncoder *activeResolveComputeEncoder = nullptr;

        ComputeStateFlags dirtyComputeState{};
        GraphicsStateFlags dirtyGraphicsState{};

        struct {
            MTL::RenderPipelineState* lastPipelineState = nullptr;
            std::vector<MTL::Viewport> lastViewports;
            std::vector<MTL::ScissorRect> lastScissors;
            std::vector<MTL::Buffer*> lastVertexBuffers;
            std::vector<uint32_t> lastVertexBufferOffsets;
            std::vector<uint32_t> lastVertexBufferIndices;
            std::vector<PushConstantData> lastPushConstants;
        } stateCache;

        MTL::PrimitiveType currentPrimitiveType = MTL::PrimitiveTypeTriangle;
        MTL::IndexType currentIndexType = MTL::IndexTypeUInt32;
        MTL::Buffer *indexBuffer = nullptr;
        uint32_t indexBufferOffset = 0;

        uint32_t viewCount = 0;
        std::vector<MTL::Buffer *> vertexBuffers;
        std::vector<uint32_t> vertexBufferOffsets;
        std::vector<uint32_t> vertexBufferIndices;

        std::vector<MTL::Viewport> viewportVector;
        std::vector<MTL::ScissorRect> scissorVector;

        std::vector<PushConstantData> pushConstants;

        MetalDevice *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;
        const MetalCommandQueue *queue = nullptr;
        const MetalFramebuffer *targetFramebuffer = nullptr;
        const MetalPipelineLayout *activeComputePipelineLayout = nullptr;
        const MetalPipelineLayout *activeGraphicsPipelineLayout = nullptr;
        const MetalGraphicsPipeline *activeGraphicsPipeline = nullptr;
        const MetalRenderState *activeRenderState = nullptr;
        const MetalComputeState *activeComputeState = nullptr;

        const MetalDescriptorSet* renderDescriptorSets[DESCRIPTOR_SET_MAX_INDEX + 1] = {};
        const MetalDescriptorSet* computeDescriptorSets[DESCRIPTOR_SET_MAX_INDEX + 1] = {};
        
        std::unordered_set<MetalDescriptorSet*> currentEncoderDescriptorSets;
        void bindEncoderResources(MTL::CommandEncoder* encoder, bool isCompute);

        MetalCommandList(const MetalCommandQueue *queue, RenderCommandListType type);
        ~MetalCommandList() override;
        void begin() override;
        void end() override;
        void endEncoder(bool clearDescs);
        void commit();
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

        void endOtherEncoders(EncoderType type);
        void checkActiveComputeEncoder();
        void endActiveComputeEncoder();
        void checkActiveRenderEncoder();
        void endActiveRenderEncoder();
        void checkActiveBlitEncoder();
        void endActiveBlitEncoder();
        void checkActiveResolveTextureComputeEncoder();
        void endActiveResolveTextureComputeEncoder();
        void prepareClearVertices(const RenderRect& rect, simd::float2* outVertices);
        void checkForUpdatesInGraphicsState();
        void setCommonClearState() const;
    };

    struct MetalCommandFence : RenderCommandFence {
        dispatch_semaphore_t semaphore;

        MetalCommandFence(MetalDevice *device);
        ~MetalCommandFence() override;
    };

    struct MetalCommandSemaphore : RenderCommandSemaphore {
        MTL::Event *mtl;
        std::atomic<uint64_t> mtlEventValue;

        MetalCommandSemaphore(const MetalDevice *device);
        ~MetalCommandSemaphore() override;
    };

    struct MetalCommandQueue : RenderCommandQueue {
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
        MetalDevice *device = nullptr;
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
        MTL::Texture *texture = nullptr;

        MetalBufferFormattedView(MetalBuffer *buffer, RenderFormat format);
        ~MetalBufferFormattedView() override;
    };

    struct MetalDrawable : ExtendedRenderTexture {
        CA::MetalDrawable *mtl = nullptr;

        MetalDrawable() = default;
        MetalDrawable(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc);
        ~MetalDrawable() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;

        MTL::Texture* getTexture() const override { return mtl->texture(); }
    };

    struct MetalTexture : ExtendedRenderTexture {
        MTL::Texture *mtl = nullptr;
        RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
        MetalPool *pool = nullptr;
        uint32_t arrayCount = 1;
        MTL::Drawable *drawable = nullptr;

        MetalTexture() = default;
        MetalTexture(const MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc);
        ~MetalTexture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;

        MTL::Texture* getTexture() const override { return mtl; }
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

        MetalShader(const MetalDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
        ~MetalShader() override;
        MTL::Function* createFunction(const RenderSpecConstant *specConstants, uint32_t specConstantsCount) const;
    };

    struct MetalSampler : RenderSampler {
        MTL::SamplerState *state = nullptr;
        RenderBorderColor borderColor = RenderBorderColor::UNKNOWN;
        RenderShaderVisibility shaderVisibility = RenderShaderVisibility::UNKNOWN;

        MetalSampler(const MetalDevice *device, const RenderSamplerDesc &desc);
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

        MetalPipeline(const MetalDevice *device, Type type);
        ~MetalPipeline() override;
    };

    struct MetalComputePipeline : MetalPipeline {
        MetalComputeState state;
        MetalComputePipeline(const MetalDevice *device, const RenderComputePipelineDesc &desc);
        ~MetalComputePipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalGraphicsPipeline : MetalPipeline {
        MetalRenderState state;
        MetalGraphicsPipeline(const MetalDevice *device, const RenderGraphicsPipelineDesc &desc);
        ~MetalGraphicsPipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct MetalPipelineLayout : RenderPipelineLayout {
        std::vector<RenderPushConstantRange> pushConstantRanges;
        uint32_t setLayoutCount = 0;

        MetalPipelineLayout(MetalDevice *device, const RenderPipelineLayoutDesc &desc);
        ~MetalPipelineLayout() override;

        void bindDescriptorSets(MTL::CommandEncoder* encoder, const MetalDescriptorSet* const* descriptorSets, uint32_t descriptorSetCount, bool isCompute, uint32_t startIndex, std::unordered_set<MetalDescriptorSet*>& encoderDescriptorSets) const;
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
        bool beginCapture() override;
        bool endCapture() override;
    };

    struct MetalInterface : RenderInterface {
        MTL::Device* device;
        RenderInterfaceCapabilities capabilities;
        MTL::ComputePipelineState *resolveTexturePipelineState;

        // Clear functionality
        MTL::Function* clearVertexFunction;
        MTL::Function* clearColorFunction;
        MTL::Function* clearDepthFunction;
        MTL::DepthStencilState *clearDepthStencilState;

        std::mutex clearPipelineStateMutex;
        std::unordered_map<uint64_t, MTL::RenderPipelineState *> clearRenderPipelineStates;

        // Blit functionality
        MTL::BlitPassDescriptor *reusableBlitDescriptor = nullptr;

        MetalInterface();
        ~MetalInterface() override;
        std::unique_ptr<RenderDevice> createDevice() override;
        const RenderInterfaceCapabilities &getCapabilities() const override;
        bool isValid() const;

        // Shader libraries and pipeline states used for emulated operations
        void createResolvePipelineState();
        void createClearShaderLibrary();

        MTL::RenderPipelineState* getOrCreateClearRenderPipelineState(MTL::RenderPipelineDescriptor *pipelineDesc, bool depthWriteEnabled = false);
    };
}
