//
// RT64
//

#include "rhi/rt64_render_interface.h"

#include <cassert>
#include <cstring>
#include <chrono>
#include <thread>

#ifdef _WIN64
#include "shaders/RenderInterfaceTestPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestRT.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestVS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestCS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.dxil.h"
#endif
#include "shaders/RenderInterfaceTestPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestRT.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestVS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestCS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.spirv.h"

#define ENABLE_SWAP_CHAIN 1
#define ENABLE_CLEAR 1
#define ENABLE_RASTER 1
#define ENABLE_TEXTURE 0
#define ENABLE_COMPUTE 0
#define ENABLE_RT 0

namespace RT64 {
    struct RasterDescriptorSet : RenderDescriptorSetBase {
        uint32_t gSampler;
        uint32_t gTextures;
        
        std::unique_ptr<RenderSampler> linearSampler;
        
        RasterDescriptorSet(RenderDevice *device, uint32_t textureArraySize) {
            linearSampler = device->createSampler(RenderSamplerDesc());

            const uint32_t TextureArrayUpperRange = 512;
            builder.begin();
            gSampler = builder.addImmutableSampler(1, linearSampler.get());
            gTextures = builder.addTexture(2, TextureArrayUpperRange);
            builder.end(true, textureArraySize);

            create(device);
        }
    };

    struct ComputeDescriptorFirstSet : RenderDescriptorSetBase {
        uint32_t gBlueNoiseTexture;
        uint32_t gSampler;
        uint32_t gTarget;

        std::unique_ptr<RenderSampler> linearSampler;

        ComputeDescriptorFirstSet(RenderDevice *device) {
            linearSampler = device->createSampler(RenderSamplerDesc());

            builder.begin();
            gBlueNoiseTexture = builder.addTexture(1);
            gSampler = builder.addImmutableSampler(2, linearSampler.get());
            builder.end();

            create(device);
        }
    };

    struct ComputeDescriptorSecondSet : RenderDescriptorSetBase {
        uint32_t gTarget;

        ComputeDescriptorSecondSet(RenderDevice *device) {
            builder.begin();
            gTarget = builder.addReadWriteTexture(16);
            builder.end();

            create(device);
        }
    };

    struct RaytracingDescriptorSet : RenderDescriptorSetBase {
        uint32_t gBVH;
        uint32_t gOutput;
        uint32_t gBufferParams;

        RaytracingDescriptorSet(RenderDevice *device) {
            builder.begin();
            gBVH = builder.addAccelerationStructure(0);
            gOutput = builder.addReadWriteTexture(1);
            gBufferParams = builder.addStructuredBuffer(2);
            builder.end();

            create(device);
        }
    };

    struct TestMembers {
        static const uint32_t BufferCount = 2;
        static const uint32_t MSAACount = 4;
        static const RenderFormat ColorFormat = RenderFormat::R8G8B8A8_UNORM;
        static const RenderFormat DepthFormat = RenderFormat::D32_FLOAT;
#ifdef __ANDROID__
        static const RenderFormat SwapchainFormat = RenderFormat::R8G8B8A8_UNORM;
#else
        static const RenderFormat SwapchainFormat = RenderFormat::B8G8R8A8_UNORM;
#endif
        const RenderInterface *renderInterface = nullptr;
        std::unique_ptr<RenderDevice> device;
        std::unique_ptr<RenderCommandQueue> commandQueue;
        std::unique_ptr<RenderCommandList> commandList;
        std::unique_ptr<RenderCommandFence> commandFence;
        std::unique_ptr<RenderSwapChain> swapChain;
        std::unique_ptr<RenderFramebuffer> framebuffer;
        std::vector<std::unique_ptr<RenderFramebuffer>> swapFramebuffers;
        std::unique_ptr<RenderSampler> linearSampler;
        std::unique_ptr<RenderSampler> postSampler;
        std::unique_ptr<RasterDescriptorSet> rasterSet;
        std::unique_ptr<ComputeDescriptorFirstSet> computeFirstSet;
        std::unique_ptr<ComputeDescriptorSecondSet> computeSecondSet;
        std::unique_ptr<RaytracingDescriptorSet> rtSet;
        std::unique_ptr<RenderDescriptorSet> postSet;
        std::unique_ptr<RenderPipelineLayout> rasterPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> computePipelineLayout;
        std::unique_ptr<RenderPipelineLayout> rtPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> postPipelineLayout;
        std::unique_ptr<RenderPipeline> rasterPipeline;
        std::unique_ptr<RenderPipeline> computePipeline;
        std::unique_ptr<RenderPipeline> rtPipeline;
        std::unique_ptr<RenderPipeline> postPipeline;
        std::unique_ptr<RenderTexture> colorTargetMS;
        std::unique_ptr<RenderTexture> colorTargetResolved;
        std::unique_ptr<RenderTexture> depthTarget;
        std::unique_ptr<RenderBuffer> uploadBuffer;
        std::unique_ptr<RenderTexture> blueNoiseTexture;
        std::unique_ptr<RenderBuffer> vertexBuffer;
        std::unique_ptr<RenderBuffer> indexBuffer;
        std::unique_ptr<RenderBuffer> rtParamsBuffer;
        std::unique_ptr<RenderBuffer> rtVertexBuffer;
        std::unique_ptr<RenderBuffer> rtScratchBuffer;
        std::unique_ptr<RenderBuffer> rtInstancesBuffer;
        std::unique_ptr<RenderBuffer> rtBottomLevelASBuffer;
        std::unique_ptr<RenderAccelerationStructure> rtBottomLevelAS;
        std::unique_ptr<RenderBuffer> rtTopLevelASBuffer;
        std::unique_ptr<RenderAccelerationStructure> rtTopLevelAS;
        std::unique_ptr<RenderBuffer> rtShaderBindingTableBuffer;
        RenderShaderBindingTableInfo rtShaderBindingTableInfo;
        RenderVertexBufferView vertexBufferView;
        RenderIndexBufferView indexBufferView;
        RenderInputSlot inputSlot;
    } Test;

