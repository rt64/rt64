//
// RT64
//

#include "rhi/rt64_render_interface.h"

#include <cassert>
#include <cstring>
#include <chrono>
#include <functional>
#include <SDL.h>
#include <SDL_syswm.h>
#include <thread>
#include <random>

#ifdef _WIN64
#include "shaders/RenderInterfaceTestAsyncCS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestColorPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestDecalPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestTextureBindfulPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestTextureBindlessPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestRT.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestVS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestCS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.dxil.h"
#endif
#include "shaders/RenderInterfaceTestAsyncCS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestColorPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestDecalPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestTextureBindfulPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestTextureBindlessPS.hlsl.spirv.h"
#ifndef __APPLE__
#include "shaders/RenderInterfaceTestRT.hlsl.spirv.h"
#endif
#include "shaders/RenderInterfaceTestVS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestCS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestSpecPS.hlsl.spirv.h"
#ifdef __APPLE__
#include "shaders/RenderInterfaceTestAsyncCS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestColorPS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestDecalPS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestTextureBindfulPS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestTextureBindlessPS.hlsl.metal.h"
// TODO: Enable when RT is added to Metal.
//#include "shaders/RenderInterfaceTestRT.hlsl.metal.h"
#include "shaders/RenderInterfaceTestVS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestCS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestSpecPS.hlsl.metal.h"
#endif

namespace RT64 {
    static const uint32_t BufferCount = 2;
    static const RenderFormat SwapchainFormat = RenderFormat::B8G8R8A8_UNORM;
    static const uint32_t MSAACount = 4;
    static const RenderFormat ColorFormat = RenderFormat::R8G8B8A8_UNORM;
    static const RenderFormat DepthFormat = RenderFormat::D32_FLOAT;

    struct CheckeredTextureGenerator {
        static std::vector<uint8_t> generateCheckeredData(uint32_t width, uint32_t height) {
            std::vector<uint8_t> textureData(width * height * 4);
            const uint32_t squareSize = 32; // Size of each checker square

            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    uint32_t index = (y * width + x) * 4;
                    bool isWhite = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                    uint8_t pixelValue = isWhite ? 255 : 0;

                    textureData[index + 0] = pixelValue;  // R
                    textureData[index + 1] = pixelValue;  // G
                    textureData[index + 2] = pixelValue;  // B
                    textureData[index + 3] = 255;         // A
                }
            }

            return textureData;
        }
    };

    struct StripedTextureGenerator {
        static std::vector<uint8_t> generateStripedData(uint32_t width, uint32_t height) {
            std::vector<uint8_t> textureData(width * height * 4);
            const uint32_t stripeSize = 32;

            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    uint32_t index = (y * width + x) * 4;
                    bool isWhite = ((x + y) / stripeSize) % 2 == 0;
                    uint8_t pixelValue = isWhite ? 255 : 0;

                    textureData[index + 0] = pixelValue;  // R
                    textureData[index + 1] = pixelValue;  // G
                    textureData[index + 2] = pixelValue;  // B
                    textureData[index + 3] = 255;         // A
                }
            }

            return textureData;
        }
    };

    struct NoiseTextureGenerator {
        static std::vector<uint8_t> generateNoiseData(uint32_t width, uint32_t height) {
            std::vector<uint8_t> textureData(width * height * 4);
            std::srand(std::time(nullptr));
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    uint32_t index = (y * width + x) * 4;
                    textureData[index + 0] = std::rand() % 255; // R
                    textureData[index + 1] = std::rand() % 255; // G
                    textureData[index + 2] = std::rand() % 255; // B
                    textureData[index + 3] = 255;               // A
                }
            }

            return textureData;
        }
    };

    struct RasterTextureBindfulDescriptorSet : RenderDescriptorSetBase {
        uint32_t gSampler;
        uint32_t gTexture;

        RasterTextureBindfulDescriptorSet(RenderDevice *device, RenderSampler *linearSampler) {
            builder.begin();
            gSampler = builder.addImmutableSampler(1, linearSampler);
            gTexture = builder.addTexture(2);
            builder.end(true);

            create(device);
        }
    };

    struct RasterTextureBindlessDescriptorSet : RenderDescriptorSetBase {
        uint32_t gSampler;
        uint32_t gTextures;

        RasterTextureBindlessDescriptorSet(RenderDevice *device, RenderSampler *linearSampler, uint32_t textureArraySize) {
            const uint32_t TextureArrayUpperRange = 8192;
            builder.begin();
            gSampler = builder.addImmutableSampler(1, linearSampler);
            gTextures = builder.addTexture(2, TextureArrayUpperRange);
            builder.end(true, textureArraySize);

            create(device);
        }
    };

    struct ComputeDescriptorFirstSet : RenderDescriptorSetBase {
        uint32_t gBlueNoiseTexture;
        uint32_t gSampler;
        uint32_t gTarget;

        ComputeDescriptorFirstSet(RenderDevice *device, RenderSampler *linearSampler) {
            builder.begin();
            gBlueNoiseTexture = builder.addTexture(1);
            gSampler = builder.addImmutableSampler(2, linearSampler);
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

    struct RasterPushConstant {
        float colorAdd[4] = {};
        uint32_t textureIndex = 0;
    };

    struct ComputePushConstant {
        float Multiply[4] = {};
        uint32_t Resolution[2] = {};
    };

    enum class ShaderType {
        VERTEX,
        COLOR_PIXEL,
        DECAL_PIXEL,
        TEXTURE_BINDFUL_PIXEL,
        TEXTURE_BINDLESS_PIXEL,
        COMPUTE,
        ASYNC_COMPUTE,
#ifndef __APPLE__
        RAY_TRACE,
#endif
        POST_VERTEX,
        POST_PIXEL,
        SPEC_PIXEL
    };

    struct ShaderData {
        const void* blob;
        uint64_t size;
        RenderShaderFormat format;
    };

    struct TestContext {
        const RenderInterface *renderInterface = nullptr;
        RenderWindow window;
        uint32_t swapChainTextureIndex = 0;
        std::unique_ptr<RenderDevice> device;
        std::unique_ptr<RenderCommandQueue> commandQueue;
        std::unique_ptr<RenderCommandList> commandList;
        std::unique_ptr<RenderCommandSemaphore> acquireSemaphore;
        std::unique_ptr<RenderCommandSemaphore> drawSemaphore;
        std::unique_ptr<RenderCommandFence> commandFence;
        std::unique_ptr<RenderSwapChain> swapChain;
        std::unique_ptr<RenderFramebuffer> framebuffer;
        std::unique_ptr<RenderFramebuffer> framebufferDepthRead;
        std::vector<std::unique_ptr<RenderFramebuffer>> swapFramebuffers;
        std::unique_ptr<RenderSampler> linearSampler;
        std::unique_ptr<RenderSampler> postSampler;
        std::unique_ptr<RasterTextureBindfulDescriptorSet> rasterTextureStripedSet;
        std::unique_ptr<RasterTextureBindfulDescriptorSet> rasterTextureNoiseSet;
        std::unique_ptr<RasterTextureBindlessDescriptorSet> rasterTextureBindlessSet;
        std::unique_ptr<ComputeDescriptorFirstSet> computeFirstSet;
        std::unique_ptr<ComputeDescriptorSecondSet> computeSecondSet;
        std::unique_ptr<RaytracingDescriptorSet> rtSet;
        std::unique_ptr<RenderDescriptorSet> postSet;
        std::unique_ptr<RenderPipelineLayout> rasterColorPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> rasterTextureBindlessPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> rasterTextureBindfulPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> computePipelineLayout;
        std::unique_ptr<RenderPipelineLayout> rtPipelineLayout;
        std::unique_ptr<RenderPipelineLayout> postPipelineLayout;
        std::unique_ptr<RenderPipeline> rasterColorPipeline;
        std::unique_ptr<RenderPipeline> rasterTextureBindfulPipeline;
        std::unique_ptr<RenderPipeline> rasterTextureBindlessPipeline;
        std::unique_ptr<RenderPipeline> computePipeline;
        std::unique_ptr<RenderPipeline> rtPipeline;
        std::unique_ptr<RenderPipeline> postPipeline;
        std::unique_ptr<RenderTexture> colorTargetMS;
        std::unique_ptr<RenderTexture> colorTargetResolved;
        std::unique_ptr<RenderTexture> depthTarget;
        std::unique_ptr<RenderTextureView> depthTargetView;
        std::unique_ptr<RenderBuffer> uploadBuffer;
        std::unique_ptr<RenderTexture> checkTexture;
        std::unique_ptr<RenderTexture> stripedTexture;
        std::unique_ptr<RenderTexture> noiseTexture;
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
    };

    struct TestBase {
        virtual ~TestBase() {}
        virtual void initialize(TestContext& ctx) = 0;
        virtual void resize(TestContext& ctx) = 0;
        virtual void draw(TestContext& ctx) = 0;
        virtual void shutdown(TestContext& ctx) {
            ctx.rtParamsBuffer.reset(nullptr);
            ctx.rtVertexBuffer.reset(nullptr);
            ctx.rtScratchBuffer.reset(nullptr);
            ctx.rtInstancesBuffer.reset(nullptr);
            ctx.rtBottomLevelASBuffer.reset(nullptr);
            ctx.rtTopLevelASBuffer.reset(nullptr);
            ctx.rtShaderBindingTableBuffer.reset(nullptr);
            ctx.uploadBuffer.reset(nullptr);
            ctx.checkTexture.reset(nullptr);
            ctx.noiseTexture.reset(nullptr);
            ctx.vertexBuffer.reset(nullptr);
            ctx.indexBuffer.reset(nullptr);
            ctx.rasterColorPipeline.reset(nullptr);
            ctx.rasterTextureBindfulPipeline.reset(nullptr);
            ctx.rasterTextureBindlessPipeline.reset(nullptr);
            ctx.computePipeline.reset(nullptr);
            ctx.rtPipeline.reset(nullptr);
            ctx.postPipeline.reset(nullptr);
            ctx.rasterColorPipelineLayout.reset(nullptr);
            ctx.rasterTextureBindfulPipelineLayout.reset(nullptr);
            ctx.rasterTextureBindlessPipelineLayout.reset(nullptr);
            ctx.computePipelineLayout.reset(nullptr);
            ctx.rtPipelineLayout.reset(nullptr);
            ctx.postPipelineLayout.reset(nullptr);
            ctx.rtSet.reset(nullptr);
            ctx.rasterTextureStripedSet.reset(nullptr);
            ctx.rasterTextureNoiseSet.reset(nullptr);
            ctx.rasterTextureBindlessSet.reset(nullptr);
            ctx.computeFirstSet.reset(nullptr);
            ctx.computeSecondSet.reset(nullptr);
            ctx.postSet.reset(nullptr);
            ctx.linearSampler.reset(nullptr);
            ctx.postSampler.reset(nullptr);
            ctx.framebuffer.reset(nullptr);
            ctx.framebufferDepthRead.reset(nullptr);
            ctx.colorTargetMS.reset(nullptr);
            ctx.colorTargetResolved.reset(nullptr);
            ctx.depthTargetView.reset(nullptr);
            ctx.depthTarget.reset(nullptr);
            ctx.swapFramebuffers.clear();
            ctx.commandList.reset(nullptr);
            ctx.drawSemaphore.reset(nullptr);
            ctx.acquireSemaphore.reset(nullptr);
            ctx.commandFence.reset(nullptr);
            ctx.swapChain.reset(nullptr);
            ctx.commandQueue.reset(nullptr);
            ctx.device.reset(nullptr);
        }
    };

    // Common utilities
    static ShaderData getShaderData(RenderShaderFormat format, ShaderType type) {
        ShaderData data = {};
        data.format = format;

        switch (format) {
#       ifdef _WIN64
            case RenderShaderFormat::DXIL:
                switch (type) {
                    case ShaderType::VERTEX:
                        data.blob = RenderInterfaceTestVSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestVSBlobDXIL);
                        break;
                    case ShaderType::COLOR_PIXEL:
                        data.blob = RenderInterfaceTestColorPSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestColorPSBlobDXIL);
                        break;
                    case ShaderType::DECAL_PIXEL:
                        data.blob = RenderInterfaceTestDecalPSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestDecalPSBlobDXIL);
                        break;
                    case ShaderType::TEXTURE_BINDFUL_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindfulPSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestTextureBindfulPSBlobDXIL);
                        break;
                    case ShaderType::TEXTURE_BINDLESS_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindlessPSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestTextureBindlessPSBlobDXIL);
                        break;
                    case ShaderType::COMPUTE:
                        data.blob = RenderInterfaceTestCSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestCSBlobDXIL);
                        break;
                    case ShaderType::ASYNC_COMPUTE:
                        data.blob = RenderInterfaceTestAsyncCSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestAsyncCSBlobDXIL);
                        break;
                    case ShaderType::RAY_TRACE:
                        data.blob = RenderInterfaceTestRTBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestRTBlobDXIL);
                        break;
                    case ShaderType::POST_VERTEX:
                        data.blob = RenderInterfaceTestPostVSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestPostVSBlobDXIL);
                        break;
                    case ShaderType::POST_PIXEL:
                        data.blob = RenderInterfaceTestPostPSBlobDXIL;
                        data.size = sizeof(RenderInterfaceTestPostPSBlobDXIL);
                        break;
                    case ShaderType::SPEC_PIXEL:
                        assert(false && "Spec constants are not supported in DXIL.");
                        break;
                }
                break;
