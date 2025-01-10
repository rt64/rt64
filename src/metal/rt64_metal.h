#pragma once

#include <set>

#include "rhi/rt64_render_interface.h"
#include <simd/simd.h>

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

static constexpr size_t MAX_CLEAR_RECTS = 16;

namespace RT64 {
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
        Render,
        Compute,
        Blit,
        Resolve
    };

    struct ComputeStateFlags {
        uint32_t pipelineState : 1;
        uint32_t descriptorSets : 1;
        uint32_t pushConstants : 1;
        
        void setAll() {
            pipelineState = 1;
            descriptorSets = 1;
            pushConstants = 1;
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
        
        void setAll() {
            pipelineState = 1;
            descriptorSets = 1;
            pushConstants = 1;
            viewports = 1;
            scissors = 1;
            vertexBuffers = 1;
            indexBuffer = 1;
        }
    };

    struct MetalArgumentBuffer {
        MTL::ArgumentEncoder *argumentEncoder;
        MTL::Buffer *argumentBuffer;
        uint32_t argumentBufferOffset;
        uint32_t argumentBufferEncodedSize;
        
        void setBuffer(MTL::Buffer *buffer, uint32_t offset, uint32_t index);
        void setTexture(MTL::Texture *texture, uint32_t index);
        void setSamplerState(MTL::SamplerState *sampler, uint32_t index);
    };MetalArgumentBuffer

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
        std::unordered_map<uint32_t, uint32_t> bindingToIndex;
        
        std::vector<uint32_t> descriptorIndexBases;
        std::vector<uint32_t> descriptorBindingIndices;

//        std::vector<MTL::SamplerState *> staticSamplers;
//        std::vector<MTL::ArgumentDescriptor *> argumentDescriptors;
//        MTL::ArgumentEncoder *argumentEncoder = nullptr;
//        MTL::Buffer *descriptorBuffer = nullptr;
//        std::vector<RenderDescriptorRangeType> descriptorTypes;
//        std::vector<uint32_t> descriptorToRangeIndex;
//        std::vector<uint32_t> descriptorIndexBases;
//        std::vector<uint32_t> descriptorRangeBinding;
//        std::vector<uint32_t> samplerIndices;
//        uint32_t currentArgumentBufferOffset = 0;
//        uint32_t entryCount = 0;
//        uint32_t descriptorTypeMaxIndex = 0;

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

    struct MetalBufferBinding {
        const MetalBuffer *buffer;
        uint32_t offset;

        MetalBufferBinding(const MetalBuffer* buffer = nullptr, uint32_t offset = 0)
            : buffer(buffer), offset(offset) {}
    };

    struct ExtendedRenderTexture: RenderTexture {
        RenderTextureDesc desc;
        virtual MTL::Texture* getTexture() const = 0;
    };

    struct MetalDescriptorSet : RenderDescriptorSet {
//        std::unordered_map<uint32_t, const MetalTexture *> indicesToTextures;
//        std::unordered_map<uint32_t, const MetalTextureView *> indicesToTextureViews;
//        std::unordered_map<uint32_t, MetalBufferBinding> indicesToBuffers;
//        std::unordered_map<uint32_t, const MetalBufferFormattedView *> indicesToBufferFormattedViews;
//        std::unordered_map<uint32_t, MTL::SamplerState *> indicesToSamplers;
//        std::vector<MTL::SamplerState *> staticSamplers;
//        std::vector<MTL::ArgumentDescriptor *> argumentDescriptors;
//        uint32_t entryCount = 0;
//        uint32_t descriptorTypeMaxIndex = 0;
//        std::vector<RenderDescriptorRangeType> descriptorTypes;
        
        MetalDevice *device = nullptr;
        MetalDescriptorSetLayout *setLayout = nullptr;
        std::vector<Descriptor> descriptors;
        
        MetalArgumentBuffer argumentBuffer;

        MetalDescriptorSet(MetalDevice *device, const RenderDescriptorSetDesc &desc);
        MetalDescriptorSet(MetalDevice *device, uint32_t entryCount);
        ~MetalDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        virtual void setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
        
        void setDescriptor(uint32_t descriptorIndex, const MetalBuffer *buffer, const MetalTexture *texture, const MetalTextureView *textureView);
        RenderDescriptorRangeType getDescriptorType(uint32_t binding);
    };

    struct MetalSwapChain : RenderSwapChain {
        CA::MetalLayer *layer = nullptr;
        MetalCommandQueue *commandQueue = nullptr;
        RenderWindow renderWindow = {};
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;
        std::vector<MetalDrawable> drawables;
        uint32_t currentAvailableDrawableIndex = 0;

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
        std::vector<const ExtendedRenderTexture *> colorAttachments;
        const MetalTexture *depthAttachment = nullptr;
        
        MTL::SamplePosition samplePositions[16] = {};
        uint32_t sampleCount = 0;

        MetalFramebuffer(MetalDevice *device, const RenderFramebufferDesc &desc);
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
        struct PendingColorClear {
            uint32_t attachmentIndex;
            RenderColor colorValue;
            RenderRect clearRects[MAX_CLEAR_RECTS];
            uint32_t clearRectCount;
        };

        struct PendingDepthClear {
            float depthValue;
            RenderRect clearRects[MAX_CLEAR_RECTS];
            uint32_t clearRectCount;
        };
        
        struct PushConstantData : RenderPushConstantRange {
            std::vector<uint8_t> data;
            
            bool operator==(const PushConstantData& other) const {
                return offset == other.offset && size == other.size && stageFlags == other.stageFlags && data == other.data;
            }
            
            bool operator!=(const PushConstantData& other) const {
                return !(*this == other);
            }
        };
        
        struct ClearVertex {
            simd::float4 position;  // xy = position, z = 0, w = unused
        };
        
        MTL::CommandBuffer *mtl = nullptr;
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

        std::unordered_map<uint32_t, MetalDescriptorSet *> indicesToRenderDescriptorSets;
        std::unordered_map<uint32_t, MetalDescriptorSet *> indicesToComputeDescriptorSets;

        MetalCommandList(MetalCommandQueue *queue, RenderCommandListType type);
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
        void bindDescriptorSetLayout(const MetalPipelineLayout* layout, MTL::CommandEncoder* encoder, const std::unordered_map<uint32_t, MetalDescriptorSet*>& descriptorSets, bool isCompute);
        void endOtherEncoders(EncoderType type);
        void checkActiveComputeEncoder();
        void endActiveComputeEncoder();
        void checkActiveRenderEncoder();
        void endActiveRenderEncoder();
        void checkActiveBlitEncoder();
        void endActiveBlitEncoder();
        void checkActiveResolveTextureComputeEncoder();
        void endActiveResolveTextureComputeEncoder();
        
        std::vector<simd::float2> prepareClearVertices(const RenderRect& rect);
        void checkForUpdatesInGraphicsState();
        void setCommonClearState();
    };

    struct MetalCommandFence : RenderCommandFence {
        dispatch_semaphore_t semaphore;

        MetalCommandFence(MetalDevice *device);
        ~MetalCommandFence() override;
    };

    struct MetalCommandSemaphore : RenderCommandSemaphore {
        MTL::Event *mtl;
        std::atomic<uint64_t> mtlEventValue;

        MetalCommandSemaphore(MetalDevice *device);
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
        mutable std::set<std::pair<MetalDescriptorSet *, uint32_t>> residenceSets;

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
        mutable std::set<std::pair<MetalDescriptorSet *, uint32_t>> residenceSets;

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

        MTL::Texture* getTexture() const override {
            return mtl->texture();
        }
    };

    struct MetalTexture : ExtendedRenderTexture {
        MTL::Texture *mtl = nullptr;
        RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
        MetalPool *pool = nullptr;
        uint32_t arrayCount = 1;
        MTL::Drawable *drawable = nullptr;
        mutable std::set<std::pair<MetalDescriptorSet *, uint32_t>> residenceSets;

        MetalTexture() = default;
        MetalTexture(MetalDevice *device, MetalPool *pool, const RenderTextureDesc &desc);
        ~MetalTexture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;
        
        MTL::Texture* getTexture() const override {
            return mtl;
        }
    };

    struct MetalTextureView : RenderTextureView {
        MetalTexture *backingTexture = nullptr;
        MTL::Texture *texture = nullptr;
        mutable std::set<std::pair<MetalDescriptorSet *, uint32_t>> residenceSets;

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
        std::vector<RenderPushConstantRange> pushConstantRanges;
        uint32_t setLayoutCount = 0;
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