    struct RasterPushConstant {
        float colorAdd[4] = {};
        uint32_t textureIndex = 0;
    };

    struct ComputePushConstant {
        float Multiply[4] = {};
        uint32_t Resolution[2] = {};
    };

    void TestInitialize(RenderInterface *renderInterface, RenderWindow window) {
        Test.renderInterface = renderInterface;
        Test.device = renderInterface->createDevice();
        Test.commandQueue = Test.device->createCommandQueue(RenderCommandListType::DIRECT);
        Test.commandList = Test.device->createCommandList(RenderCommandListType::DIRECT);
        Test.commandFence = Test.device->createCommandFence();

#   if ENABLE_SWAP_CHAIN
        Test.swapChain = Test.commandQueue->createSwapChain(window, Test.BufferCount, Test.SwapchainFormat);
#   endif

#   if ENABLE_RASTER
        const uint32_t textureArraySize = 3;
        Test.rasterSet = std::make_unique<RasterDescriptorSet>(Test.device.get(), 3);

        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, true);
        layoutBuilder.addPushConstant(0, 0, sizeof(RasterPushConstant), RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(Test.rasterSet->builder);
        layoutBuilder.end();

        Test.rasterPipelineLayout = layoutBuilder.create(Test.device.get());

        // Pick shader format depending on the render interface's requirements.
        const RenderInterfaceCapabilities &interfaceCapabilities = Test.renderInterface->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;
        const void *PSBlob = nullptr;
        const void *VSBlob = nullptr;
        const void *CSBlob = nullptr;
        const void *RTBlob = nullptr;
        const void *PostPSBlob = nullptr;
        const void *PostVSBlob = nullptr;
        uint64_t PSBlobSize = 0;
        uint64_t VSBlobSize = 0;
        uint64_t CSBlobSize = 0;
        uint64_t RTBlobSize = 0;
        uint64_t PostPSBlobSize = 0;
        uint64_t PostVSBlobSize = 0;
        switch (shaderFormat) {
#ifdef _WIN64
        case RenderShaderFormat::DXIL:
            PSBlob = RenderInterfaceTestPSBlobDXIL;
            PSBlobSize = sizeof(RenderInterfaceTestPSBlobDXIL);
            VSBlob = RenderInterfaceTestVSBlobDXIL;
            VSBlobSize = sizeof(RenderInterfaceTestVSBlobDXIL);
            CSBlob = RenderInterfaceTestCSBlobDXIL;
            CSBlobSize = sizeof(RenderInterfaceTestCSBlobDXIL);
            RTBlob = RenderInterfaceTestRTBlobDXIL;
            RTBlobSize = sizeof(RenderInterfaceTestRTBlobDXIL);
            PostPSBlob = RenderInterfaceTestPostPSBlobDXIL;
            PostPSBlobSize = sizeof(RenderInterfaceTestPostPSBlobDXIL);
            PostVSBlob = RenderInterfaceTestPostVSBlobDXIL;
            PostVSBlobSize = sizeof(RenderInterfaceTestPostVSBlobDXIL);
            break;
#endif
        case RenderShaderFormat::SPIRV:
            PSBlob = RenderInterfaceTestPSBlobSPIRV;
            PSBlobSize = sizeof(RenderInterfaceTestPSBlobSPIRV);
            VSBlob = RenderInterfaceTestVSBlobSPIRV;
            VSBlobSize = sizeof(RenderInterfaceTestVSBlobSPIRV);
            CSBlob = RenderInterfaceTestCSBlobSPIRV;
            CSBlobSize = sizeof(RenderInterfaceTestCSBlobSPIRV);
            RTBlob = RenderInterfaceTestRTBlobSPIRV;
            RTBlobSize = sizeof(RenderInterfaceTestRTBlobSPIRV);
            PostPSBlob = RenderInterfaceTestPostPSBlobSPIRV;
            PostPSBlobSize = sizeof(RenderInterfaceTestPostPSBlobSPIRV);
            PostVSBlob = RenderInterfaceTestPostVSBlobSPIRV;
            PostVSBlobSize = sizeof(RenderInterfaceTestPostVSBlobSPIRV);
            break;
        default:
            assert(false && "Unknown shader format.");
            break;
        }

        const uint32_t VertexCount = 3;
        const uint32_t FloatsPerVertex = 4;
        const float Vertices[VertexCount * FloatsPerVertex] = {
            -0.5f, -0.25f, 0.0f, 0.0f,
            0.5f, -0.25f, 1.0f, 0.0f,
            0.25f, 0.25f, 0.0f, 1.0f
        };

        const uint32_t Indices[3] = {
            0, 1, 2
        };

        Test.inputSlot = RenderInputSlot(0, sizeof(float) * FloatsPerVertex);

        std::vector<RenderInputElement> inputElements;
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
        inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

        std::unique_ptr<RenderShader> pixelShader = Test.device->createShader(PSBlob, PSBlobSize, "PSMain", shaderFormat);
        std::unique_ptr<RenderShader> vertexShader = Test.device->createShader(VSBlob, VSBlobSize, "VSMain", shaderFormat);

        RenderGraphicsPipelineDesc graphicsDesc;
        graphicsDesc.inputSlots = &Test.inputSlot;
        graphicsDesc.inputSlotsCount = 1;
        graphicsDesc.inputElements = inputElements.data();
        graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
        graphicsDesc.pipelineLayout = Test.rasterPipelineLayout.get();
        graphicsDesc.pixelShader = pixelShader.get();
        graphicsDesc.vertexShader = vertexShader.get();
        graphicsDesc.renderTargetFormat[0] = Test.ColorFormat;
        graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        graphicsDesc.depthTargetFormat = Test.DepthFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.multisampling.sampleCount = Test.MSAACount;
        Test.rasterPipeline = Test.device->createGraphicsPipeline(graphicsDesc);

        Test.postSampler = Test.device->createSampler(RenderSamplerDesc());
        const RenderSampler *postSamplerPtr = Test.postSampler.get();

        // Create the post processing pipeline
        std::vector<RenderDescriptorRange> postDescriptorRanges = {
            RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 1, 1),
            RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 2, 1, &postSamplerPtr)
        };

        RenderDescriptorSetDesc postDescriptorSetDesc(postDescriptorRanges.data(), uint32_t(postDescriptorRanges.size()));
        Test.postSet = Test.device->createDescriptorSet(postDescriptorSetDesc);
        Test.postPipelineLayout = Test.device->createPipelineLayout(RenderPipelineLayoutDesc(nullptr, 0, &postDescriptorSetDesc, 1, false, true));

        inputElements.clear();
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));

        std::unique_ptr<RenderShader> postPixelShader = Test.device->createShader(PostPSBlob, PostPSBlobSize, "PSMain", shaderFormat);
        std::unique_ptr<RenderShader> postVertexShader = Test.device->createShader(PostVSBlob, PostVSBlobSize, "VSMain", shaderFormat);

        RenderGraphicsPipelineDesc postDesc;
        postDesc.inputSlots = nullptr;
        postDesc.inputSlotsCount = 0;
        postDesc.inputElements = nullptr;
        postDesc.inputElementsCount = 0;
        postDesc.pipelineLayout = Test.postPipelineLayout.get();
        postDesc.pixelShader = postPixelShader.get();
        postDesc.vertexShader = postVertexShader.get();
        postDesc.renderTargetFormat[0] = Test.SwapchainFormat;
        postDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        postDesc.renderTargetCount = 1;
        Test.postPipeline = Test.device->createGraphicsPipeline(postDesc);

