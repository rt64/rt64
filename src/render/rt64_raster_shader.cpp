//
// RT64
//

#include "rt64_raster_shader.h"

#include "xxHash/xxh3.h"

#include "shaders/RasterPSDynamic.hlsl.spirv.h"
#include "shaders/RasterPSDynamicMS.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstant.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantFlat.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantDepth.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantDepthMS.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantFlatDepth.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantFlatDepthMS.hlsl.spirv.h"
#include "shaders/RasterVSDynamic.hlsl.spirv.h"
#include "shaders/RasterVSSpecConstant.hlsl.spirv.h"
#include "shaders/RasterVSSpecConstantFlat.hlsl.spirv.h"
#include "shaders/PostBlendDitherNoiseAddPS.hlsl.spirv.h"
#include "shaders/PostBlendDitherNoiseSubPS.hlsl.spirv.h"
#ifdef _WIN32
#   include "shaders/RasterPSDynamic.hlsl.dxil.h"
#   include "shaders/RasterPSDynamicMS.hlsl.dxil.h"
#   include "shaders/RasterVSDynamic.hlsl.dxil.h"
#   include "shaders/PostBlendDitherNoiseAddPS.hlsl.dxil.h"
#   include "shaders/PostBlendDitherNoiseSubPS.hlsl.dxil.h"
#endif
#include "shaders/RasterPS.hlsl.rw.h"
#include "shaders/RasterVS.hlsl.rw.h"
#include "shared/rt64_raster_params.h"

#include "rt64_descriptor_sets.h"
#include "rt64_render_target.h"

namespace RT64 {
    static const std::string RasterPSString(reinterpret_cast<const char *>(RasterPSText), sizeof(RasterPSText));
    static const std::string RasterVSString(reinterpret_cast<const char *>(RasterVSText), sizeof(RasterVSText));
     
    static const RenderFormat RasterPositionFormat = RenderFormat::R32G32B32A32_FLOAT;
    static const RenderFormat RasterTexcoordFormat = RenderFormat::R32G32_FLOAT;
    static const RenderFormat RasterColorFormat = RenderFormat::R32G32B32A32_FLOAT;

    static const RenderInputSlot RasterInputSlots[3] = {
        RenderInputSlot(0, RenderFormatSize(RasterPositionFormat)),
        RenderInputSlot(1, RenderFormatSize(RasterTexcoordFormat)),
        RenderInputSlot(2, RenderFormatSize(RasterColorFormat))
    };

    static const RenderInputElement RasterInputElements[3] = {
        RenderInputElement("POSITION", 0, 0, RasterPositionFormat, 0, 0),
        RenderInputElement("TEXCOORD", 0, 1, RasterTexcoordFormat, 1, 0),
        RenderInputElement("COLOR", 0, 2, RasterColorFormat, 2, 0)
    };

    // RasterShader