#       endif
            case RenderShaderFormat::SPIRV:
                switch (type) {
                    case ShaderType::VERTEX:
                        data.blob = RenderInterfaceTestVSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestVSBlobSPIRV);
                        break;
                    case ShaderType::COLOR_PIXEL:
                        data.blob = RenderInterfaceTestColorPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestColorPSBlobSPIRV);
                        break;
                    case ShaderType::DECAL_PIXEL:
                        data.blob = RenderInterfaceTestDecalPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestDecalPSBlobSPIRV);
                        break;
                    case ShaderType::TEXTURE_BINDFUL_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindfulPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestTextureBindfulPSBlobSPIRV);
                        break;
                    case ShaderType::TEXTURE_BINDLESS_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindlessPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestTextureBindlessPSBlobSPIRV);
                        break;
                    case ShaderType::COMPUTE:
                        data.blob = RenderInterfaceTestCSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestCSBlobSPIRV);
                        break;
                    case ShaderType::ASYNC_COMPUTE:
                        data.blob = RenderInterfaceTestAsyncCSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestAsyncCSBlobSPIRV);
                        break;
#               ifndef __APPLE__
                    case ShaderType::RAY_TRACE:
                        data.blob = RenderInterfaceTestRTBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestRTBlobSPIRV);
                        break;
#               endif
                    case ShaderType::POST_VERTEX:
                        data.blob = RenderInterfaceTestPostVSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestPostVSBlobSPIRV);
                        break;
                    case ShaderType::POST_PIXEL:
                        data.blob = RenderInterfaceTestPostPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestPostPSBlobSPIRV);
                        break;
                    case ShaderType::SPEC_PIXEL:
                        data.blob = RenderInterfaceTestSpecPSBlobSPIRV;
                        data.size = sizeof(RenderInterfaceTestSpecPSBlobSPIRV);
                        break;
                }
                break;
#       ifdef __APPLE__
            case RenderShaderFormat::METAL:
                switch (type) {
                    case ShaderType::VERTEX:
                        data.blob = RenderInterfaceTestVSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestVSBlobMSL);
                        break;
                    case ShaderType::COLOR_PIXEL:
                        data.blob = RenderInterfaceTestColorPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestColorPSBlobMSL);
                        break;
                    case ShaderType::DECAL_PIXEL:
                        data.blob = RenderInterfaceTestDecalPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestDecalPSBlobMSL);
                        break;
                    case ShaderType::TEXTURE_BINDFUL_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindfulPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestTextureBindfulPSBlobMSL);
                        break;
                    case ShaderType::TEXTURE_BINDLESS_PIXEL:
                        data.blob = RenderInterfaceTestTextureBindlessPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestTextureBindlessPSBlobMSL);
                        break;
                    case ShaderType::COMPUTE:
                        data.blob = RenderInterfaceTestCSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestCSBlobMSL);
                        break;
                    case ShaderType::ASYNC_COMPUTE:
                        data.blob = RenderInterfaceTestAsyncCSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestAsyncCSBlobMSL);
                        break;
                    case ShaderType::POST_VERTEX:
                        data.blob = RenderInterfaceTestPostVSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestPostVSBlobMSL);
                        break;
                    case ShaderType::POST_PIXEL:
                        data.blob = RenderInterfaceTestPostPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestPostPSBlobMSL);
                        break;
                    case ShaderType::SPEC_PIXEL:
                        data.blob = RenderInterfaceTestSpecPSBlobMSL;
                        data.size = sizeof(RenderInterfaceTestSpecPSBlobMSL);
                        break;
                }
                break;