#   if ENABLE_TEXTURE
        // Upload a texture.
        const uint32_t Width = 512;
        const uint32_t Height = 512;
        const uint32_t RowLength = Width;
        const RenderFormat Format = RenderFormat::R8G8B8A8_UNORM;
        const uint32_t BufferSize = RowLength * Height * RenderFormatSize(Format);
        Test.uploadBuffer = Test.device->createBuffer(RenderBufferDesc::UploadBuffer(BufferSize));
        Test.blueNoiseTexture = Test.device->createTexture(RenderTextureDesc::Texture2D(Width, Height, 1, Format));
        Test.rasterSet->setTexture(Test.rasterSet->gTextures + 2, Test.blueNoiseTexture.get(), RenderTextureLayout::SHADER_READ);

        // Copy to upload buffer.
        void *bufferData = Test.uploadBuffer->map();
        memcpy(bufferData, LDR_64_64_64_RGB1_BGRA8, BufferSize);
        Test.uploadBuffer->unmap();

        // Run command list to copy the upload buffer to the texture.
        Test.commandList->begin();
        Test.commandList->barriers(RenderBarrierStage::COPY,
            RenderBufferBarrier(Test.uploadBuffer.get(), RenderBufferAccess::READ),
            RenderTextureBarrier(Test.blueNoiseTexture.get(), RenderTextureLayout::COPY_DEST)
        );

        Test.commandList->copyTextureRegion(
            RenderTextureCopyLocation::Subresource(Test.blueNoiseTexture.get()),
            RenderTextureCopyLocation::PlacedFootprint(Test.uploadBuffer.get(), Format, Width, Height, 1, RowLength));

        Test.commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, RenderTextureBarrier(Test.blueNoiseTexture.get(), RenderTextureLayout::SHADER_READ));
        Test.commandList->end();
        Test.commandQueue->executeCommandLists(Test.commandList.get(), Test.commandFence.get());
        Test.commandQueue->waitForCommandFence(Test.commandFence.get());