    RasterShader::RasterShader(RenderDevice *device, const ShaderDescription &desc, const RenderPipelineLayout *pipelineLayout, RenderShaderFormat shaderFormat, const RenderMultisampling &multisampling, 
        const ShaderCompiler *shaderCompiler, std::vector<uint8_t> *vsBytes, std::vector<uint8_t> *psBytes, bool useBytes)
    {
        assert(device != nullptr);

        this->device = device;
        this->desc = desc;
        
        const bool useMSAA = (multisampling.sampleCount > 1);
        std::unique_ptr<RenderShader> vertexShader;
        std::unique_ptr<RenderShader> pixelShader;
        std::vector<RenderSpecConstant> specConstants;
        if (shaderFormat == RenderShaderFormat::SPIRV) {
            // Choose the pre-compiled shader permutations.
            const void *VSBlob = nullptr;
            const void *PSBlob = nullptr;
            uint32_t VSBlobSize = 0;
            uint32_t PSBlobSize = 0;
            const bool outputDepth = desc.outputDepth(useMSAA);
            if (desc.flags.smoothShade) {
                VSBlob = RasterVSSpecConstantBlobSPIRV;
                VSBlobSize = uint32_t(std::size(RasterVSSpecConstantBlobSPIRV));
            }
            else {
                VSBlob = RasterVSSpecConstantFlatBlobSPIRV;
                VSBlobSize = uint32_t(std::size(RasterVSSpecConstantFlatBlobSPIRV));
            }
            
            // Pick the correct SPIR-V based on the configuration.
            if (desc.flags.smoothShade) {
                if (outputDepth) {
                    PSBlob = useMSAA ? RasterPSSpecConstantDepthMSBlobSPIRV : RasterPSSpecConstantDepthBlobSPIRV;
                    PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSSpecConstantDepthMSBlobSPIRV) : std::size(RasterPSSpecConstantDepthBlobSPIRV));
                }
                else {
                    PSBlob = RasterPSSpecConstantBlobSPIRV;
                    PSBlobSize = uint32_t(std::size(RasterPSSpecConstantBlobSPIRV));
                }
            }
            else {
                if (outputDepth) {
                    PSBlob = useMSAA ? RasterPSSpecConstantFlatDepthMSBlobSPIRV : RasterPSSpecConstantFlatDepthBlobSPIRV;
                    PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSSpecConstantFlatDepthMSBlobSPIRV) : std::size(RasterPSSpecConstantFlatDepthBlobSPIRV));
                }
                else {
                    PSBlob = RasterPSSpecConstantFlatBlobSPIRV;
                    PSBlobSize = uint32_t(std::size(RasterPSSpecConstantFlatBlobSPIRV));
                }
            }
            
            vertexShader = device->createShader(VSBlob, VSBlobSize, "VSMain", shaderFormat);
            pixelShader = device->createShader(PSBlob, PSBlobSize, "PSMain", shaderFormat);
            
            // Spec constants should replace the constants embedded in the shader directly.
            specConstants.emplace_back(0, desc.otherMode.L);
            specConstants.emplace_back(1, desc.otherMode.H);
            specConstants.emplace_back(2, desc.colorCombiner.L);
            specConstants.emplace_back(3, desc.colorCombiner.H);
            specConstants.emplace_back(4, desc.flags.value);
        }
        else {
#       if defined(_WIN32)
            if (useBytes) {
                vertexShader = device->createShader(vsBytes->data(), vsBytes->size(), "VSMain", shaderFormat);
                pixelShader = device->createShader(psBytes->data(), psBytes->size(), "PSMain", shaderFormat);
            }
            else {
                RasterShaderText shaderText = generateShaderText(desc, useMSAA);

                // Compile both shaders from text with the constants hard-coded in.
                IDxcBlob *blobVS, *blobPS;
                const std::wstring VertexShaderName = L"VSMain";
                shaderCompiler->compile(shaderText.vertexShader, VertexShaderName, L"vs_6_3", shaderFormat, &blobVS);

                const std::wstring PixelShaderName = L"PSMain";
                shaderCompiler->compile(shaderText.pixelShader, PixelShaderName, L"ps_6_3", shaderFormat, &blobPS);

                vertexShader = device->createShader(blobVS->GetBufferPointer(), blobVS->GetBufferSize(), "VSMain", shaderFormat);
                pixelShader = device->createShader(blobPS->GetBufferPointer(), blobPS->GetBufferSize(), "PSMain", shaderFormat);

                // Store the bytes in the auxiliary buffers if specified.
                if (vsBytes != nullptr) {
                    *vsBytes = std::vector<uint8_t>((const uint8_t *)(blobVS->GetBufferPointer()), (const uint8_t *)(blobVS->GetBufferPointer()) + blobVS->GetBufferSize());
                }

                if (psBytes != nullptr) {
                    *psBytes = std::vector<uint8_t>((const uint8_t *)(blobPS->GetBufferPointer()), (const uint8_t *)(blobPS->GetBufferPointer()) + blobPS->GetBufferSize());
                }

                // Blobs can be discarded once the shaders are created.
                blobPS->Release();
                blobVS->Release();
            }
#       else
            assert(false && "This platform does not support runtime shader compilation.");
#       endif
        }
        
        // Create root signature and PSO.
        const bool copyMode = (desc.otherMode.cycleType() == G_CYC_COPY);
        PipelineCreation creation;
        creation.device = device;
        creation.pipelineLayout = pipelineLayout;
        creation.vertexShader = vertexShader.get();
        creation.pixelShader = pixelShader.get();
        creation.alphaBlend = !copyMode && interop::Blender::usesAlphaBlend(desc.otherMode);
        creation.culling = !copyMode && desc.flags.culling;
        creation.zCmp = !copyMode && desc.otherMode.zCmp();
        creation.zUpd = !copyMode && desc.otherMode.zUpd();
        creation.zDecal = !copyMode && (desc.otherMode.zMode() == ZMODE_DEC);
        creation.cvgAdd = (desc.otherMode.cvgDst() == CVG_DST_WRAP) || (desc.otherMode.cvgDst() == CVG_DST_SAVE);
        creation.NoN = desc.flags.NoN;
        creation.usesHDR = desc.flags.usesHDR;
        creation.specConstants = specConstants;
        creation.multisampling = multisampling;
        pipeline = createPipeline(creation);
    }

    RasterShader::~RasterShader() { }

    RasterShaderText RasterShader::generateShaderText(const ShaderDescription &desc, bool multisampling) {
        const std::string renderParamsCode = desc.toShader();

        // Generate vertex shader.
        std::stringstream vss;
        vss << RasterVSString;
        vss << "RenderParams getRenderParams() {" + renderParamsCode + "; return rp; }";
        vss <<
            "void VSMain("
            "   in float4 iPosition : POSITION,"
            "   in float2 iUV : TEXCOORD,"
            "   in float4 iColor : COLOR,"
            "   out float4 oPosition : SV_POSITION,"
            "   out float2 oUV : TEXCOORD,"
            "   out float4 oSmoothColor : COLOR0";

        if (!desc.flags.smoothShade) {
            vss << ", out float4 oFlatColor : COLOR1) {";
        }
        else {
            vss << ") { float4 oFlatColor;";
        }

        vss <<
            "   RasterVS(getRenderParams(), iPosition, iUV, iColor, oPosition, oUV, oSmoothColor, oFlatColor);"
            "}";

        // Generate pixel shader.
        std::stringstream pss;
        if (multisampling) {
            pss << 
                "Texture2DMS<float> gBackgroundDepth : register(t2, space3);"
                "float sampleBackgroundDepth(int2 pixelPos, uint sampleIndex) { return gBackgroundDepth.Load(pixelPos, sampleIndex); }";
        }
        else {
            pss <<
                "Texture2D<float> gBackgroundDepth : register(t2, space3);"
                "float sampleBackgroundDepth(int2 pixelPos, uint sampleIndex) { return gBackgroundDepth.Load(int3(pixelPos, 0)); }";
        }

        pss << RasterPSString;
        pss << "RenderParams getRenderParams() {" + renderParamsCode + "; return rp; }";
        pss <<
            "void PSMain("
            "  in float4 vertexPosition : SV_POSITION"
            ", in float2 vertexUV : TEXCOORD"
            ", in float4 vertexSmoothColor : COLOR0";

        if (!desc.flags.smoothShade) {
            pss << ", nointerpolation in float4 vertexFlatColor : COLOR1";
        }

        if (multisampling) {
            pss << ", in uint sampleIndex : SV_SampleIndex";
        }

        pss <<
            ", [[vk::location(0)]] [[vk::index(0)]] out float4 resultColor : SV_TARGET0"
            ", [[vk::location(0)]] [[vk::index(1)]] out float4 resultAlpha : SV_TARGET1";

        if (desc.outputDepth(multisampling)) {
            pss << ", out float resultDepth : SV_DEPTH";
        }

        if (desc.outputDepth(multisampling)) {
            pss << ") { bool outputDepth = true;";
        }
        else {
            pss << ") { float resultDepth; bool outputDepth = false;";
        }
        
        if (desc.flags.smoothShade) {
            pss << "float4 vertexFlatColor = vertexSmoothColor;";
        }

        if (!multisampling) {
            pss << "uint sampleIndex = 0;";
        }

        pss <<
            "   RasterPS(getRenderParams(), outputDepth, vertexPosition, vertexUV, vertexSmoothColor, vertexFlatColor, sampleIndex, resultColor, resultAlpha, resultDepth);"
            "}";

        return { vss.str(), pss.str() };
    }
    
    std::unique_ptr<RenderPipeline> RasterShader::createPipeline(const PipelineCreation &c) {
        assert((!c.zDecal || !c.zUpd) && "Decals with depth write should never be created.");

        RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(c.usesHDR);
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.cullMode = c.culling ? RenderCullMode::FRONT : RenderCullMode::NONE;
        pipelineDesc.depthClipEnabled = !c.NoN;
        pipelineDesc.depthEnabled = c.zCmp || c.zUpd;
        pipelineDesc.depthWriteEnabled = c.zUpd;
        pipelineDesc.depthTargetFormat = RenderFormat::D32_FLOAT;
        pipelineDesc.multisampling = c.multisampling;
        pipelineDesc.inputSlots = RasterInputSlots;
        pipelineDesc.inputSlotsCount = uint32_t(std::size(RasterInputSlots));
        pipelineDesc.inputElements = RasterInputElements;
        pipelineDesc.inputElementsCount = uint32_t(std::size(RasterInputElements));
        pipelineDesc.pipelineLayout = c.pipelineLayout;
        pipelineDesc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
        pipelineDesc.vertexShader = c.vertexShader;
        pipelineDesc.pixelShader = c.pixelShader;
        pipelineDesc.specConstants = c.specConstants.data();
        pipelineDesc.specConstantsCount = uint32_t(c.specConstants.size());

        if (c.zCmp) {
            // While these modes evaluate equality in the hardware, we use LEQUAL to simulate the depth comparison in the shader instead.
            if (c.zDecal) {
                pipelineDesc.depthFunction = RenderComparisonFunction::LESS_EQUAL;
            }
            // ZMODE_OPA, ZMODE_XLU and ZMODE_INTER only differ based on coverage, which is not emulated, so they can all be approximated the same way.
            else {
                pipelineDesc.depthFunction = RenderComparisonFunction::LESS;
            }
        }
        else {
            pipelineDesc.depthFunction = RenderComparisonFunction::ALWAYS;
        }

        // Alpha blending is performed by using dual source blending. The blending factor will be in the secondary output.
        RenderBlendDesc &targetBlend = pipelineDesc.renderTargetBlend[0];
        if (c.alphaBlend) {
            targetBlend.blendEnabled = true;
            targetBlend.srcBlend = RenderBlend::SRC1_ALPHA;
            targetBlend.dstBlend = RenderBlend::INV_SRC1_ALPHA;
            targetBlend.blendOp = RenderBlendOperation::ADD;
        }

        // Emulating coverage wrap requires turning the alpha blending into an additive mode.
        if (c.cvgAdd) {
            targetBlend.blendEnabled = true;
            targetBlend.dstBlendAlpha = RenderBlend::ONE;
        }

        return c.device->createGraphicsPipeline(pipelineDesc);
    }
    
    RenderMultisampling RasterShader::generateMultisamplingPattern(RenderSampleCounts sampleCount, bool sampleLocationsSupported) {
#   if SAMPLE_LOCATIONS_REQUIRED
        if (!sampleLocationsSupported) {
            return RenderMultisampling();
        }
#   endif

        RenderMultisampling multisampling;
        multisampling.sampleCount = sampleCount;

        if ((sampleCount > 1) && sampleLocationsSupported) {
            multisampling.sampleLocationsEnabled = true;
            
            // These were roughly translated from this article (https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels)
            // into the bottom right corner of the pixel area to account for the half pixel offset problem.
            auto &locations = multisampling.sampleLocations;
            switch (sampleCount) {
            case 2:
                locations[0] = { 2, 2 };
                locations[1] = { 6, 6 };
                break;
            case 4:
                locations[0] = { 3, 1 };
                locations[1] = { 7, 3 };
                locations[2] = { 1, 5 };
                locations[3] = { 5, 7 };
                break;
            case 8:
                locations[0] = { 4, 2 };
                locations[1] = { 3, 5 };
                locations[2] = { 6, 4 };
                locations[3] = { 2, 1 };
                locations[4] = { 1, 6 };
                locations[5] = { 0, 3 };
                locations[6] = { 5, 7 };
                locations[7] = { 7, 0 };
                break;
            default:
                assert(false && "Unknown sample count.");
                break;
            }
        }

        return multisampling;
    }

    // RasterShaderUber

    const uint64_t RasterShaderUber::RasterVSTextHash = XXH3_64bits(RasterVSText, sizeof(RasterVSText));
    const uint64_t RasterShaderUber::RasterPSTextHash = XXH3_64bits(RasterPSText, sizeof(RasterPSText));

    RasterShaderUber::RasterShaderUber(RenderDevice *device, RenderShaderFormat shaderFormat, const RenderMultisampling &multisampling, const ShaderLibrary *shaderLibrary, uint32_t threadCount) {
        assert(device != nullptr);

        // Create the shaders.
        const void *VSBlob = nullptr;
        const void *PSBlob = nullptr;
        uint32_t VSBlobSize = 0;
        uint32_t PSBlobSize = 0;
        const bool useMSAA = (multisampling.sampleCount > 1);
        switch (shaderFormat) {
#   ifdef _WIN32
        case RenderShaderFormat::DXIL:
            VSBlob = RasterVSDynamicBlobDXIL;
            PSBlob = useMSAA ? RasterPSDynamicMSBlobDXIL : RasterPSDynamicBlobDXIL;
            VSBlobSize = uint32_t(std::size(RasterVSDynamicBlobDXIL));
            PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSDynamicMSBlobDXIL) : std::size(RasterPSDynamicBlobDXIL));
            break;