#       endif
            default:
                assert(false && "Unknown shader format.");
        }

        return data;
    }

    static void createContext(TestContext& ctx, RenderInterface* renderInterface, RenderWindow window) {
        ctx.renderInterface = renderInterface;
        ctx.window = window;
        ctx.device = renderInterface->createDevice();
        ctx.commandQueue = ctx.device->createCommandQueue(RenderCommandListType::DIRECT);
        ctx.commandList = ctx.commandQueue->createCommandList(RenderCommandListType::DIRECT);
        ctx.acquireSemaphore = ctx.device->createCommandSemaphore();
        ctx.drawSemaphore = ctx.device->createCommandSemaphore();
        ctx.commandFence = ctx.device->createCommandFence();
        ctx.swapChain = ctx.commandQueue->createSwapChain(window, BufferCount, SwapchainFormat);
        ctx.linearSampler = ctx.device->createSampler(RenderSamplerDesc());
    }

    static void createSwapChain(TestContext& ctx) {
        ctx.swapFramebuffers.clear();
        ctx.swapChain->resize();
        ctx.swapFramebuffers.resize(ctx.swapChain->getTextureCount());
        for (uint32_t i = 0; i < ctx.swapChain->getTextureCount(); i++) {
            const RenderTexture* curTex = ctx.swapChain->getTexture(i);
            ctx.swapFramebuffers[i] = ctx.device->createFramebuffer(RenderFramebufferDesc{&curTex, 1});
        }
    }

    static void createTargets(TestContext& ctx) {
        ctx.colorTargetMS = ctx.device->createTexture(RenderTextureDesc::ColorTarget(ctx.swapChain->getWidth(), ctx.swapChain->getHeight(), ColorFormat, RenderMultisampling(MSAACount), nullptr));
        ctx.colorTargetResolved = ctx.device->createTexture(RenderTextureDesc::ColorTarget(ctx.swapChain->getWidth(), ctx.swapChain->getHeight(), ColorFormat, 1, nullptr, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
        ctx.depthTarget = ctx.device->createTexture(RenderTextureDesc::DepthTarget(ctx.swapChain->getWidth(), ctx.swapChain->getHeight(), DepthFormat, RenderMultisampling(MSAACount)));
        ctx.depthTargetView = ctx.depthTarget->createTextureView(RenderTextureViewDesc::Texture2D(RenderFormat::D32_FLOAT));

        const RenderTexture *colorTargetPtr = ctx.colorTargetMS.get();
        ctx.framebuffer = ctx.device->createFramebuffer(RenderFramebufferDesc(&colorTargetPtr, 1, ctx.depthTarget.get()));
        ctx.framebufferDepthRead = ctx.device->createFramebuffer(RenderFramebufferDesc(&colorTargetPtr, 1, ctx.depthTarget.get(), true));
    }

    static void createRasterColorShader(TestContext &ctx) {
        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, true);
        layoutBuilder.addPushConstant(0, 0, sizeof(RasterPushConstant), RenderShaderStageFlag::PIXEL);
        layoutBuilder.end();

        ctx.rasterColorPipelineLayout = layoutBuilder.create(ctx.device.get());

        // Pick shader format depending on the render interface's requirements.
        const RenderInterfaceCapabilities &interfaceCapabilities = ctx.renderInterface->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;

        ShaderData psData = getShaderData(shaderFormat, ShaderType::COLOR_PIXEL);
        ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

        const uint32_t FloatsPerVertex = 4;

        ctx.inputSlot = RenderInputSlot(0, sizeof(float) * FloatsPerVertex);

        std::vector<RenderInputElement> inputElements;
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
        inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

        std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
        std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);

        RenderGraphicsPipelineDesc graphicsDesc;
        graphicsDesc.inputSlots = &ctx.inputSlot;
        graphicsDesc.inputSlotsCount = 1;
        graphicsDesc.inputElements = inputElements.data();
        graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
        graphicsDesc.pipelineLayout = ctx.rasterColorPipelineLayout.get();
        graphicsDesc.pixelShader = pixelShader.get();
        graphicsDesc.vertexShader = vertexShader.get();
        graphicsDesc.renderTargetFormat[0] = ColorFormat;
        graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        graphicsDesc.depthEnabled = true;
        graphicsDesc.depthFunction = RenderComparisonFunction::LESS_EQUAL;
        graphicsDesc.depthWriteEnabled = true;
        graphicsDesc.depthTargetFormat = DepthFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.multisampling.sampleCount = MSAACount;
        ctx.rasterColorPipeline = ctx.device->createGraphicsPipeline(graphicsDesc);
    }

    static void createRasterTextureBindfulShader(TestContext& ctx) {
        ctx.rasterTextureNoiseSet = std::make_unique<RasterTextureBindfulDescriptorSet>(ctx.device.get(), ctx.linearSampler.get());
        ctx.rasterTextureStripedSet = std::make_unique<RasterTextureBindfulDescriptorSet>(ctx.device.get(), ctx.linearSampler.get());

        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, true);
        layoutBuilder.addPushConstant(0, 0, sizeof(RasterPushConstant) - sizeof(uint32_t), RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(ctx.rasterTextureNoiseSet->builder);
        layoutBuilder.end();

        ctx.rasterTextureBindfulPipelineLayout = layoutBuilder.create(ctx.device.get());

        // Pick shader format depending on the render interface's requirements.
        const RenderInterfaceCapabilities &interfaceCapabilities = ctx.renderInterface->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;

        ShaderData psData = getShaderData(shaderFormat, ShaderType::TEXTURE_BINDFUL_PIXEL);
        ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

        const uint32_t FloatsPerVertex = 4;
        ctx.inputSlot = RenderInputSlot(0, sizeof(float) * FloatsPerVertex);

        std::vector<RenderInputElement> inputElements;
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
        inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

        std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
        std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);
        
        RenderGraphicsPipelineDesc graphicsDesc;
        graphicsDesc.inputSlots = &ctx.inputSlot;
        graphicsDesc.inputSlotsCount = 1;
        graphicsDesc.inputElements = inputElements.data();
        graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
        graphicsDesc.pipelineLayout = ctx.rasterTextureBindfulPipelineLayout.get();
        graphicsDesc.pixelShader = pixelShader.get();
        graphicsDesc.vertexShader = vertexShader.get();
        graphicsDesc.renderTargetFormat[0] = ColorFormat;
        graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        graphicsDesc.depthTargetFormat = DepthFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.multisampling.sampleCount = MSAACount;
        ctx.rasterTextureBindfulPipeline = ctx.device->createGraphicsPipeline(graphicsDesc);
    }

    static void createRasterTextureBindlessShader(TestContext& ctx) {
        ctx.rasterTextureBindlessSet = std::make_unique<RasterTextureBindlessDescriptorSet>(ctx.device.get(), ctx.linearSampler.get(), 4);

        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, true);
        layoutBuilder.addPushConstant(0, 0, sizeof(RasterPushConstant), RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(ctx.rasterTextureBindlessSet->builder);
        layoutBuilder.end();

        ctx.rasterTextureBindlessPipelineLayout = layoutBuilder.create(ctx.device.get());

        // Pick shader format depending on the render interface's requirements.
        const RenderInterfaceCapabilities &interfaceCapabilities = ctx.renderInterface->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;

        ShaderData psData = getShaderData(shaderFormat, ShaderType::TEXTURE_BINDLESS_PIXEL);
        ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

        const uint32_t FloatsPerVertex = 4;

        ctx.inputSlot = RenderInputSlot(0, sizeof(float) * FloatsPerVertex);

        std::vector<RenderInputElement> inputElements;
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
        inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

        std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
        std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);

        RenderGraphicsPipelineDesc graphicsDesc;
        graphicsDesc.inputSlots = &ctx.inputSlot;
        graphicsDesc.inputSlotsCount = 1;
        graphicsDesc.inputElements = inputElements.data();
        graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
        graphicsDesc.pipelineLayout = ctx.rasterTextureBindlessPipelineLayout.get();
        graphicsDesc.pixelShader = pixelShader.get();
        graphicsDesc.vertexShader = vertexShader.get();
        graphicsDesc.renderTargetFormat[0] = ColorFormat;
        graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        graphicsDesc.depthTargetFormat = DepthFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.multisampling.sampleCount = MSAACount;
        ctx.rasterTextureBindlessPipeline = ctx.device->createGraphicsPipeline(graphicsDesc);
    }

    static void createPostPipeline(TestContext &ctx) {
        ctx.postSampler = ctx.device->createSampler(RenderSamplerDesc());
        const RenderSampler *postSamplerPtr = ctx.postSampler.get();
        std::vector<RenderDescriptorRange> postDescriptorRanges = {
           RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 1, 1),
           RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 2, 1, &postSamplerPtr)
        };

        RenderDescriptorSetDesc postDescriptorSetDesc(postDescriptorRanges.data(), uint32_t(postDescriptorRanges.size()));
        ctx.postSet = ctx.device->createDescriptorSet(postDescriptorSetDesc);
        ctx.postPipelineLayout = ctx.device->createPipelineLayout(RenderPipelineLayoutDesc(nullptr, 0, &postDescriptorSetDesc, 1, false, true));

        std::vector<RenderInputElement> inputElements;
        inputElements.clear();
        inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));

        // Pick shader format depending on the render interface's requirements.
        const RenderInterfaceCapabilities &interfaceCapabilities = ctx.renderInterface->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;

        ShaderData postPsData = getShaderData(shaderFormat, ShaderType::POST_PIXEL);
        ShaderData postVsData = getShaderData(shaderFormat, ShaderType::POST_VERTEX);

        std::unique_ptr<RenderShader> postPixelShader = ctx.device->createShader(postPsData.blob, postPsData.size, "PSMain", postPsData.format);
        std::unique_ptr<RenderShader> postVertexShader = ctx.device->createShader(postVsData.blob, postVsData.size, "VSMain", postVsData.format);

        RenderGraphicsPipelineDesc postDesc;
        postDesc.inputSlots = nullptr;
        postDesc.inputSlotsCount = 0;
        postDesc.inputElements = nullptr;
        postDesc.inputElementsCount = 0;
        postDesc.pipelineLayout = ctx.postPipelineLayout.get();
        postDesc.pixelShader = postPixelShader.get();
        postDesc.vertexShader = postVertexShader.get();
        postDesc.renderTargetFormat[0] = SwapchainFormat;
        postDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        postDesc.renderTargetCount = 1;
        ctx.postPipeline = ctx.device->createGraphicsPipeline(postDesc);
    }

    static void uploadTexture(TestContext& ctx, uint32_t textureWidth, uint32_t textureHeight, const std::vector<uint8_t> textureData, std::unique_ptr<RenderTexture> &dstTexture) {
        // Upload a texture.
        const uint32_t RowLength = textureWidth;
        const RenderFormat Format = RenderFormat::R8G8B8A8_UNORM;
        const uint32_t BufferSize = RowLength * textureHeight * RenderFormatSize(Format);
        ctx.uploadBuffer = ctx.device->createBuffer(RenderBufferDesc::UploadBuffer(BufferSize));
        dstTexture = ctx.device->createTexture(RenderTextureDesc::Texture2D(textureWidth, textureHeight, 1, Format));

        // Copy to upload buffer.
        void *bufferData = ctx.uploadBuffer->map();
        memcpy(bufferData, textureData.data(), BufferSize);
        ctx.uploadBuffer->unmap();

        // Run command list to copy the upload buffer to the texture.
        ctx.commandList->begin();
        ctx.commandList->barriers(RenderBarrierStage::COPY,
                                   RenderBufferBarrier(ctx.uploadBuffer.get(), RenderBufferAccess::READ),
                                   RenderTextureBarrier(dstTexture.get(), RenderTextureLayout::COPY_DEST)
                                   );

        ctx.commandList->copyTextureRegion(
                                            RenderTextureCopyLocation::Subresource(dstTexture.get()),
                                            RenderTextureCopyLocation::PlacedFootprint(ctx.uploadBuffer.get(), Format, textureWidth, textureHeight, 1, RowLength));

        ctx.commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, RenderTextureBarrier(dstTexture.get(), RenderTextureLayout::SHADER_READ));
        ctx.commandList->end();
        ctx.commandQueue->executeCommandLists(ctx.commandList.get(), ctx.commandFence.get());
        ctx.commandQueue->waitForCommandFence(ctx.commandFence.get());
    }

    static void uploadCheckTexture(TestContext& ctx) {
        auto textureData = CheckeredTextureGenerator::generateCheckeredData(512, 512);
        uploadTexture(ctx, 512, 512, textureData, ctx.checkTexture);
        ctx.rasterTextureBindlessSet->setTexture(ctx.rasterTextureBindlessSet->gTextures + 2, ctx.checkTexture.get(), RenderTextureLayout::SHADER_READ);
    }

    static void uploadNoiseTexture(TestContext& ctx) {
        auto textureData = NoiseTextureGenerator::generateNoiseData(512, 512);
        uploadTexture(ctx, 512, 512, textureData, ctx.noiseTexture);
        ctx.rasterTextureBindlessSet->setTexture(ctx.rasterTextureBindlessSet->gTextures + 3, ctx.noiseTexture.get(), RenderTextureLayout::SHADER_READ);
        ctx.rasterTextureNoiseSet->setTexture(ctx.rasterTextureNoiseSet->gTexture, ctx.noiseTexture.get(), RenderTextureLayout::SHADER_READ);
    }

    static void uploadStripedTexture(TestContext& ctx) {
        auto textureData = StripedTextureGenerator::generateStripedData(512, 512);
        uploadTexture(ctx, 512, 512, textureData, ctx.stripedTexture);
        ctx.rasterTextureStripedSet->setTexture(ctx.rasterTextureStripedSet->gTexture, ctx.stripedTexture.get(), RenderTextureLayout::SHADER_READ);
    }

    static void createVertexBuffer(TestContext& ctx) {
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

        ctx.vertexBuffer = ctx.device->createBuffer(RenderBufferDesc::VertexBuffer(sizeof(Vertices), RenderHeapType::UPLOAD));
        void *dstData = ctx.vertexBuffer->map();
        memcpy(dstData, Vertices, sizeof(Vertices));
        ctx.vertexBuffer->unmap();
        ctx.vertexBufferView = RenderVertexBufferView(ctx.vertexBuffer.get(), sizeof(Vertices));

        ctx.indexBuffer = ctx.device->createBuffer(RenderBufferDesc::IndexBuffer(sizeof(Indices), RenderHeapType::UPLOAD));
        dstData = ctx.indexBuffer->map();
        memcpy(dstData, Indices, sizeof(Indices));
        ctx.indexBuffer->unmap();
        ctx.indexBufferView = RenderIndexBufferView(ctx.indexBuffer.get(), sizeof(Indices), RenderFormat::R32_UINT);
    }

    static void createComputePipeline(TestContext& ctx) {
        ctx.computeFirstSet = std::make_unique<ComputeDescriptorFirstSet>(ctx.device.get(), ctx.linearSampler.get());
        ctx.computeSecondSet = std::make_unique<ComputeDescriptorSecondSet>(ctx.device.get());
        ctx.computeFirstSet->setTexture(ctx.computeFirstSet->gBlueNoiseTexture, ctx.checkTexture.get(), RenderTextureLayout::SHADER_READ);

        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin();
        layoutBuilder.addPushConstant(0, 0, sizeof(ComputePushConstant), RenderShaderStageFlag::COMPUTE);
        layoutBuilder.addDescriptorSet(ctx.computeFirstSet->builder);
        layoutBuilder.addDescriptorSet(ctx.computeSecondSet->builder);
        layoutBuilder.end();

        ctx.computePipelineLayout = layoutBuilder.create(ctx.device.get());

        ShaderData computeData = getShaderData(ctx.renderInterface->getCapabilities().shaderFormat, ShaderType::COMPUTE);
        std::unique_ptr<RenderShader> computeShader = ctx.device->createShader(computeData.blob, computeData.size, "CSMain", computeData.format);
        RenderComputePipelineDesc computeDesc(ctx.computePipelineLayout.get(), computeShader.get(), 8, 8, 1);
        ctx.computePipeline = ctx.device->createComputePipeline(computeDesc);
    }

    static void presentSwapChain(TestContext& ctx) {
        const RenderCommandList* cmdList = ctx.commandList.get();
        RenderCommandSemaphore* waitSemaphore = ctx.acquireSemaphore.get();
        RenderCommandSemaphore* signalSemaphore = ctx.drawSemaphore.get();

        ctx.commandQueue->executeCommandLists(&cmdList, 1, &waitSemaphore, 1, &signalSemaphore, 1, ctx.commandFence.get());
        ctx.swapChain->present(ctx.swapChainTextureIndex, &signalSemaphore, 1);
        ctx.commandQueue->waitForCommandFence(ctx.commandFence.get());
    }

    static void initializeRenderTargets(TestContext& ctx) {
        const uint32_t width = ctx.swapChain->getWidth();
        const uint32_t height = ctx.swapChain->getHeight();
        const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
        const RenderRect scissor(0, 0, width, height);

        ctx.commandList->setViewports(viewport);
        ctx.commandList->setScissors(scissor);
        ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(ctx.colorTargetMS.get(), RenderTextureLayout::COLOR_WRITE));
        ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(ctx.depthTarget.get(), RenderTextureLayout::DEPTH_WRITE));
        ctx.commandList->setFramebuffer(ctx.framebuffer.get());

        // Clear full screen
        ctx.commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.5f)); // Clear to blue

        // Clear with rects
        const RenderRect clearRects[] = {
            RenderRect(0, 0, 100, 100),
            RenderRect(200, 200, 300, 300),
            RenderRect(400, 400, 500, 500)
        };

        ctx.commandList->clearColor(0, RenderColor(0.0f, 1.0f, 0.5f), clearRects, std::size(clearRects)); // Clear to green

        // Clear full depth buffer
        ctx.commandList->clearDepth();  // Clear to 1.0f

        // Clear depth buffer with rects
        ctx.commandList->clearDepth(true, 0, clearRects, std::size(clearRects));
    }

    static void resolveMultisampledTexture(TestContext& ctx) {
        ctx.commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(ctx.colorTargetMS.get(), RenderTextureLayout::RESOLVE_SOURCE));
        ctx.commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(ctx.colorTargetResolved.get(), RenderTextureLayout::RESOLVE_DEST));
        ctx.commandList->resolveTexture(ctx.colorTargetResolved.get(), ctx.colorTargetMS.get());
    }

    static void applyPostProcessToSwapChain(TestContext& ctx) {
        const uint32_t width = ctx.swapChain->getWidth();
        const uint32_t height = ctx.swapChain->getHeight();
        const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
        const RenderRect scissor(0, 0, width, height);

        ctx.swapChain->acquireTexture(ctx.acquireSemaphore.get(), &ctx.swapChainTextureIndex);
        RenderTexture *swapChainTexture = ctx.swapChain->getTexture(ctx.swapChainTextureIndex);
        RenderFramebuffer *swapFramebuffer = ctx.swapFramebuffers[ctx.swapChainTextureIndex].get();
        ctx.commandList->setViewports(viewport);
        ctx.commandList->setScissors(scissor);
        ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
        ctx.commandList->setFramebuffer(swapFramebuffer);

        ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(ctx.colorTargetResolved.get(), RenderTextureLayout::SHADER_READ));
        ctx.commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.0f));
        ctx.commandList->setPipeline(ctx.postPipeline.get());
        ctx.commandList->setGraphicsPipelineLayout(ctx.postPipelineLayout.get());
        ctx.postSet->setTexture(0, ctx.colorTargetResolved.get(), RenderTextureLayout::SHADER_READ);
        ctx.commandList->setGraphicsDescriptorSet(ctx.postSet.get(), 0);
        ctx.commandList->drawInstanced(3, 1, 0, 0);
        ctx.commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));
    }

    static void setupRasterColorPipeline(TestContext& ctx) {
        ctx.commandList->setPipeline(ctx.rasterColorPipeline.get());
        ctx.commandList->setGraphicsPipelineLayout(ctx.rasterColorPipelineLayout.get());
    }

    static void setupRasterTextureBindlessPipeline(TestContext &ctx) {
        RasterPushConstant pushConstant;
        pushConstant.colorAdd[0] = 0.5f;
        pushConstant.colorAdd[1] = 0.25f;
        pushConstant.colorAdd[2] = 0.0f;
        pushConstant.colorAdd[3] = 0.0f;
        pushConstant.textureIndex = 2;
        ctx.commandList->setPipeline(ctx.rasterTextureBindlessPipeline.get());
        ctx.commandList->setGraphicsPipelineLayout(ctx.rasterTextureBindlessPipelineLayout.get());
        ctx.commandList->setGraphicsPushConstants(0, &pushConstant);
    }

    static void setupRasterTextureBindfulPipeline(TestContext &ctx) {
        RasterPushConstant pushConstant;
        pushConstant.colorAdd[0] = 0.5f;
        pushConstant.colorAdd[1] = 0.25f;
        pushConstant.colorAdd[2] = 0.0f;
        pushConstant.colorAdd[3] = 0.0f;
        ctx.commandList->setPipeline(ctx.rasterTextureBindfulPipeline.get());
        ctx.commandList->setGraphicsPipelineLayout(ctx.rasterTextureBindfulPipelineLayout.get());
        ctx.commandList->setGraphicsPushConstants(0, &pushConstant);
    }

    static void drawRasterTriangle(TestContext& ctx) {
        ctx.commandList->setVertexBuffers(0, &ctx.vertexBufferView, 1, &ctx.inputSlot);
        ctx.commandList->setIndexBuffer(&ctx.indexBufferView);
        ctx.commandList->drawInstanced(3, 1, 0, 0);
    }

    static void dispatchCompute(TestContext& ctx) {
        const uint32_t GroupCount = 8;
        const uint32_t width = ctx.swapChain->getWidth();
        const uint32_t height = ctx.swapChain->getHeight();

        ComputePushConstant pushCostant;
        pushCostant.Resolution[0] = width;
        pushCostant.Resolution[1] = height;
        pushCostant.Multiply[0] = 0.5f;
        pushCostant.Multiply[1] = 0.5f;
        pushCostant.Multiply[2] = 1.0f;
        pushCostant.Multiply[3] = 1.0f;
        ctx.commandList->setPipeline(ctx.computePipeline.get());
        ctx.commandList->setComputePipelineLayout(ctx.computePipelineLayout.get());
        ctx.commandList->setComputePushConstants(0, &pushCostant);
        ctx.commandList->setComputeDescriptorSet(ctx.computeFirstSet->get(), 0);
        ctx.commandList->setComputeDescriptorSet(ctx.computeSecondSet->get(), 1);
        ctx.commandList->dispatch((width + GroupCount - 1) / GroupCount, (height + GroupCount - 1) / GroupCount, 1);
    }

    struct ClearTest : public TestBase {
        void initialize(TestContext& ctx) override {
            createPostPipeline(ctx);
            resize(ctx);
        }

        void resize(TestContext& ctx) override {
            createSwapChain(ctx);
            createTargets(ctx);
        }

        void draw(TestContext& ctx) override {
            ctx.commandList->begin();
            initializeRenderTargets(ctx);
            resolveMultisampledTexture(ctx);
            applyPostProcessToSwapChain(ctx);
            ctx.commandList->end();
            presentSwapChain(ctx);
        }
    };

    struct RasterTest : public TestBase {
        std::unique_ptr<RenderPipeline> rasterColorPipelineBlend;
        std::unique_ptr<RenderPipeline> rasterDecalPipeline;
        std::unique_ptr<RenderPipelineLayout> rasterDecalPipelineLayout;
        std::unique_ptr<RenderDescriptorSet> rasterDecalDescriptorSet;

        void createSecondRasterPipeline(TestContext& ctx) {
            const RenderShaderFormat shaderFormat = ctx.renderInterface->getCapabilities().shaderFormat;
            ShaderData psData = getShaderData(shaderFormat, ShaderType::COLOR_PIXEL);
            ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

            std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
            std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);

            // Create input elements matching the first pipeline
            std::vector<RenderInputElement> inputElements;
            inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
            inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, 0));

            RenderGraphicsPipelineDesc graphicsDesc;
            graphicsDesc.inputSlots = &ctx.inputSlot;
            graphicsDesc.inputSlotsCount = 1;
            graphicsDesc.inputElements = inputElements.data();
            graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
            graphicsDesc.pipelineLayout = ctx.rasterColorPipelineLayout.get();
            graphicsDesc.pixelShader = pixelShader.get();
            graphicsDesc.vertexShader = vertexShader.get();
            graphicsDesc.renderTargetFormat[0] = ColorFormat;
            graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
            graphicsDesc.depthTargetFormat = DepthFormat;
            graphicsDesc.renderTargetCount = 1;
            graphicsDesc.multisampling.sampleCount = MSAACount;

            rasterColorPipelineBlend = ctx.device->createGraphicsPipeline(graphicsDesc);
        }

        void createRasterDecalPipeline(TestContext &ctx) {
            RenderDescriptorSetBuilder setBuilder;
            setBuilder.begin();
            setBuilder.addTexture(0);
            setBuilder.end();
            rasterDecalDescriptorSet = setBuilder.create(ctx.device.get());

            RenderPipelineLayoutBuilder layoutBuilder;
            layoutBuilder.begin(false, true);
            layoutBuilder.addDescriptorSet(setBuilder);
            layoutBuilder.end();

            rasterDecalPipelineLayout = layoutBuilder.create(ctx.device.get());

            const RenderShaderFormat shaderFormat = ctx.renderInterface->getCapabilities().shaderFormat;
            ShaderData psData = getShaderData(shaderFormat, ShaderType::DECAL_PIXEL);
            ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

            std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
            std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);

            // Create input elements matching the first pipeline
            std::vector<RenderInputElement> inputElements;
            inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
            inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

            RenderGraphicsPipelineDesc graphicsDesc;
            graphicsDesc.inputSlots = &ctx.inputSlot;
            graphicsDesc.inputSlotsCount = 1;
            graphicsDesc.inputElements = inputElements.data();
            graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
            graphicsDesc.pipelineLayout = rasterDecalPipelineLayout.get();
            graphicsDesc.pixelShader = pixelShader.get();
            graphicsDesc.vertexShader = vertexShader.get();
            graphicsDesc.renderTargetFormat[0] = ColorFormat;
            graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            graphicsDesc.depthTargetFormat = DepthFormat;
            graphicsDesc.renderTargetCount = 1;
            graphicsDesc.multisampling.sampleCount = MSAACount;

            rasterDecalPipeline = ctx.device->createGraphicsPipeline(graphicsDesc);
        }

        void initialize(TestContext& ctx) override {
            createRasterColorShader(ctx);
            createSecondRasterPipeline(ctx);
            createRasterDecalPipeline(ctx);
            createPostPipeline(ctx);
            createVertexBuffer(ctx);
            resize(ctx);
        }

        void resize(TestContext& ctx) override {
            createSwapChain(ctx);
            createTargets(ctx);
            rasterDecalDescriptorSet->setTexture(0, ctx.depthTarget.get(), RenderTextureLayout::DEPTH_READ, ctx.depthTargetView.get());
        }

        void draw(TestContext& ctx) override {
            ctx.commandList->begin();
            initializeRenderTargets(ctx);

            // Draw first triangle with normal pipeline
            setupRasterColorPipeline(ctx);
            RasterPushConstant pushConstant0;
            pushConstant0.colorAdd[0] = 0.0f;
            pushConstant0.colorAdd[1] = 0.0f;
            pushConstant0.colorAdd[2] = 0.0f;
            pushConstant0.colorAdd[3] = 0.0f;
            ctx.commandList->setGraphicsPushConstants(0, &pushConstant0);
            drawRasterTriangle(ctx);

            // Offset the viewport for the second triangle
            const uint32_t width = ctx.swapChain->getWidth();
            const uint32_t height = ctx.swapChain->getHeight();
            const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
            RenderViewport offsetViewport = viewport;

            // Draw second triangle with push constants for red color
            offsetViewport.y -= 200.0f;
            ctx.commandList->setViewports(offsetViewport);
            RasterPushConstant pushConstant1;
            pushConstant1.colorAdd[0] = 1.0f; // Red
            pushConstant1.colorAdd[1] = 0.0f;
            pushConstant1.colorAdd[2] = 0.0f;
            pushConstant1.colorAdd[3] = 1.0f;
            ctx.commandList->setGraphicsPushConstants(0, &pushConstant1);
            drawRasterTriangle(ctx);

            // Draw third triangle with push constants for green color
            offsetViewport.y -= 100.0f;
            ctx.commandList->setViewports(offsetViewport);
            RasterPushConstant pushConstant2;
            pushConstant2.colorAdd[0] = 0.0f;
            pushConstant2.colorAdd[1] = 1.0f; // Green
            pushConstant2.colorAdd[2] = 0.0f;
            pushConstant2.colorAdd[3] = 1.0f;
            ctx.commandList->setGraphicsPushConstants(0, &pushConstant2);
            drawRasterTriangle(ctx);

            // Depth target must be in read-only mode
            ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(ctx.depthTarget.get(), RenderTextureLayout::DEPTH_READ));

            // Draw decal triangle
            offsetViewport.x += 300.0f;
            offsetViewport.y += 300.0f;
            ctx.commandList->setFramebuffer(ctx.framebufferDepthRead.get());
            ctx.commandList->setViewports(offsetViewport);
            ctx.commandList->setGraphicsPipelineLayout(rasterDecalPipelineLayout.get());
            ctx.commandList->setGraphicsDescriptorSet(rasterDecalDescriptorSet.get(), 0);
            ctx.commandList->setPipeline(rasterDecalPipeline.get());
            drawRasterTriangle(ctx);

            // Switch depth target back to write mode
            ctx.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(ctx.depthTarget.get(), RenderTextureLayout::DEPTH_WRITE));

            // Draw alpha blended triangle
            offsetViewport.x -= 200.0f;
            ctx.commandList->setFramebuffer(ctx.framebuffer.get());
            ctx.commandList->setViewports(offsetViewport);
            ctx.commandList->setGraphicsPipelineLayout(ctx.rasterColorPipelineLayout.get());
            ctx.commandList->setPipeline(rasterColorPipelineBlend.get());
            ctx.commandList->setGraphicsPushConstants(0, &pushConstant0);
            drawRasterTriangle(ctx);

            resolveMultisampledTexture(ctx);
            applyPostProcessToSwapChain(ctx);
            ctx.commandList->end();
            presentSwapChain(ctx);
        }
    };

    struct TextureTest : public TestBase {
        void initialize(TestContext& ctx) override {
            createRasterTextureBindfulShader(ctx);
            createRasterTextureBindlessShader(ctx);
            createPostPipeline(ctx);
            uploadCheckTexture(ctx);
            uploadNoiseTexture(ctx);
            uploadStripedTexture(ctx);
            createVertexBuffer(ctx);
            resize(ctx);
        }

        void resize(TestContext& ctx) override {
            createSwapChain(ctx);
            createTargets(ctx);
        }

        void draw(TestContext& ctx) override {
            ctx.commandList->begin();
            initializeRenderTargets(ctx);
            setupRasterTextureBindlessPipeline(ctx);
            ctx.commandList->setGraphicsDescriptorSet(ctx.rasterTextureBindlessSet->get(), 0);
            drawRasterTriangle(ctx);

            // Share pipeline and layout on both of the next triangles, only change the sets.
            setupRasterTextureBindfulPipeline(ctx);

            const uint32_t width = ctx.swapChain->getWidth();
            const uint32_t height = ctx.swapChain->getHeight();
            const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
            RenderViewport offsetViewport = viewport;
            offsetViewport.x += 200.0f;
            ctx.commandList->setViewports(offsetViewport);
            ctx.commandList->setGraphicsDescriptorSet(ctx.rasterTextureNoiseSet->get(), 0);
            drawRasterTriangle(ctx);

            offsetViewport.x += 200.0f;
            ctx.commandList->setViewports(offsetViewport);
            ctx.commandList->setGraphicsDescriptorSet(ctx.rasterTextureStripedSet->get(), 0);
            drawRasterTriangle(ctx);

            resolveMultisampledTexture(ctx);
            applyPostProcessToSwapChain(ctx);
            ctx.commandList->end();
            presentSwapChain(ctx);
        }
    };

    struct ComputeTest : public TestBase {
        void initialize(TestContext& ctx) override {
            createRasterTextureBindlessShader(ctx);
            createPostPipeline(ctx);
            uploadCheckTexture(ctx);
            createVertexBuffer(ctx);
            createComputePipeline(ctx);
            resize(ctx);
        }

        void resize(TestContext& ctx) override {
            createSwapChain(ctx);
            createTargets(ctx);
            ctx.computeSecondSet->setTexture(ctx.computeSecondSet->gTarget, ctx.colorTargetResolved.get(), RenderTextureLayout::GENERAL);
        }

        void draw(TestContext& ctx) override {
            ctx.commandList->begin();
            initializeRenderTargets(ctx);
            setupRasterTextureBindlessPipeline(ctx);
            ctx.commandList->setGraphicsDescriptorSet(ctx.rasterTextureBindlessSet->get(), 0);
            drawRasterTriangle(ctx);
            resolveMultisampledTexture(ctx);
            ctx.commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(ctx.colorTargetResolved.get(), RenderTextureLayout::GENERAL));
            dispatchCompute(ctx);
            applyPostProcessToSwapChain(ctx);
            ctx.commandList->end();
            presentSwapChain(ctx);
        }
    };

    struct SpecConstantTest : public TestBase {
        std::unique_ptr<RenderPipeline> specConstantPipeline;
        std::unique_ptr<RenderPipelineLayout> specConstantLayout;

        void createSpecConstantPipeline(TestContext& ctx) {
            const RenderShaderFormat shaderFormat = ctx.renderInterface->getCapabilities().shaderFormat;
            assert(shaderFormat != RenderShaderFormat::DXIL && "DXIL does not support specialization constants.");

            RenderPipelineLayoutBuilder layoutBuilder;
            layoutBuilder.begin(false, true);
            layoutBuilder.end();

            specConstantLayout = layoutBuilder.create(ctx.device.get());

            ShaderData psData = getShaderData(shaderFormat, ShaderType::SPEC_PIXEL);
            ShaderData vsData = getShaderData(shaderFormat, ShaderType::VERTEX);

            std::vector<RenderInputElement> inputElements;
            inputElements.emplace_back(RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32_FLOAT, 0, 0));
            inputElements.emplace_back(RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT, 0, sizeof(float) * 2));

            std::unique_ptr<RenderShader> pixelShader = ctx.device->createShader(psData.blob, psData.size, "PSMain", shaderFormat);
            std::unique_ptr<RenderShader> vertexShader = ctx.device->createShader(vsData.blob, vsData.size, "VSMain", shaderFormat);

            const uint32_t FloatsPerVertex = 4;

            ctx.inputSlot = RenderInputSlot(0, sizeof(float) * FloatsPerVertex);

            // Create specialization constant
            std::vector<RenderSpecConstant> specConstants;
            specConstants.emplace_back(0, 1);

            RenderGraphicsPipelineDesc graphicsDesc;
            graphicsDesc.inputSlots = &ctx.inputSlot;
            graphicsDesc.inputSlotsCount = 1;
            graphicsDesc.inputElements = inputElements.data();
            graphicsDesc.inputElementsCount = uint32_t(inputElements.size());
            graphicsDesc.pipelineLayout = specConstantLayout.get();
            graphicsDesc.pixelShader = pixelShader.get();
            graphicsDesc.vertexShader = vertexShader.get();
            graphicsDesc.renderTargetFormat[0] = ColorFormat;
            graphicsDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            graphicsDesc.depthTargetFormat = DepthFormat;
            graphicsDesc.renderTargetCount = 1;
            graphicsDesc.multisampling.sampleCount = MSAACount;
            graphicsDesc.specConstants = specConstants.data();
            graphicsDesc.specConstantsCount = uint32_t(specConstants.size());

            specConstantPipeline = ctx.device->createGraphicsPipeline(graphicsDesc);
        }

        void initialize(TestContext& ctx) override {
            createSpecConstantPipeline(ctx);
            createPostPipeline(ctx);
            createVertexBuffer(ctx);
            resize(ctx);
        }

        void resize(TestContext& ctx) override {
            createSwapChain(ctx);
            createTargets(ctx);
        }

        void draw(TestContext& ctx) override {
            ctx.commandList->begin();
            initializeRenderTargets(ctx);

            // Draw triangle with spec constant pipeline
            ctx.commandList->setPipeline(specConstantPipeline.get());
            ctx.commandList->setGraphicsPipelineLayout(specConstantLayout.get());
            drawRasterTriangle(ctx);

            resolveMultisampledTexture(ctx);
            applyPostProcessToSwapChain(ctx);
            ctx.commandList->end();
            presentSwapChain(ctx);
        }
    };

    struct AsyncComputeTest : public TestBase {
        std::unique_ptr<std::thread> thread;
        std::unique_ptr<RenderPipeline> asyncPipeline;
        std::unique_ptr<RenderPipelineLayout> asyncPipelineLayout;
        std::unique_ptr<RenderDescriptorSet> asyncDescriptorSet;
        std::unique_ptr<RenderBuffer> asyncBuffer;
        std::unique_ptr<RenderBufferFormattedView> asyncBufferFormattedView;
        std::unique_ptr<RenderCommandQueue> asyncCommandQueue;
        std::unique_ptr<RenderCommandList> asyncCommandList;
        std::unique_ptr<RenderCommandFence> asyncCommandFence;
        RenderDevice* device;
        std::unique_ptr<RenderBuffer> asyncStructuredBuffer;
        std::unique_ptr<RenderBuffer> asyncByteAddressBuffer;

        void initialize(TestContext &ctx) override {
            device = ctx.device.get();
            asyncCommandQueue = ctx.device->createCommandQueue(RenderCommandListType::COMPUTE);
            asyncCommandList = asyncCommandQueue->createCommandList(RenderCommandListType::COMPUTE);
            asyncCommandFence = ctx.device->createCommandFence();

            // Update descriptor set builder to include two structured buffer views
            RenderDescriptorSetBuilder setBuilder;
            setBuilder.begin();
            uint32_t formattedBufferIndex = setBuilder.addReadWriteFormattedBuffer(1);
            uint32_t structuredBufferBaseIndex = setBuilder.addReadWriteStructuredBuffer(2);
            uint32_t structuredBufferOffsetIndex = setBuilder.addReadWriteStructuredBuffer(3);
            uint32_t byteAddressBufferIndex = setBuilder.addReadWriteByteAddressBuffer(4);
            setBuilder.end();

            RenderPipelineLayoutBuilder layoutBuilder;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(float), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(setBuilder);
            layoutBuilder.end();

            asyncPipelineLayout = layoutBuilder.create(ctx.device.get());

            // Create formatted buffer
            asyncBuffer = ctx.device->createBuffer(RenderBufferDesc::ReadbackBuffer(4 * sizeof(float),
                RenderBufferFlag::UNORDERED_ACCESS | RenderBufferFlag::FORMATTED | RenderBufferFlag::STORAGE));
            asyncBufferFormattedView = asyncBuffer->createBufferFormattedView(RenderFormat::R32_FLOAT);

            // Create structured buffer with test data
            struct CustomStruct {
                float point3D[3];
                float size2D[2];
            };
            CustomStruct structData[] = {
                {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},    // Element 0: sum = 5
                {{2.0f, 2.0f, 2.0f}, {2.0f, 2.0f}},    // Element 1: sum = 10
                {{3.0f, 3.0f, 3.0f}, {3.0f, 3.0f}},    // Element 2: sum = 15
                {{4.0f, 4.0f, 4.0f}, {4.0f, 4.0f}}     // Element 3: sum = 20
            };
            asyncStructuredBuffer = ctx.device->createBuffer(RenderBufferDesc::ReadbackBuffer(sizeof(structData),
                RenderBufferFlag::UNORDERED_ACCESS | RenderBufferFlag::STORAGE));
            void* structPtr = asyncStructuredBuffer->map();
            memcpy(structPtr, structData, sizeof(structData));
            asyncStructuredBuffer->unmap();

            // Create two views into the same buffer with different offsets
            const RenderBufferStructuredView baseView(sizeof(CustomStruct), 0);
            const RenderBufferStructuredView offsetView(sizeof(CustomStruct), 2);

            // Create byte address buffer with 8 floats
            const uint32_t byteAddressSize = 32;  // 8 floats * 4 bytes
            float byteAddressData[8] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
            asyncByteAddressBuffer = ctx.device->createBuffer(RenderBufferDesc::ReadbackBuffer(byteAddressSize,
                RenderBufferFlag::UNORDERED_ACCESS | RenderBufferFlag::STORAGE));
            void* bytePtr = asyncByteAddressBuffer->map();
            memcpy(bytePtr, byteAddressData, sizeof(byteAddressData));
            asyncByteAddressBuffer->unmap();

            // Bind both views to the descriptor set
            asyncDescriptorSet = setBuilder.create(ctx.device.get());
            asyncDescriptorSet->setBuffer(formattedBufferIndex, asyncBuffer.get(), 4 * sizeof(float), nullptr, asyncBufferFormattedView.get());
            asyncDescriptorSet->setBuffer(structuredBufferBaseIndex, asyncStructuredBuffer.get(), sizeof(structData), &baseView);
            asyncDescriptorSet->setBuffer(structuredBufferOffsetIndex, asyncStructuredBuffer.get(), sizeof(structData), &offsetView);
            asyncDescriptorSet->setBuffer(byteAddressBufferIndex, asyncByteAddressBuffer.get());

            // Create pipeline
            const RenderShaderFormat shaderFormat = ctx.renderInterface->getCapabilities().shaderFormat;
            ShaderData csData = getShaderData(shaderFormat, ShaderType::ASYNC_COMPUTE);
            std::unique_ptr<RenderShader> computeShader = ctx.device->createShader(csData.blob, csData.size, "CSMain", shaderFormat);
            asyncPipeline = ctx.device->createComputePipeline(RenderComputePipelineDesc(asyncPipelineLayout.get(), computeShader.get(), 1, 1, 1));

            thread = std::make_unique<std::thread>(&AsyncComputeTest::threadFunction, this);
        }

        void resize(TestContext &ctx) override {
            // This test does not use a swap chain.
        }

        void draw(TestContext &ctx) override {
            // This test does not draw anything.
        }

        void threadFunction() {
            const RenderCommandList *commandListPtr = asyncCommandList.get();
            float inputValue = 1.0f;
            float outputValues[4] = {};
            int frameCount = 0;

            while (true) {
                asyncCommandList->begin();
                asyncCommandList->setComputePipelineLayout(asyncPipelineLayout.get());
                asyncCommandList->setPipeline(asyncPipeline.get());
                asyncCommandList->setComputeDescriptorSet(asyncDescriptorSet.get(), 0);
                asyncCommandList->setComputePushConstants(0, &inputValue);
                asyncCommandList->dispatch(1, 1, 1);
                asyncCommandList->end();
                asyncCommandQueue->executeCommandLists(&commandListPtr, 1, nullptr, 0, nullptr, 0, asyncCommandFence.get());
                asyncCommandQueue->waitForCommandFence(asyncCommandFence.get());

                void* bufferData = asyncBuffer->map();
                memcpy(outputValues, bufferData, sizeof(outputValues));
                asyncBuffer->unmap();

                printf("Frame %d Results:\n", frameCount);
                printf("  Formatted Buffer: sqrt(%f) = %f (expected: %f)\n",
                    inputValue, outputValues[0], sqrt(inputValue));
                printf("  Structured Buffer Base View[0]: sum = %f (expected: %f)\n",
                    outputValues[1], 5.0f + (frameCount * 5.0f));
                printf("  Structured Buffer Offset View[0]: sum = %f (expected: %f)\n",
                    outputValues[2], 15.0f + (frameCount * 5.0f));
                printf("  Byte Address Buffer: value at offset 16 = %f (expected: %f)\n",
                    outputValues[3], 5.0f + frameCount);

                inputValue += 1.0f;
                frameCount++;

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(500ms);
            }
        }
    };

    // Test registration and management
    using TestSetupFunc = std::function<std::unique_ptr<TestBase>()>;
    static std::vector<TestSetupFunc> g_Tests;
    static std::unique_ptr<TestBase> g_CurrentTest;
    static uint32_t g_CurrentTestIndex = 1;

    void RegisterTests() {
        g_Tests.push_back([]() { return std::make_unique<ClearTest>(); });
        g_Tests.push_back([]() { return std::make_unique<RasterTest>(); });
        g_Tests.push_back([]() { return std::make_unique<TextureTest>(); });
        g_Tests.push_back([]() { return std::make_unique<ComputeTest>(); });
        g_Tests.push_back([]() { return std::make_unique<SpecConstantTest>(); });
        g_Tests.push_back([]() { return std::make_unique<AsyncComputeTest>(); });
    }

    // Update platform specific code to use the new test framework