#   endif

        Test.vertexBuffer = Test.device->createBuffer(RenderBufferDesc::VertexBuffer(sizeof(Vertices), RenderHeapType::UPLOAD));
        void *dstData = Test.vertexBuffer->map();
        memcpy(dstData, Vertices, sizeof(Vertices));
        Test.vertexBuffer->unmap();
        Test.vertexBufferView = RenderVertexBufferView(Test.vertexBuffer.get(), sizeof(Vertices));

        Test.indexBuffer = Test.device->createBuffer(RenderBufferDesc::IndexBuffer(sizeof(Indices), RenderHeapType::UPLOAD));
        dstData = Test.indexBuffer->map();
        memcpy(dstData, Indices, sizeof(Indices));
        Test.indexBuffer->unmap();
        Test.indexBufferView = RenderIndexBufferView(Test.indexBuffer.get(), sizeof(Indices), RenderFormat::R32_UINT);
#   endif

#   if ENABLE_COMPUTE
        Test.computeFirstSet = std::make_unique<ComputeDescriptorFirstSet>(Test.device.get());
        Test.computeSecondSet = std::make_unique<ComputeDescriptorSecondSet>(Test.device.get());
        Test.computeFirstSet->setTexture(Test.computeFirstSet->gBlueNoiseTexture, Test.blueNoiseTexture.get(), RenderTextureLayout::SHADER_READ);

        layoutBuilder.begin();
        layoutBuilder.addPushConstant(0, 0, sizeof(ComputePushConstant), RenderShaderStageFlag::COMPUTE);
        layoutBuilder.addDescriptorSet(Test.computeFirstSet->builder);
        layoutBuilder.addDescriptorSet(Test.computeSecondSet->builder);
        layoutBuilder.end();

        Test.computePipelineLayout = layoutBuilder.create(Test.device.get());

        std::unique_ptr<RenderShader> computeShader = Test.device->createShader(CSBlob, CSBlobSize, "CSMain", shaderFormat);
        RenderComputePipelineDesc computeDesc;
        computeDesc.computeShader = computeShader.get();
        computeDesc.pipelineLayout = Test.computePipelineLayout.get();
        Test.computePipeline = Test.device->createComputePipeline(computeDesc);
#   endif

