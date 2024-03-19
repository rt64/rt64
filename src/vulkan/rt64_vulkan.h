//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include "volk/volk.h"

#include "vk_mem_alloc.h"

namespace RT64 {
    struct VulkanCommandQueue;
    struct VulkanDevice;
    struct VulkanInterface;
    struct VulkanPool;
    struct VulkanQueue;

    struct VulkanBuffer : RenderBuffer {
        VkBuffer vk = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;
        VulkanPool *pool = nullptr;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};
        RenderBufferDesc desc;
        RenderBarrierStages barrierStages = RenderBarrierStage::NONE;

        VulkanBuffer() = default;
        VulkanBuffer(VulkanDevice *device, VulkanPool *pool, const RenderBufferDesc &desc);
        ~VulkanBuffer() override;
        void *map(uint32_t subresource, const RenderRange *readRange) override;
        void unmap(uint32_t subresource, const RenderRange *writtenRange) override;
        std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
    };

    struct VulkanBufferFormattedView : RenderBufferFormattedView {
        VkBufferView vk = VK_NULL_HANDLE;
        VulkanBuffer *buffer = nullptr;

        VulkanBufferFormattedView(VulkanBuffer *buffer, RenderFormat format);
        ~VulkanBufferFormattedView() override;
    };

    struct VulkanTexture : RenderTexture {
        VkImage vk = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        VkImageSubresourceRange imageSubresourceRange = {};
        VulkanDevice *device = nullptr;
        VulkanPool *pool = nullptr;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};
        RenderTextureLayout textureLayout = RenderTextureLayout::UNKNOWN;
        RenderBarrierStages barrierStages = RenderBarrierStage::NONE;
        bool ownership = false;
        RenderTextureDesc desc;

        VulkanTexture() = default;
        VulkanTexture(VulkanDevice *device, VulkanPool *pool, const RenderTextureDesc &desc);
        VulkanTexture(VulkanDevice *device, VkImage image);
        ~VulkanTexture() override;
        void createImageView(VkFormat format);
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) override;
        void setName(const std::string &name) override;
        void fillSubresourceRange();
    };

    struct VulkanTextureView : RenderTextureView {
        VkImageView vk = VK_NULL_HANDLE;
        VulkanTexture *texture = nullptr;

        VulkanTextureView(VulkanTexture *texture, const RenderTextureViewDesc &desc);
        ~VulkanTextureView() override;
    };

    struct VulkanAccelerationStructure : RenderAccelerationStructure {
        VkAccelerationStructureKHR vk = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;
        RenderAccelerationStructureType type = RenderAccelerationStructureType::UNKNOWN;

        VulkanAccelerationStructure(VulkanDevice *device, const RenderAccelerationStructureDesc &desc);
        ~VulkanAccelerationStructure() override;
    };

    struct VulkanDescriptorSetLayout {
        VkDescriptorSetLayout vk = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayoutBinding> setBindings;
        std::vector<uint32_t> descriptorIndexBases;
        std::vector<uint32_t> descriptorBindingIndices;
        VulkanDevice *device = nullptr;

        VulkanDescriptorSetLayout(VulkanDevice *device, const RenderDescriptorSetDesc &descriptorSetDesc);
        ~VulkanDescriptorSetLayout();
    };

    struct VulkanPipelineLayout : RenderPipelineLayout {
        VkPipelineLayout vk = VK_NULL_HANDLE;
        std::vector<VkPushConstantRange> pushConstantRanges;
        std::vector<VulkanDescriptorSetLayout *> descriptorSetLayouts;
        VulkanDevice *device = nullptr;

        VulkanPipelineLayout(VulkanDevice *device, const RenderPipelineLayoutDesc &desc);
        ~VulkanPipelineLayout() override;
    };

    struct VulkanShader : RenderShader {
        VkShaderModule vk = VK_NULL_HANDLE;
        std::string entryPointName;
        VulkanDevice *device = nullptr;
        RenderShaderFormat format = RenderShaderFormat::UNKNOWN;

        VulkanShader(VulkanDevice *device, const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format);
        ~VulkanShader() override;
    };

    struct VulkanSampler : RenderSampler {
        VkSampler vk = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;

        VulkanSampler(VulkanDevice *device, const RenderSamplerDesc &desc);
        ~VulkanSampler();
    };

    struct VulkanPipeline : RenderPipeline {
        enum class Type {
            Unknown,
            Compute,
            Graphics,
            Raytracing
        };

        VulkanDevice *device = nullptr;
        Type type = Type::Unknown;

        VulkanPipeline(VulkanDevice *device, Type type);
        virtual ~VulkanPipeline() override;
    };

    struct VulkanComputePipeline : VulkanPipeline {
        VkPipeline vk = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        VulkanComputePipeline(VulkanDevice *device, const RenderComputePipelineDesc &desc);
        ~VulkanComputePipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct VulkanGraphicsPipeline : VulkanPipeline {
        VkPipeline vk = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;

        VulkanGraphicsPipeline(VulkanDevice *device, const RenderGraphicsPipelineDesc &desc);
        ~VulkanGraphicsPipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
        static VkRenderPass createRenderPass(VulkanDevice *device, const VkFormat *renderTargetFormat, uint32_t renderTargetCount, VkFormat depthTargetFormat, VkSampleCountFlagBits sampleCount);
    };

    struct VulkanRaytracingPipeline : VulkanPipeline {
        VkPipeline vk = VK_NULL_HANDLE;
        std::unordered_map<std::string, RenderPipelineProgram> nameProgramMap;
        uint32_t groupCount = 0;
        uint32_t descriptorSetCount = 0;

        VulkanRaytracingPipeline(VulkanDevice *device, const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline);
        ~VulkanRaytracingPipeline() override;
        RenderPipelineProgram getProgram(const std::string &name) const override;
    };

    struct VulkanDescriptorSet : RenderDescriptorSet {
        VkDescriptorSet vk = VK_NULL_HANDLE;
        VulkanDescriptorSetLayout *setLayout = nullptr;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;

        VulkanDescriptorSet(VulkanDevice *device, const RenderDescriptorSetDesc &desc);
        ~VulkanDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
        void setDescriptor(uint32_t descriptorIndex, const VkDescriptorBufferInfo *bufferInfo, const VkDescriptorImageInfo *imageInfo, const VkBufferView *texelBufferView, void *pNext);
        static VkDescriptorPool createDescriptorPool(VulkanDevice *device, const std::unordered_map<VkDescriptorType, uint32_t> &typeCounts);
    };

    struct VulkanSwapChain : RenderSwapChain {
        VkSwapchainKHR vk = VK_NULL_HANDLE;
        VulkanCommandQueue *commandQueue = nullptr;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        RenderWindow renderWindow = {};
        uint32_t textureIndex = 0;
        uint32_t textureCount = 0;
        uint64_t presentCount = 0;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        VkSwapchainCreateInfoKHR createInfo = {};
        VkSurfaceFormatKHR pickedSurfaceFormat = {};
        VkPresentModeKHR pickedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkCompositeAlphaFlagBitsKHR pickedAlphaFlag = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        VkSemaphore acquireNextTextureSemaphore = VK_NULL_HANDLE;
        VkSemaphore presentTransitionSemaphore = VK_NULL_HANDLE;
        VkFence acquireNextTextureFence = VK_NULL_HANDLE;
        std::vector<VulkanTexture> textures;
        bool acquireNextTextureSemaphoreSignaled = false;
        bool presentTransitionSemaphoreSignaled = false;

        VulkanSwapChain(VulkanCommandQueue *commandQueue, RenderWindow renderWindow, uint32_t textureCount, RenderFormat format);
        ~VulkanSwapChain() override;
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
        void checkAcquireNextTextureSemaphore();
        void acquireNextTexture();
    };

    struct VulkanFramebuffer : RenderFramebuffer {
        VulkanDevice *device = nullptr;
        VkFramebuffer vk = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<const VulkanTexture *> colorAttachments;
        const VulkanTexture *depthAttachment = nullptr;
        bool depthAttachmentReadOnly = false;
        uint32_t width = 0;
        uint32_t height = 0;

        VulkanFramebuffer(VulkanDevice *device, const RenderFramebufferDesc &desc);
        ~VulkanFramebuffer() override;
        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
        bool contains(const VulkanTexture *attachment) const;
    };

    struct VulkanCommandList : RenderCommandList {
        VkCommandBuffer vk = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;
        std::unordered_set<VulkanTexture *> fromPresentTransitionSet;
        std::unordered_set<VulkanTexture *> toPresentTransitionSet;
        std::vector<VulkanTexture *> fromPresentTransitions;
        std::vector<VulkanTexture *> toPresentTransitions;
        const VulkanFramebuffer *targetFramebuffer = nullptr;
        const VulkanPipelineLayout *activeComputePipelineLayout = nullptr;
        const VulkanPipelineLayout *activeGraphicsPipelineLayout = nullptr;
        const VulkanPipelineLayout *activeRaytracingPipelineLayout = nullptr;
        VkRenderPass activeRenderPass = VK_NULL_HANDLE;

        VulkanCommandList(VulkanDevice *device, RenderCommandListType type);
        ~VulkanCommandList() override;
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
        void checkActiveRenderPass();
        void endActiveRenderPass();
        void copyAndClearTransitionSet(std::unordered_set<VulkanTexture *> &set, std::vector<VulkanTexture *> &setVector);
        void setDescriptorSet(VkPipelineBindPoint bindPoint, const VulkanPipelineLayout *pipelineLayout, const RenderDescriptorSet *descriptorSet, uint32_t setIndex);
    };

    struct VulkanCommandFence : RenderCommandFence {
        VkFence vk = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;

        VulkanCommandFence(VulkanDevice *device);
        ~VulkanCommandFence() override;
    };

    struct VulkanCommandQueue : RenderCommandQueue {
        VulkanQueue *queue = nullptr;
        VulkanDevice *device = nullptr;
        uint32_t familyIndex = 0;
        uint32_t queueIndex = 0;
        std::unordered_set<VulkanSwapChain *> swapChains;

        VulkanCommandQueue(VulkanDevice *device, RenderCommandListType commandListType);
        ~VulkanCommandQueue() override;
        std::unique_ptr<RenderSwapChain> createSwapChain(RenderWindow renderWindow, uint32_t bufferCount, RenderFormat format) override;
        void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandFence *signalFence) override;
        void waitForCommandFence(RenderCommandFence *fence) override;
    };

    struct VulkanPool : RenderPool {
        VmaPool vk = VK_NULL_HANDLE;
        VulkanDevice *device = nullptr;

        VulkanPool(VulkanDevice *device, const RenderPoolDesc &desc);
        ~VulkanPool() override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
    };
    
    struct VulkanQueue {
        VkQueue vk;
        std::unique_ptr<std::mutex> mutex;
        std::unordered_set<const VulkanCommandQueue *> virtualQueues;
    };

    struct VulkanQueueFamily {
        std::vector<VulkanQueue> queues;

        void add(VulkanCommandQueue *virtualQueue);
        void remove(VulkanCommandQueue *virtualQueue);
    };

    struct VulkanDevice : RenderDevice {
        VkDevice vk = VK_NULL_HANDLE;
        VulkanInterface *renderInterface = nullptr;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties physicalDeviceProperties = {};
        VmaAllocator allocator = VK_NULL_HANDLE;
        uint32_t queueFamilyIndices[3] = {};
        std::vector<VulkanQueueFamily> queueFamilies;
        RenderDeviceCapabilities capabilities;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties = {};
        VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocationProperties = {};
        bool loadStoreOpNoneSupported = false;

        VulkanDevice(VulkanInterface *renderInterface);
        ~VulkanDevice() override;
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

    struct VulkanInterface : RenderInterface {
        VkInstance instance = VK_NULL_HANDLE;
        VkApplicationInfo appInfo = {};
        RenderInterfaceCapabilities capabilities;

        VulkanInterface();
        ~VulkanInterface() override;
        std::unique_ptr<RenderDevice> createDevice() override;
        const RenderInterfaceCapabilities &getCapabilities() const override;
    };
};