#if defined(_WIN64)
    TestContext g_TestContext;

    static LRESULT CALLBACK TestWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_SIZE:
            if (g_CurrentTest != nullptr) {
                g_CurrentTest->resize(g_TestContext);
            }

            return 0;
        case WM_PAINT:
            if (g_CurrentTest != nullptr) {
                g_CurrentTest->draw(g_TestContext);
            }

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
        RegisterTests();
        HWND hwnd = TestCreateWindow();

        createContext(g_TestContext, renderInterface, hwnd);

        g_CurrentTest = g_Tests[g_CurrentTestIndex]();
        g_CurrentTest->initialize(g_TestContext);

        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        g_CurrentTest->shutdown(g_TestContext);
        DestroyWindow(hwnd);
    }
#elif defined(__ANDROID__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        assert(false);
    }
#elif defined(__linux__) || defined(__APPLE__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        RegisterTests();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return;
        }

        uint32_t flags = SDL_WINDOW_RESIZABLE;
    #if defined(__APPLE__)
        flags |= SDL_WINDOW_METAL;
    #endif

        SDL_Window *window = SDL_CreateWindow("Render Interface Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
        if (window == nullptr) {
            fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
            SDL_Quit();
            return;
        }

        // SDL_Window's handle can be used directly if needed
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);

        TestContext g_TestContext;
#   if defined(__linux__)
        createContext(g_TestContext, renderInterface, { wmInfo.info.x11.display, wmInfo.info.x11.window });
#   elif defined(__APPLE__)
        SDL_MetalView view = SDL_Metal_CreateView(window);
        createContext(g_TestContext, renderInterface, { wmInfo.info.cocoa.window, SDL_Metal_GetLayer(view) });
#   endif

        g_CurrentTest = g_Tests[g_CurrentTestIndex]();
        g_CurrentTest->initialize(g_TestContext);

        std::chrono::system_clock::time_point prev_frame = std::chrono::system_clock::now();
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            g_CurrentTest->resize(g_TestContext);
                        }
                        break;
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            auto now_time = std::chrono::system_clock::now();
            if (now_time - prev_frame > 16666us) {
                prev_frame = now_time;
                g_CurrentTest->draw(g_TestContext);
            }
        }

        g_CurrentTest->shutdown(g_TestContext);
#   if defined(__APPLE__)
        SDL_Metal_DestroyView(view);
#   endif
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
#endif
};