#   if ENABLE_RT
        Test.rtSet = std::make_unique<RaytracingDescriptorSet>(Test.device.get());

        layoutBuilder.begin(true);
        layoutBuilder.addDescriptorSet(Test.rtSet->builder);
        layoutBuilder.end();

        Test.rtPipelineLayout = layoutBuilder.create(Test.device.get());

        struct BufferParams {
            float rgbMultiplier[3];
            uint32_t pad[3];
        };
        
        BufferParams paramsToUpload[2];
        paramsToUpload[0].rgbMultiplier[0] = 1.0f;
        paramsToUpload[0].rgbMultiplier[1] = 0.25f;
        paramsToUpload[0].rgbMultiplier[2] = 0.25f;
        paramsToUpload[1].rgbMultiplier[0] = 0.25f;
        paramsToUpload[1].rgbMultiplier[1] = 1.0f;
        paramsToUpload[1].rgbMultiplier[2] = 0.25f;

        RenderBufferStructuredView paramsView(sizeof(BufferParams));
        Test.rtParamsBuffer = Test.device->createBuffer(RenderBufferDesc::UploadBuffer(sizeof(paramsToUpload), RenderBufferFlag::STORAGE));
        Test.rtSet->setBuffer(Test.rtSet->gBufferParams, Test.rtParamsBuffer.get(), sizeof(paramsToUpload), &paramsView);
        dstData = Test.rtParamsBuffer->map();
        memcpy(dstData, paramsToUpload, sizeof(paramsToUpload));
        Test.rtParamsBuffer->unmap();

        RenderRaytracingPipelineLibrarySymbol rtLibrarySymbols[] = {
            RenderRaytracingPipelineLibrarySymbol("ColorRayGen", RenderRaytracingPipelineLibrarySymbolType::RAYGEN),
            RenderRaytracingPipelineLibrarySymbol("ColorClosestHit", RenderRaytracingPipelineLibrarySymbolType::CLOSEST_HIT),
            RenderRaytracingPipelineLibrarySymbol("ColorMiss", RenderRaytracingPipelineLibrarySymbolType::MISS)
        };

        std::unique_ptr<RenderShader> rtShader = Test.device->createShader(RTBlob, RTBlobSize, nullptr, shaderFormat);
        RenderRaytracingPipelineLibrary rtLibrary(rtShader.get(), rtLibrarySymbols, uint32_t(std::size(rtLibrarySymbols)));
        RenderRaytracingPipelineHitGroup rtHitGroup("ColorHitGroup", "ColorClosestHit");
        RenderRaytracingPipelineDesc rtPsoDesc;
        rtPsoDesc.libraries = &rtLibrary;
        rtPsoDesc.librariesCount = 1;
        rtPsoDesc.hitGroups = &rtHitGroup;
        rtPsoDesc.hitGroupsCount = 1;
        rtPsoDesc.pipelineLayout = Test.rtPipelineLayout.get();
        rtPsoDesc.maxPayloadSize = sizeof(float) * 4;
        Test.rtPipeline = Test.device->createRaytracingPipeline(rtPsoDesc);
        
        // Create RT BVH.
        const float BLASVertices[] = {
            0.25f, 0.25f, 1.0f,
            0.75f, 0.25f, 1.0f,
            0.5f, 0.75f, 1.0f
        };

        dstData = Test.uploadBuffer->map();
        memcpy(dstData, BLASVertices, sizeof(BLASVertices));
        Test.uploadBuffer->unmap();

        Test.rtVertexBuffer = Test.device->createBuffer(RenderBufferDesc::VertexBuffer(sizeof(BLASVertices), RenderHeapType::DEFAULT, RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT));

        RenderBottomLevelASMesh blasMesh;
        blasMesh.vertexBuffer = Test.rtVertexBuffer.get();
        blasMesh.vertexFormat = RenderFormat::R32G32B32_FLOAT;
        blasMesh.vertexCount = 3;
        blasMesh.vertexStride = sizeof(float) * 3;
        blasMesh.isOpaque = true;

        RenderBottomLevelASBuildInfo blasBuildInfo;
        Test.device->setBottomLevelASBuildInfo(blasBuildInfo, &blasMesh, 1);

        Test.rtBottomLevelASBuffer = Test.device->createBuffer(RenderBufferDesc::AccelerationStructureBuffer(blasBuildInfo.accelerationStructureSize));
        Test.rtBottomLevelAS = Test.device->createAccelerationStructure(RenderAccelerationStructureDesc(RenderAccelerationStructureType::BOTTOM_LEVEL, Test.rtBottomLevelASBuffer.get(), blasBuildInfo.accelerationStructureSize));

        RenderTopLevelASInstance tlasInstance;
        tlasInstance.bottomLevelAS = Test.rtBottomLevelASBuffer.get();
        tlasInstance.instanceMask = 0xFF;
        tlasInstance.cullDisable = true;

        RenderTopLevelASBuildInfo tlasBuildInfo;
        Test.device->setTopLevelASBuildInfo(tlasBuildInfo, &tlasInstance, 1);

        Test.rtInstancesBuffer = Test.device->createBuffer(RenderBufferDesc::UploadBuffer(tlasBuildInfo.instancesBufferData.size(), RenderBufferFlag::ACCELERATION_STRUCTURE_INPUT));
        dstData = Test.rtInstancesBuffer->map();
        memcpy(dstData, tlasBuildInfo.instancesBufferData.data(), tlasBuildInfo.instancesBufferData.size());
        Test.rtInstancesBuffer->unmap();

        Test.rtScratchBuffer = Test.device->createBuffer(RenderBufferDesc::DefaultBuffer(std::max(blasBuildInfo.scratchSize, tlasBuildInfo.scratchSize), RenderBufferFlag::ACCELERATION_STRUCTURE_SCRATCH));
        Test.rtTopLevelASBuffer = Test.device->createBuffer(RenderBufferDesc::AccelerationStructureBuffer(tlasBuildInfo.accelerationStructureSize));
        Test.rtTopLevelAS = Test.device->createAccelerationStructure(RenderAccelerationStructureDesc(RenderAccelerationStructureType::TOP_LEVEL, Test.rtTopLevelASBuffer.get(), tlasBuildInfo.accelerationStructureSize));
        Test.rtSet->setAccelerationStructure(Test.rtSet->gBVH, Test.rtTopLevelAS.get());

        RenderShaderBindingGroups bindingGroups;
        RenderPipelineProgram rayGenProgram = Test.rtPipeline->getProgram("ColorRayGen");
        RenderPipelineProgram missProgram = Test.rtPipeline->getProgram("ColorMiss");
        RenderPipelineProgram hitGroupProgram = Test.rtPipeline->getProgram("ColorHitGroup");
        bindingGroups.rayGen = RenderShaderBindingGroup(&rayGenProgram, 1);
        bindingGroups.miss = RenderShaderBindingGroup(&missProgram, 1);
        bindingGroups.hitGroup = RenderShaderBindingGroup(&hitGroupProgram, 1);

        RenderDescriptorSet *rtSetPtr = Test.rtSet->get();
        Test.device->setShaderBindingTableInfo(Test.rtShaderBindingTableInfo, bindingGroups, Test.rtPipeline.get(), &rtSetPtr, 1);

        const std::vector<uint8_t> tableData = Test.rtShaderBindingTableInfo.tableBufferData;
        Test.rtShaderBindingTableBuffer = Test.device->createBuffer(RenderBufferDesc::UploadBuffer(tableData.size(), RenderBufferFlag::SHADER_BINDING_TABLE));
        dstData = Test.rtShaderBindingTableBuffer->map();
        memcpy(dstData, tableData.data(), tableData.size());
        Test.rtShaderBindingTableBuffer->unmap();

        // Run command list to copy the vertex buffer and build the BLAS/TLAS.
        Test.commandList->begin();
        Test.commandList->barriers(RenderBarrierStage::COPY, RenderBufferBarrier(Test.rtVertexBuffer.get(), RenderBufferAccess::WRITE));
        Test.commandList->copyBufferRegion(Test.rtVertexBuffer.get(), Test.uploadBuffer.get(), sizeof(BLASVertices));
        Test.commandList->barriers(RenderBarrierStage::COMPUTE, RenderBufferBarrier(Test.rtVertexBuffer.get(), RenderBufferAccess::READ));
        Test.commandList->buildBottomLevelAS(Test.rtBottomLevelAS.get(), Test.rtScratchBuffer.get(), blasBuildInfo);
        Test.commandList->barriers(RenderBarrierStage::NONE, RenderBufferBarrier(Test.rtBottomLevelASBuffer.get(), RenderBufferAccess::READ));
        Test.commandList->buildTopLevelAS(Test.rtTopLevelAS.get(), Test.rtScratchBuffer.get(), Test.rtInstancesBuffer.get(), tlasBuildInfo);
        Test.commandList->end();
        Test.commandQueue->executeCommandLists(Test.commandList.get(), Test.commandFence.get());
        Test.commandQueue->waitForCommandFence(Test.commandFence.get());
