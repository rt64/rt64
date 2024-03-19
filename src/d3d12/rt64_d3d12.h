//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

#include <map>
#include <mutex>
#include <unordered_map>

#include <d3d12.h>
#include <dxgi1_4.h>

#include "D3D12MemAlloc.h"

namespace RT64 {
    struct D3D12Buffer;
    struct D3D12CommandQueue;
    struct D3D12Device;
    struct D3D12GraphicsPipeline;
    struct D3D12Interface;
    struct D3D12Pipeline;
    struct D3D12Pool;
    struct D3D12PipelineLayout;
    struct D3D12Texture;

    struct D3D12DescriptorHeapAllocator {
        enum {
            INVALID_OFFSET = 0xFFFFFFFFU
        };

        // Reference implementation http://diligentgraphics.com/diligent-engine/architecture/d3d12/variable-size-memory-allocations-manager/
        struct FreeBlock;

        typedef std::map<uint32_t, FreeBlock> OffsetFreeBlockMap;
        typedef std::multimap<uint32_t, OffsetFreeBlockMap::iterator> SizeFreeBlockMap;

        struct FreeBlock {
            uint32_t size;
            SizeFreeBlockMap::iterator sizeMapIterator;

            FreeBlock(uint32_t size) {
                this->size = size;
            }
        };

        ID3D12DescriptorHeap *hostHeap = nullptr;
        ID3D12DescriptorHeap *shaderHeap = nullptr;
        uint32_t heapSize = 0;
        uint32_t freeSize = 0;
        D3D12Device *device = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE hostCPUDescriptorHandle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE shaderCPUDescriptorHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE shaderGPUDescriptorHandle = {};
        UINT descriptorHandleIncrement = 0;
        OffsetFreeBlockMap offsetFreeBlockMap;
        SizeFreeBlockMap sizeFreeBlockMap;
        std::mutex allocationMutex;