#   endif
        case RenderShaderFormat::SPIRV:
            VSBlob = RasterVSDynamicBlobSPIRV;
            PSBlob = useMSAA ? RasterPSDynamicMSBlobSPIRV : RasterPSDynamicBlobSPIRV;
            VSBlobSize = uint32_t(std::size(RasterVSDynamicBlobSPIRV));
            PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSDynamicMSBlobSPIRV) : std::size(RasterPSDynamicBlobSPIRV));
            break;
        default:
            assert(false && "Unknown shader format.");
            return;
        }

        vertexShader = device->createShader(VSBlob, VSBlobSize, "VSMain", shaderFormat);
        pixelShader = device->createShader(PSBlob, PSBlobSize, "PSMain", shaderFormat);

        FramebufferRendererDescriptorCommonSet descriptorCommonSet(shaderLibrary->samplerLibrary, device->getCapabilities().raytracing);
        FramebufferRendererDescriptorTextureSet descriptorTextureSet;
        FramebufferRendererDescriptorFramebufferSet descriptorFramebufferSet;
        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, true);
        layoutBuilder.addPushConstant(0, 0, sizeof(interop::RasterParams), RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(descriptorCommonSet);
        layoutBuilder.addDescriptorSet(descriptorTextureSet);
        layoutBuilder.addDescriptorSet(descriptorTextureSet);
        layoutBuilder.addDescriptorSet(descriptorFramebufferSet);
        layoutBuilder.end();
        pipelineLayout = layoutBuilder.create(device);

        // Generate all possible combinations of pipeline creations and assign them to each thread. Skip the ones that are invalid.
        pipelineThreadCreations.clear();
        pipelineThreadCreations.resize(threadCount);

        PipelineCreation creation;
        creation.device = device;
        creation.pipelineLayout = pipelineLayout.get();
        creation.vertexShader = vertexShader.get();
        creation.pixelShader = pixelShader.get();
        creation.NoN = true;
        creation.usesHDR = shaderLibrary->usesHDR;
        creation.multisampling = multisampling;

        uint32_t threadIndex = 0;
        uint32_t pipelineCount = uint32_t(std::size(pipelines));
        for (uint32_t i = 0; i < pipelineCount; i++) {
            creation.alphaBlend = i & (1 << 0);
            creation.culling = i & (1 << 1);
            creation.zCmp = i & (1 << 2);
            creation.zUpd = i & (1 << 3);
            creation.zDecal = i & (1 << 4);
            creation.cvgAdd = i & (1 << 5);

            // Skip all PSOs that would lead to invalid decal behavior.
            if (creation.zDecal && (creation.zUpd || !creation.zCmp)) {
                continue;
            }

            pipelineThreadCreations[threadIndex].emplace_back(creation);
            threadIndex = (threadIndex + 1) % threadCount;
        }

        // Spawn the threads that will compile all the pipelines.
        pipelineThreads.clear();
        pipelineThreads.resize(threadCount);
        for (uint32_t i = 0; i < threadCount; i++) {
            pipelineThreads[i] = std::make_unique<std::thread>(&RasterShaderUber::threadCreatePipelines, this, i);
        }

        // Create the pipelines for post blend operations.
        std::unique_ptr<RenderShader> postBlendAddPixelShader;
        std::unique_ptr<RenderShader> postBlendSubPixelShader;
        switch (shaderFormat) {
#   ifdef _WIN32
        case RenderShaderFormat::DXIL:
            postBlendAddPixelShader = device->createShader(PostBlendDitherNoiseAddPSBlobDXIL, std::size(PostBlendDitherNoiseAddPSBlobDXIL), "PSMain", shaderFormat);
            postBlendSubPixelShader = device->createShader(PostBlendDitherNoiseSubPSBlobDXIL, std::size(PostBlendDitherNoiseSubPSBlobDXIL), "PSMain", shaderFormat);
            break;
#   endif
        case RenderShaderFormat::SPIRV:
            postBlendAddPixelShader = device->createShader(PostBlendDitherNoiseAddPSBlobSPIRV, std::size(PostBlendDitherNoiseAddPSBlobSPIRV), "PSMain", shaderFormat);
            postBlendSubPixelShader = device->createShader(PostBlendDitherNoiseSubPSBlobSPIRV, std::size(PostBlendDitherNoiseSubPSBlobSPIRV), "PSMain", shaderFormat);
            break;
        default:
            assert(false && "Unknown shader format.");
            return;
        }

        RenderGraphicsPipelineDesc postBlendDesc;
        postBlendDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        postBlendDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(shaderLibrary->usesHDR);
        postBlendDesc.renderTargetCount = 1;
        postBlendDesc.cullMode = RenderCullMode::NONE;
        postBlendDesc.depthTargetFormat = RenderFormat::D32_FLOAT;
        postBlendDesc.multisampling = multisampling;
        postBlendDesc.inputSlots = RasterInputSlots;
        postBlendDesc.inputSlotsCount = uint32_t(std::size(RasterInputSlots));
        postBlendDesc.inputElements = RasterInputElements;
        postBlendDesc.inputElementsCount = uint32_t(std::size(RasterInputElements));
        postBlendDesc.pipelineLayout = pipelineLayout.get();
        postBlendDesc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
        postBlendDesc.vertexShader = vertexShader.get();
        postBlendDesc.pixelShader = postBlendAddPixelShader.get();

        RenderBlendDesc &targetBlend = postBlendDesc.renderTargetBlend[0];
        targetBlend.blendEnabled = true;
        targetBlend.srcBlend = RenderBlend::ONE;
        targetBlend.dstBlend = RenderBlend::ONE;
        targetBlend.blendOp = RenderBlendOperation::ADD;
        targetBlend.srcBlendAlpha = RenderBlend::ZERO;
        targetBlend.dstBlendAlpha = RenderBlend::ONE;
        targetBlend.blendOpAlpha = RenderBlendOperation::ADD;

        postBlendDitherNoiseAddPipeline = device->createGraphicsPipeline(postBlendDesc);

        postBlendDesc.pixelShader = postBlendSubPixelShader.get();
        targetBlend.blendOp = RenderBlendOperation::REV_SUBTRACT;
        postBlendDitherNoiseSubPipeline = device->createGraphicsPipeline(postBlendDesc);
    }

    RasterShaderUber::~RasterShaderUber() {
        waitForPipelineCreation();
    }

    void RasterShaderUber::threadCreatePipelines(uint32_t threadIndex) {
        for (const PipelineCreation &creation : pipelineThreadCreations[threadIndex]) {
            uint32_t pipelineIndex = pipelineStateIndex(creation.alphaBlend, creation.culling, creation.zCmp, creation.zUpd, creation.zDecal, creation.cvgAdd);
            pipelines[pipelineIndex] = RasterShader::createPipeline(creation);
        }
    }

    void RasterShaderUber::waitForPipelineCreation() {
        if (!pipelinesCreated) {
            for (std::unique_ptr<std::thread> &thread : pipelineThreads) {
                thread->join();
            }

            pipelineThreads.clear();
            pipelineThreadCreations.clear();
            vertexShader.reset();
            pixelShader.reset();
            pipelinesCreated = true;
        }
    }

    uint32_t RasterShaderUber::pipelineStateIndex(bool alphaBlend, bool culling, bool zCmp, bool zUpd, bool zDecal, bool cvgAdd) const {
        return
            (uint32_t(alphaBlend)   << 0) |
            (uint32_t(culling)      << 1) |
            (uint32_t(zCmp)         << 2) |
            (uint32_t(zUpd)         << 3) |
            (uint32_t(zDecal)       << 4) |
            (uint32_t(cvgAdd)       << 5);
    }

    const RenderPipeline *RasterShaderUber::getPipeline(bool alphaBlend, bool culling, bool zCmp, bool zUpd, bool zDecal, bool cvgAdd) const {
        // Force read and turn off writing on decal modes since those PSOs are not generated.
        if (zDecal) {
            zCmp = true;
            zUpd = false;
        }

        return pipelines[pipelineStateIndex(alphaBlend, culling, zCmp, zUpd, zDecal, cvgAdd)].get();
    }
};