#   endif
    }

    void TestResize() {
        // Resize can be called during window creation by Windows.
        if (Test.swapChain == nullptr) {
            return;
        }

#   if ENABLE_SWAP_CHAIN
        Test.swapFramebuffers.clear();
        Test.swapChain->resize();
        Test.swapFramebuffers.resize(Test.swapChain->getTextureCount());
        for (uint32_t i = 0; i < Test.swapChain->getTextureCount(); i++) {
            const RenderTexture *curTex = Test.swapChain->getTexture(i);
            Test.swapFramebuffers[i] = Test.device->createFramebuffer(RenderFramebufferDesc{&curTex, 1});
        }
#   endif
#   if ENABLE_CLEAR
        Test.colorTargetMS = Test.device->createTexture(RenderTextureDesc::ColorTarget(Test.swapChain->getWidth(), Test.swapChain->getHeight(), Test.ColorFormat, RenderMultisampling(Test.MSAACount), nullptr));
        Test.colorTargetResolved = Test.device->createTexture(RenderTextureDesc::ColorTarget(Test.swapChain->getWidth(), Test.swapChain->getHeight(), Test.ColorFormat, 1, nullptr, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
        Test.depthTarget = Test.device->createTexture(RenderTextureDesc::DepthTarget(Test.swapChain->getWidth(), Test.swapChain->getHeight(), Test.DepthFormat, RenderMultisampling(Test.MSAACount)));

        const RenderTexture *colorTargetPtr = Test.colorTargetMS.get();
        Test.framebuffer = Test.device->createFramebuffer(RenderFramebufferDesc(&colorTargetPtr, 1, Test.depthTarget.get()));
#   endif
#   if ENABLE_COMPUTE
        Test.computeSecondSet->setTexture(Test.computeSecondSet->gTarget, Test.colorTarget.get(), RenderTextureLayout::GENERAL);
#   endif
#   if ENABLE_RT
        Test.rtSet->setTexture(Test.rtSet->gOutput, Test.colorTarget.get(), RenderTextureLayout::GENERAL);
#   endif
    }
    
    void TestDraw() {
        // Begin.
        Test.commandList->begin();
#   if ENABLE_CLEAR
        const uint32_t SyncInterval = 1;
        const uint32_t width = Test.swapChain->getWidth();
        const uint32_t height = Test.swapChain->getHeight();
        const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
        const RenderRect scissor(0, 0, width, height);
        
        // Clear.
        Test.commandList->setViewports(viewport);
        Test.commandList->setScissors(scissor);
        Test.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(Test.colorTargetMS.get(), RenderTextureLayout::COLOR_WRITE));
        Test.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(Test.depthTarget.get(), RenderTextureLayout::DEPTH_WRITE));
        Test.commandList->setFramebuffer(Test.framebuffer.get());
        Test.commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.5f));
        Test.commandList->clearDepth();