        D3D12DescriptorHeapAllocator(D3D12Device *device, uint32_t heapSize, D3D12_DESCRIPTOR_HEAP_TYPE heapType);
        ~D3D12DescriptorHeapAllocator();
        void addFreeBlock(uint32_t offset, uint32_t size);
        uint32_t allocate(uint32_t size);
        void free(uint32_t offset, uint32_t size);
        D3D12_CPU_DESCRIPTOR_HANDLE getHostCPUHandleAt(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE getShaderCPUHandleAt(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE getShaderGPUHandleAt(uint32_t index) const;
    };

    struct D3D12DescriptorSet : RenderDescriptorSet {
        D3D12Device *device = nullptr;
        uint32_t allocatorOffset = 0;
        uint32_t entryCount = 0;
        std::vector<RenderDescriptorRangeType> descriptorTypes;
        uint32_t descriptorTypeMaxIndex = 0;
        uint32_t hostModifiedIndex = 0;
        uint32_t hostModifiedCount = 0;

        D3D12DescriptorSet(D3D12Device *device, const RenderDescriptorSetDesc &desc);
        D3D12DescriptorSet(D3D12Device *device, uint32_t entryCount);
        ~D3D12DescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
        void setSRV(uint32_t descriptorIndex, ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC *viewDesc);
        void setUAV(uint32_t descriptorIndex, ID3D12Resource *resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *viewDesc);
        void setCBV(uint32_t descriptorIndex, ID3D12Resource *resource, uint64_t bufferSize);
        void setHostModified(uint32_t descriptorIndex);
    };

    struct D3D12SwapChain : RenderSwapChain {
        IDXGISwapChain3 *d3d = nullptr;
        HANDLE waitableObject = 0;
        D3D12CommandQueue *commandQueue = nullptr;
        RenderWindow renderWindow = {};
        std::vector<D3D12Texture> textures;
        uint32_t textureCount = 0;
        RenderFormat format = RenderFormat::UNKNOWN;
        DXGI_FORMAT nativeFormat = DXGI_FORMAT_UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;

        D3D12SwapChain(D3D12CommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format);
        ~D3D12SwapChain() override;
        bool present() override;
        void resize() override;
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

    struct D3D12Framebuffer : RenderFramebuffer {
        D3D12Device *device = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<const D3D12Texture *> colorTargets;
        const D3D12Texture *depthTarget = nullptr;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> colorHandles;
        D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = {};

        D3D12Framebuffer(D3D12Device *device, const RenderFramebufferDesc &desc);
        ~D3D12Framebuffer() override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
    };

    struct D3D12CommandList : RenderCommandList {
        ID3D12GraphicsCommandList4 *d3d = nullptr;
        ID3D12CommandAllocator *commandAllocator = nullptr;
        D3D12Device *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;
        const D3D12Framebuffer *targetFramebuffer = nullptr;
        bool targetFramebufferSamplePositionsSet = false;
        bool open = false;
        const D3D12PipelineLayout *activeComputePipelineLayout = nullptr;
        const D3D12PipelineLayout *activeGraphicsPipelineLayout = nullptr;
        const D3D12GraphicsPipeline *activeGraphicsPipeline = nullptr;
        bool descriptorHeapsSet = false;
        D3D12_PRIMITIVE_TOPOLOGY activeTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        bool activeSamplePositions = false;

        D3D12CommandList(D3D12Device *device, RenderCommandListType type);
        ~D3D12CommandList() override;
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
        void checkDescriptorHeaps();
        void notifyDescriptorHeapWasChangedExternally();
        void checkTopology();
        void checkFramebufferSamplePositions();
        void setSamplePositions(const RenderTexture *texture);
        void resetSamplePositions();
        void setDescriptorSet(const D3D12PipelineLayout *activePipelineLayout, RenderDescriptorSet *descriptorSet, uint32_t setIndex, bool setCompute);
    };

    struct D3D12CommandFence : RenderCommandFence {
        ID3D12Fence *d3d = nullptr;
        D3D12Device *device = nullptr;
        HANDLE fenceEvent = 0;
        UINT64 fenceValue = 0;

        D3D12CommandFence(D3D12Device *device);
        ~D3D12CommandFence() override;
    };

    struct D3D12CommandQueue : RenderCommandQueue {
        ID3D12CommandQueue *d3d = nullptr;
        D3D12Device *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;

        D3D12CommandQueue(D3D12Device *device, RenderCommandListType type);
        ~D3D12CommandQueue() override;
        std::unique_ptr<RenderSwapChain> createSwapChain(RenderWindow renderWindow, uint32_t textureCount, RenderFormat format) override;
        void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandFence *signalFence) override;
        void waitForCommandFence(RenderCommandFence *fence) override;
    };

    struct D3D12Buffer : RenderBuffer {
        ID3D12Resource *d3d = nullptr;
        D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATE_COMMON;
        D3D12Device *device = nullptr;
        D3D12MA::Allocation *allocation = nullptr;
        D3D12Pool *pool = nullptr;
        RenderBufferDesc desc;

        D3D12Buffer() = default;
        D3D12Buffer(D3D12Device *device, D3D12Pool *pool, const RenderBufferDesc &desc);
        ~D3D12Buffer() override;
        void *map(uint32_t subresource, const RenderRange *readRange) override;
        void unmap(uint32_t subresource, const RenderRange *writtenRange) override;
        std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
    };

    struct D3D12BufferFormattedView : RenderBufferFormattedView {
        RenderFormat format = RenderFormat::UNKNOWN;
        D3D12Buffer *buffer = nullptr;

        D3D12BufferFormattedView(D3D12Buffer *buffer, RenderFormat format);
        ~D3D12BufferFormattedView() override;
    };

    struct D3D12Texture : RenderTexture {
        ID3D12Resource *d3d = nullptr;
        D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATE_COMMON;
        RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
        D3D12Device *device = nullptr;
        D3D12MA::Allocation *allocation = nullptr;
        D3D12Pool *pool = nullptr;
        RenderTextureDesc desc;
        uint32_t targetAllocatorOffset = 0;
        uint32_t targetEntryCount = 0;
        bool targetHeapDepth = false;

        D3D12Texture() = default;
        D3D12Texture(D3D12Device *device, D3D12Pool *pool, const RenderTextureDesc &desc);
        ~D3D12Texture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;
        void createRenderTargetHeap();
        void createDepthStencilHeap();
        void releaseTargetHeap();
    };

    struct D3D12TextureView : RenderTextureView {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        D3D12Texture *texture = nullptr;
        RenderTextureDimension dimension = RenderTextureDimension::UNKNOWN;
        uint32_t mipLevels = 0;
        uint32_t mipSlice = 0;

        D3D12TextureView(D3D12Texture *texture, const RenderTextureViewDesc &desc);
        ~D3D12TextureView() override;
    };

    struct D3D12AccelerationStructure :RenderAccelerationStructure {
        D3D12Device *device = nullptr;
        const D3D12Buffer *buffer = nullptr;
        uint64_t offset = 0;
        uint64_t size = 0;
        RenderAccelerationStructureType type = RenderAccelerationStructureType::UNKNOWN;

        D3D12AccelerationStructure(D3D12Device *device, const RenderAccelerationStructureDesc &desc);
        ~D3D12AccelerationStructure() override;
    };

    struct D3D12Pool : RenderPool {
        D3D12MA::Pool *d3d = nullptr;
        D3D12Device *device = nullptr;
        RenderPoolDesc desc;

        D3D12Pool(D3D12Device *device, const RenderPoolDesc &desc);
        ~D3D12Pool() override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
    };

    struct D3D12Shader : RenderShader {
        std::vector<uint8_t> d3d;
        std::string entryPointName;
        D3D12Device *device = nullptr;
        RenderShaderFormat format = RenderShaderFormat::UNKNOWN;

        D3D12Shader(D3D12Device *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
        ~D3D12Shader() override;
    };

    struct D3D12Sampler : RenderSampler {
        D3D12_SAMPLER_DESC samplerDesc = {};
        D3D12Device *device = nullptr;
        RenderBorderColor borderColor = RenderBorderColor::UNKNOWN;
        RenderShaderVisibility shaderVisibility = RenderShaderVisibility::UNKNOWN;

        D3D12Sampler(D3D12Device *device, const RenderSamplerDesc &desc);
        ~D3D12Sampler() override;
    };

    struct D3D12Pipeline : RenderPipeline {
        enum class Type {
            Unknown,
            Compute,
            Graphics,
            Raytracing
        };

        D3D12Device *device = nullptr;
        Type type = Type::Unknown;

        D3D12Pipeline(D3D12Device *device, Type type);
        virtual ~D3D12Pipeline() override;
    };

    struct D3D12ComputePipeline : D3D12Pipeline {
        ID3D12PipelineState *d3d = nullptr;

        D3D12ComputePipeline(D3D12Device *device, const RenderComputePipelineDesc &desc);
        ~D3D12ComputePipeline() override;
        virtual RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct D3D12GraphicsPipeline : D3D12Pipeline {
        ID3D12PipelineState *d3d = nullptr;
        std::vector<RenderInputSlot> inputSlots;
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

        D3D12GraphicsPipeline(D3D12Device *device, const RenderGraphicsPipelineDesc &desc);
        ~D3D12GraphicsPipeline() override;
        virtual RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct D3D12RaytracingPipeline : D3D12Pipeline {
        ID3D12StateObject *stateObject = nullptr;
        ID3D12StateObjectProperties *stateObjectProperties = nullptr;
        std::vector<void *> programShaderIdentifiers;
        std::unordered_map<std::string, RenderPipelineProgram> nameProgramMap;
        uint32_t descriptorSetCount = 0;

        D3D12RaytracingPipeline(D3D12Device *device, const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline);
        ~D3D12RaytracingPipeline() override;
        virtual RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct D3D12PipelineLayout : RenderPipelineLayout {
        ID3D12RootSignature *rootSignature = nullptr;
        D3D12Device *device = nullptr;
        std::vector<RenderPushConstantRange> pushConstantRanges;
        uint32_t setCount = 0;

        D3D12PipelineLayout(D3D12Device *device, const RenderPipelineLayoutDesc &desc);
        ~D3D12PipelineLayout() override;
    };

    struct D3D12Device : RenderDevice {
        ID3D12Device8 *d3d = nullptr;
        D3D12Interface *renderInterface = nullptr;
        IDXGIAdapter1 *adapter = nullptr;
        D3D12MA::Allocator *allocator = nullptr;
        D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL(0);
        SIZE_T dedicatedVideoMemory = 0;
        std::unique_ptr<RenderPipelineLayout> rtDummyGlobalPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> rtDummyLocalPipelineLayout;
        std::unique_ptr<D3D12DescriptorHeapAllocator> descriptorHeapAllocator;
        std::unique_ptr<D3D12DescriptorHeapAllocator> colorTargetHeapAllocator;
        std::unique_ptr<D3D12DescriptorHeapAllocator> depthTargetHeapAllocator;
        RenderDeviceCapabilities capabilities;

        D3D12Device(D3D12Interface *renderInterface);
        ~D3D12Device() override;
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
    };

    struct D3D12Interface : RenderInterface {
        IDXGIFactory4 *dxgiFactory = nullptr;
        RenderInterfaceCapabilities capabilities;

        D3D12Interface();
        ~D3D12Interface() override;
        std::unique_ptr<RenderDevice> createDevice() override;
        const RenderInterfaceCapabilities &getCapabilities() const override;
    };
};