#   endif
#   if ENABLE_RASTER
        // Raster.
        RasterPushConstant pushConstant;
        pushConstant.colorAdd[0] = 0.5f;
        pushConstant.colorAdd[1] = 0.25f;
        pushConstant.colorAdd[2] = 0.0f;
        pushConstant.colorAdd[3] = 0.0f;
        pushConstant.textureIndex = 2;
        Test.commandList->setPipeline(Test.rasterPipeline.get());
        Test.commandList->setGraphicsPipelineLayout(Test.rasterPipelineLayout.get());
        Test.commandList->setGraphicsPushConstants(0, &pushConstant);
#   if ENABLE_TEXTURE
        Test.commandList->setGraphicsDescriptorSet(Test.rasterSet->get(), 0);
#   endif
        Test.commandList->setVertexBuffers(0, &Test.vertexBufferView, 1, &Test.inputSlot);
        Test.commandList->setIndexBuffer(&Test.indexBufferView);
        Test.commandList->drawInstanced(3, 1, 0, 0);
        Test.commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(Test.depthTarget.get(), RenderTextureLayout::DEPTH_READ));
#   endif
#   if ENABLE_COMPUTE || ENABLE_RT || ENABLE_CLEAR
        Test.commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(Test.colorTargetMS.get(), RenderTextureLayout::RESOLVE_SOURCE));
        Test.commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(Test.colorTargetResolved.get(), RenderTextureLayout::RESOLVE_DEST));
        Test.commandList->resolveTexture(Test.colorTargetResolved.get(), Test.colorTargetMS.get());
#   endif
#   if ENABLE_COMPUTE || ENABLE_RT
        Test.commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(Test.colorTargetResolved.get(), RenderTextureLayout::GENERAL));
#   endif
#   if ENABLE_COMPUTE
        const uint32_t GroupCount = 8;
        ComputePushConstant pushCostant;
        pushCostant.Resolution[0] = width;
        pushCostant.Resolution[1] = height;
        pushCostant.Multiply[0] = 0.5f;
        pushCostant.Multiply[1] = 0.5f;
        pushCostant.Multiply[2] = 1.0f;
        pushCostant.Multiply[3] = 1.0f;
        Test.commandList->setPipeline(Test.computePipeline.get());
        Test.commandList->setComputePipelineLayout(Test.computePipelineLayout.get());
        Test.commandList->setComputePushConstants(0, &pushCostant);
        Test.commandList->setComputeDescriptorSet(Test.computeFirstSet->get(), 0);
        Test.commandList->setComputeDescriptorSet(Test.computeSecondSet->get(), 1);
        Test.commandList->dispatch((width + GroupCount - 1) / GroupCount, (height + GroupCount - 1) / GroupCount, 1);
#   endif
#   if ENABLE_RT
        // RT.
        Test.commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(Test.colorTargetResolved.get(), RenderTextureLayout::GENERAL));
        Test.commandList->setPipeline(Test.rtPipeline.get());
        Test.commandList->setRaytracingPipelineLayout(Test.rtPipelineLayout.get());
        Test.commandList->setRaytracingDescriptorSet(Test.rtSet->get(), 0);
        Test.commandList->traceRays(width, height, 1, Test.rtShaderBindingTableBuffer.get(), Test.rtShaderBindingTableInfo.groups);
#   endif
#   if ENABLE_SWAP_CHAIN
        const uint32_t swapChainTextureIndex = Test.swapChain->getTextureIndex();
        RenderTexture *swapChainTexture = Test.swapChain->getTexture(swapChainTextureIndex);
        RenderFramebuffer *swapFramebuffer = Test.swapFramebuffers[swapChainTextureIndex].get();
        Test.commandList->setViewports(viewport);
        Test.commandList->setScissors(scissor);
        Test.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
        Test.commandList->setFramebuffer(swapFramebuffer);
#   endif
#   if ENABLE_CLEAR
        Test.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(Test.colorTargetResolved.get(), RenderTextureLayout::SHADER_READ));
        Test.commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.0f));
        Test.commandList->setPipeline(Test.postPipeline.get());
        Test.commandList->setGraphicsPipelineLayout(Test.postPipelineLayout.get());
        Test.postSet->setTexture(0, Test.colorTargetResolved.get(), RenderTextureLayout::SHADER_READ);
        Test.commandList->setGraphicsDescriptorSet(Test.postSet.get(), 0);
        Test.commandList->drawInstanced(3, 1, 0, 0);
#   endif
#   if ENABLE_SWAP_CHAIN
        Test.commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));
#   endif
        // End.
        Test.commandList->end();
        Test.commandQueue->executeCommandLists(Test.commandList.get(), Test.commandFence.get());
#   if ENABLE_SWAP_CHAIN
        Test.swapChain->present();
#   endif
        Test.commandQueue->waitForCommandFence(Test.commandFence.get());
    }
    
    void TestShutdown() {
        Test.rtParamsBuffer.reset(nullptr);
        Test.rtVertexBuffer.reset(nullptr);
        Test.rtScratchBuffer.reset(nullptr);
        Test.rtInstancesBuffer.reset(nullptr);
        Test.rtBottomLevelASBuffer.reset(nullptr);
        Test.rtTopLevelASBuffer.reset(nullptr);
        Test.rtShaderBindingTableBuffer.reset(nullptr);
        Test.uploadBuffer.reset(nullptr);
        Test.blueNoiseTexture.reset(nullptr);
        Test.vertexBuffer.reset(nullptr);
        Test.indexBuffer.reset(nullptr);
        Test.rasterPipeline.reset(nullptr);
        Test.computePipeline.reset(nullptr);
        Test.rtPipeline.reset(nullptr);
        Test.postPipeline.reset(nullptr);
        Test.rasterPipelineLayout.reset(nullptr);
        Test.computePipelineLayout.reset(nullptr);
        Test.rtPipelineLayout.reset(nullptr);
        Test.postPipelineLayout.reset(nullptr);
        Test.rtSet.reset(nullptr);
        Test.rasterSet.reset(nullptr);
        Test.computeFirstSet.reset(nullptr);
        Test.computeSecondSet.reset(nullptr);
        Test.postSet.reset(nullptr);
        Test.linearSampler.reset(nullptr);
        Test.postSampler.reset(nullptr);
        Test.colorTargetMS.reset(nullptr);
        Test.colorTargetResolved.reset(nullptr);
        Test.framebuffer.reset(nullptr);
        Test.swapFramebuffers.clear();
        Test.commandList.reset(nullptr);
        Test.commandFence.reset(nullptr);
        Test.swapChain.reset(nullptr);
        Test.commandQueue.reset(nullptr);
        Test.device.reset(nullptr);
    }

#if defined(_WIN64)
    static LRESULT CALLBACK TestWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_SIZE:
            TestResize();
            return 0;
        case WM_PAINT:
            TestDraw();
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        return 0;
    }

    static HWND TestCreateWindow() {
        // Register window class.
        WNDCLASS wc;
        memset(&wc, 0, sizeof(WNDCLASS));
        wc.lpfnWndProc = TestWndProc;
        wc.hInstance = GetModuleHandle(0);
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
        wc.lpszClassName = "RenderInterfaceTest";
        RegisterClass(&wc);

        // Create window.
        const int Width = 1280;
        const int Height = 720;
        RECT rect;
        UINT dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        rect.left = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
        rect.top = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
        rect.right = rect.left + Width;
        rect.bottom = rect.top + Height;
        AdjustWindowRectEx(&rect, dwStyle, 0, 0);

        return CreateWindow(wc.lpszClassName, "Render Interface Test", dwStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0, 0, wc.hInstance, NULL);
    }

    void RenderInterfaceTest(RenderInterface *renderInterface) {
        HWND hwnd = TestCreateWindow();
        TestInitialize(renderInterface, hwnd);
        TestResize();

        // Message loop.
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        TestShutdown();
        DestroyWindow(hwnd);
    }
#elif defined(__ANDROID__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        assert(false);
    }
#elif defined(__linux__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        Display* display = XOpenDisplay(nullptr);
        int blackColor = BlackPixel(display, DefaultScreen(display));
        Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 
            0, 0, 1280, 720, 0, blackColor, blackColor);
        

          XSelectInput(display, window, StructureNotifyMask);
        // Map the window and wait for the notify event to come in.
        XMapWindow(display, window);
        while (true) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == MapNotify) {
                break;
            }
        }

        // Set up the delete window protocol.
        Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDeleteMessage, 1);

        TestInitialize(renderInterface, {display, window});
        TestResize();
        TestDraw();

        // Loop until the window is closed.
        std::chrono::system_clock::time_point prev_frame = std::chrono::system_clock::now();
        bool running = true;
        while (running) {
            if (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);

                switch (event.type) {
                    case Expose:
                        TestDraw();
                        break;

                    case ClientMessage:
                        if (event.xclient.data.l[0] == wmDeleteMessage)
                            running = false;
                        break;

                    default:
                        break;
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            auto now_time = std::chrono::system_clock::now();
            if (now_time - prev_frame > 16666us) {
                prev_frame = now_time;
                TestDraw();
            }
        }

        TestShutdown();
        XDestroyWindow(display, window);
    }
#elif defined(__APPLE__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        assert(false);
    }
#endif
};