//
// RT64
//

#include "rt64_raster_shader.h"

#include "xxHash/xxh3.h"

#include "shaders/RenderParams.hlsli.rw.h"
#include "shaders/RasterPSDynamic.hlsl.spirv.h"
#include "shaders/RasterPSDynamicMS.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstant.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantMS.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantFlat.hlsl.spirv.h"
#include "shaders/RasterPSSpecConstantFlatMS.hlsl.spirv.h"
#include "shaders/RasterVSDynamic.hlsl.spirv.h"
#include "shaders/RasterVSSpecConstant.hlsl.spirv.h"
#include "shaders/RasterVSSpecConstantFlat.hlsl.spirv.h"
#include "shaders/PostBlendDitherNoiseAddPS.hlsl.spirv.h"
#include "shaders/PostBlendDitherNoiseSubPS.hlsl.spirv.h"
#include "shaders/PostBlendDitherNoiseSubNegativePS.hlsl.spirv.h"
#ifdef _WIN32
#   include "shaders/RasterPSLibrary.hlsl.dxil.h"
#   include "shaders/RasterPSLibraryMS.hlsl.dxil.h"
#   include "shaders/RasterVSLibrary.hlsl.dxil.h"
#   include "shaders/RasterPSDynamic.hlsl.dxil.h"
#   include "shaders/RasterPSDynamicMS.hlsl.dxil.h"
#   include "shaders/RasterVSDynamic.hlsl.dxil.h"
#   include "shaders/PostBlendDitherNoiseAddPS.hlsl.dxil.h"
#   include "shaders/PostBlendDitherNoiseSubPS.hlsl.dxil.h"
#   include "shaders/PostBlendDitherNoiseSubNegativePS.hlsl.dxil.h"
#elif defined(__APPLE__)
#   include "shaders/RasterPSDynamic.hlsl.metal.h"
#   include "shaders/RasterPSDynamicMS.hlsl.metal.h"
#   include "shaders/RasterPSSpecConstant.hlsl.metal.h"
#   include "shaders/RasterPSSpecConstantMS.hlsl.metal.h"
#   include "shaders/RasterPSSpecConstantFlat.hlsl.metal.h"
#   include "shaders/RasterPSSpecConstantFlatMS.hlsl.metal.h"
#   include "shaders/RasterVSDynamic.hlsl.metal.h"
#   include "shaders/RasterVSSpecConstant.hlsl.metal.h"
#   include "shaders/RasterVSSpecConstantFlat.hlsl.metal.h"
#   include "shaders/PostBlendDitherNoiseAddPS.hlsl.metal.h"
#   include "shaders/PostBlendDitherNoiseSubPS.hlsl.metal.h"
#   include "shaders/PostBlendDitherNoiseSubNegativePS.hlsl.metal.h"
#endif
#include "shared/rt64_raster_params.h"

#include "rt64_descriptor_sets.h"
#include "rt64_render_target.h"

namespace RT64 {
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

    // OptimizerCacheSPIRV

    void OptimizerCacheSPIRV::initialize() {
        rasterVS.parse(RasterVSSpecConstantBlobSPIRV, std::size(RasterVSSpecConstantBlobSPIRV));
        rasterVSFlat.parse(RasterVSSpecConstantFlatBlobSPIRV, std::size(RasterVSSpecConstantFlatBlobSPIRV));
        rasterPS.parse(RasterPSSpecConstantBlobSPIRV, std::size(RasterPSSpecConstantBlobSPIRV));
        rasterPSMS.parse(RasterPSSpecConstantMSBlobSPIRV, std::size(RasterPSSpecConstantMSBlobSPIRV));
        rasterPSFlat.parse(RasterPSSpecConstantFlatBlobSPIRV, std::size(RasterPSSpecConstantFlatBlobSPIRV));
        rasterPSFlatMS.parse(RasterPSSpecConstantFlatMSBlobSPIRV, std::size(RasterPSSpecConstantFlatMSBlobSPIRV));
        assert(!rasterVS.empty());
        assert(!rasterVSFlat.empty());
        assert(!rasterPS.empty());
        assert(!rasterPSMS.empty());
        assert(!rasterPSFlat.empty());
        assert(!rasterPSFlatMS.empty());
    }

    // RasterShader

    RasterShader::RasterShader(RenderDevice *device, const ShaderDescription &desc, const RenderPipelineLayout *pipelineLayout, RenderShaderFormat shaderFormat, const RenderMultisampling &multisampling, 
        const ShaderCompiler *shaderCompiler, const OptimizerCacheSPIRV *optimizerCacheSPIRV)
    {
        assert(device != nullptr);

        this->device = device;
        this->desc = desc;
        
        const bool useMSAA = (multisampling.sampleCount > 1);
        std::unique_ptr<RenderShader> vertexShader;
        std::unique_ptr<RenderShader> pixelShader;
#ifdef __APPLE__
        std::vector<RenderSpecConstant> specConstants;
#endif
        if (shaderFormat == RenderShaderFormat::SPIRV) {
            // Choose the pre-compiled shader permutations.
            const respv::Shader *VS = nullptr;
            const respv::Shader *PS = nullptr;
            VS = desc.flags.smoothShade ? &optimizerCacheSPIRV->rasterVS : &optimizerCacheSPIRV->rasterVSFlat;

            // Pick the correct SPIR-V based on the configuration.
            if (desc.flags.smoothShade) {
                PS = useMSAA ? &optimizerCacheSPIRV->rasterPSMS : &optimizerCacheSPIRV->rasterPS;
            }
            else {
                PS = useMSAA ? &optimizerCacheSPIRV->rasterPSFlatMS : &optimizerCacheSPIRV->rasterPSFlat;
            }

            thread_local std::vector<respv::SpecConstant> specConstants;
            thread_local bool specConstantsSetup = false;
            thread_local std::vector<uint8_t> optimizedVS;
            thread_local std::vector<uint8_t> optimizedPS;
            if (!specConstantsSetup) {
                for (uint32_t i = 0; i < 5; i++) {
                    specConstants.push_back(respv::SpecConstant(i, { 0 }));
                }

                specConstantsSetup = true;
            }
            
            specConstants[0].values[0] = desc.otherMode.L;
            specConstants[1].values[0] = desc.otherMode.H;
            specConstants[2].values[0] = desc.colorCombiner.L;
            specConstants[3].values[0] = desc.colorCombiner.H;
            specConstants[4].values[0] = desc.flags.value;
            
            bool vsRun = respv::Optimizer::run(*VS, specConstants.data(), uint32_t(specConstants.size()), optimizedVS);
            bool psRun = respv::Optimizer::run(*PS, specConstants.data(), uint32_t(specConstants.size()), optimizedPS);
            assert(vsRun && psRun && "Shader optimization must always succeed as the inputs are always the same.");

            vertexShader = device->createShader(optimizedVS.data(), optimizedVS.size(), "VSMain", shaderFormat);
            pixelShader = device->createShader(optimizedPS.data(), optimizedPS.size(), "PSMain", shaderFormat);
        }
        else if (shaderFormat == RenderShaderFormat::METAL) {
#       ifdef __APPLE__
            // Choose the pre-compiled shader permutations.
            const void *VSBlob = nullptr;
            const void *PSBlob = nullptr;
            uint32_t VSBlobSize = 0;
            uint32_t PSBlobSize = 0;
            if (desc.flags.smoothShade) {
                VSBlob = RasterVSSpecConstantBlobMSL;
                VSBlobSize = uint32_t(std::size(RasterVSSpecConstantBlobMSL));
            }
            else {
                VSBlob = RasterVSSpecConstantFlatBlobMSL;
                VSBlobSize = uint32_t(std::size(RasterVSSpecConstantFlatBlobMSL));
            }

            // Pick the correct MSL based on the configuration.
            if (desc.flags.smoothShade) {
                PSBlob = useMSAA ? RasterPSSpecConstantMSBlobMSL : RasterPSSpecConstantBlobMSL;
                PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSSpecConstantMSBlobMSL) : std::size(RasterPSSpecConstantBlobMSL));
            }
            else {
                PSBlob = useMSAA ? RasterPSSpecConstantFlatMSBlobMSL : RasterPSSpecConstantFlatBlobMSL;
                PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSSpecConstantFlatMSBlobMSL) : std::size(RasterPSSpecConstantFlatBlobMSL));
            }

            vertexShader = device->createShader(VSBlob, VSBlobSize, "VSMain", shaderFormat);
            pixelShader = device->createShader(PSBlob, PSBlobSize, "PSMain", shaderFormat);

            // Spec constants should replace the constants embedded in the shader directly.
            specConstants.emplace_back(0, desc.otherMode.L);
            specConstants.emplace_back(1, desc.otherMode.H);
            specConstants.emplace_back(2, desc.colorCombiner.L);
            specConstants.emplace_back(3, desc.colorCombiner.H);
            specConstants.emplace_back(4, desc.flags.value);
#       else
            assert(false && "This platform does not support METAL shaders.");
#       endif
        }
        else {
#       if defined(_WIN32)
            RasterShaderText shaderText = generateShaderText(desc, useMSAA);

            // Compile both shaders from text with the constants hard-coded in.
            static const wchar_t *blobVSLibraryNames[] = { L"RasterVSEntry", L"RasterVSLibrary" };
            static const wchar_t *blobPSLibraryNames[] = { L"RasterPSEntry", L"RasterPSLibrary" };
            IDxcBlob *blobVSLibraries[] = { nullptr, nullptr };
            IDxcBlob *blobPSLibraries[] = { nullptr, nullptr };
            shaderCompiler->dxcUtils->CreateBlobFromPinned(RasterVSLibraryBlobDXIL, sizeof(RasterVSLibraryBlobDXIL), DXC_CP_ACP, (IDxcBlobEncoding **)(&blobVSLibraries[1]));

            const void *PSLibraryBlob = useMSAA ? RasterPSLibraryMSBlobDXIL : RasterPSLibraryBlobDXIL;
            uint32_t PSLibraryBlobSize = useMSAA ? sizeof(RasterPSLibraryMSBlobDXIL) : sizeof(RasterPSLibraryBlobDXIL);
            shaderCompiler->dxcUtils->CreateBlobFromPinned(PSLibraryBlob, PSLibraryBlobSize, DXC_CP_ACP, (IDxcBlobEncoding **)(&blobPSLibraries[1]));
                
            // Compile both the vertex and pixel shader functions as libraries.
            const std::wstring VertexShaderName = L"VSMain";
            const std::wstring PixelShaderName = L"PSMain";
            shaderCompiler->compile(shaderText.vertexShader, VertexShaderName, L"lib_6_3", shaderFormat, &blobVSLibraries[0]);
            shaderCompiler->compile(shaderText.pixelShader, PixelShaderName, L"lib_6_3", shaderFormat, &blobPSLibraries[0]);

            // Link the vertex and pixel shaders with the libraries that define their main functions.
            IDxcBlob *blobVS = nullptr;
            IDxcBlob *blobPS = nullptr;
            shaderCompiler->link(VertexShaderName, L"vs_6_3", blobVSLibraries, blobVSLibraryNames, std::size(blobVSLibraries), &blobVS);
            shaderCompiler->link(PixelShaderName, L"ps_6_3", blobPSLibraries, blobPSLibraryNames, std::size(blobPSLibraries), &blobPS);

            vertexShader = device->createShader(blobVS->GetBufferPointer(), blobVS->GetBufferSize(), "VSMain", shaderFormat);
            pixelShader = device->createShader(blobPS->GetBufferPointer(), blobPS->GetBufferSize(), "PSMain", shaderFormat);

            // Blobs can be discarded once the shaders are created.
            blobVSLibraries[0]->Release();
            blobVSLibraries[1]->Release();
            blobPSLibraries[0]->Release();
            blobPSLibraries[1]->Release();
            blobPS->Release();
            blobVS->Release();
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
        creation.zCmp = !copyMode && desc.otherMode.zCmp() && (desc.otherMode.zMode() != ZMODE_DEC);
        creation.zUpd = !copyMode && desc.otherMode.zUpd();
        creation.cvgAdd = (desc.otherMode.cvgDst() == CVG_DST_WRAP) || (desc.otherMode.cvgDst() == CVG_DST_SAVE);
        creation.NoN = desc.flags.NoN;
        creation.usesHDR = desc.flags.usesHDR;
#ifdef __APPLE__
        creation.specConstants = specConstants;
#endif
        creation.multisampling = multisampling;
        pipeline = createPipeline(creation);
    }

    RasterShader::~RasterShader() { }

    RasterShaderText RasterShader::generateShaderText(const ShaderDescription &desc, bool multisampling) {
        const std::string renderParamsCode = desc.toShader();

        // Generate vertex shader.
        std::stringstream vss;
        vss << std::string_view(RenderParamsText, sizeof(RenderParamsText));
        vss << "RenderParams getRenderParams() {" + renderParamsCode + "; return rp; }";
        vss <<
            "void RasterVS(const RenderParams, in float4, in float2, in float4, out float4, out float2, out float4, out float4);"
            "[shader(\"vertex\")]"
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
        pss << std::string_view(RenderParamsText, sizeof(RenderParamsText));
        pss << "RenderParams getRenderParams() {" + renderParamsCode + "; return rp; }";
        pss <<
            "bool RasterPS(const RenderParams, float4, float2, float4, float4, bool, out float4, out float4);"
            "[shader(\"pixel\")]"
            "void PSMain("
            "  in float4 vertexPosition : SV_POSITION"
            ", in float2 vertexUV : TEXCOORD"
            ", in float4 vertexSmoothColor : COLOR0";

        if (!desc.flags.smoothShade) {
            pss << ", nointerpolation in float4 vertexFlatColor : COLOR1";
        }

        pss <<
            ", out float4 pixelColor : SV_TARGET0"
            ", out float4 pixelAlpha : SV_TARGET1"
            ") {";

        if (desc.flags.smoothShade) {
            pss << "float4 vertexFlatColor = vertexSmoothColor;";
        }
        
        pss <<
            "   float4 resultColor;"
            "   float4 resultAlpha;"
            "   if (!RasterPS(getRenderParams(), vertexPosition, vertexUV, vertexSmoothColor, vertexFlatColor, false, resultColor, resultAlpha)) discard;"
            "   pixelColor = resultColor;"
            "   pixelAlpha = resultAlpha;"
            "}";

        return { vss.str(), pss.str() };
    }
    
    std::unique_ptr<RenderPipeline> RasterShader::createPipeline(const PipelineCreation &c) {
        RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(c.usesHDR);
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.cullMode = c.culling ? RenderCullMode::FRONT : RenderCullMode::NONE;
        pipelineDesc.depthClipEnabled = !c.NoN;
        pipelineDesc.depthEnabled = c.zCmp || c.zUpd;
        pipelineDesc.depthFunction = c.zCmp ? RenderComparisonFunction::LESS : RenderComparisonFunction::ALWAYS;
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

#if defined(_WIN32)
    const uint64_t RasterShaderUber::RasterVSLibraryHash = XXH3_64bits(RasterVSLibraryBlobDXIL, sizeof(RasterVSLibraryBlobDXIL));
    const uint64_t RasterShaderUber::RasterPSLibraryHash = XXH3_64bits(RasterPSLibraryBlobDXIL, sizeof(RasterPSLibraryBlobDXIL));
#else
    // Shader hashes are not required in other platforms as they don't use a shader cache.
    const uint64_t RasterShaderUber::RasterVSLibraryHash = 0;
    const uint64_t RasterShaderUber::RasterPSLibraryHash = 0;
#endif

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
#   ifdef __APPLE__
        case RenderShaderFormat::METAL:
            VSBlob = RasterVSDynamicBlobMSL;
            PSBlob = useMSAA ? RasterPSDynamicMSBlobMSL : RasterPSDynamicBlobMSL;
            VSBlobSize = uint32_t(std::size(RasterVSDynamicBlobMSL));
            PSBlobSize = uint32_t(useMSAA ? std::size(RasterPSDynamicMSBlobMSL) : std::size(RasterPSDynamicBlobMSL));
            break;
#   endif
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
        uint32_t pipelineCount = uint32_t(std::size(pipelines));
        pipelineThreadCreations.clear();
        pipelineThreadCreations.resize(std::min(threadCount, pipelineCount));

        PipelineCreation creation;
        creation.device = device;
        creation.pipelineLayout = pipelineLayout.get();
        creation.vertexShader = vertexShader.get();
        creation.pixelShader = pixelShader.get();
        creation.alphaBlend = true;
        creation.culling = false;
        creation.NoN = true;
        creation.usesHDR = shaderLibrary->usesHDR;
        creation.multisampling = multisampling;

        uint32_t threadIndex = 0;
        for (uint32_t i = 0; i < pipelineCount; i++) {
            creation.zCmp = i & (1 << 0);
            creation.zUpd = i & (1 << 1);
            creation.cvgAdd = i & (1 << 2);

            pipelineThreadCreations[threadIndex].emplace_back(creation);
            threadIndex = (threadIndex + 1) % threadCount;
        }

        // Spawn the threads that will compile all the pipelines.
        pipelineThreads.clear();
        pipelineThreads.resize(pipelineThreadCreations.size());
        for (uint32_t i = 0; i < uint32_t(pipelineThreads.size()); i++) {
            pipelineThreads[i] = std::make_unique<std::thread>(&RasterShaderUber::threadCreatePipelines, this, i);
        }

        // Create the pipelines for post blend operations.
        std::unique_ptr<RenderShader> postBlendAddPixelShader;
        std::unique_ptr<RenderShader> postBlendSubPixelShader;
        std::unique_ptr<RenderShader> postBlendSubNegativePixelShader;
        switch (shaderFormat) {
#   ifdef _WIN32
        case RenderShaderFormat::DXIL:
            postBlendAddPixelShader = device->createShader(PostBlendDitherNoiseAddPSBlobDXIL, std::size(PostBlendDitherNoiseAddPSBlobDXIL), "PSMain", shaderFormat);
            postBlendSubPixelShader = device->createShader(PostBlendDitherNoiseSubPSBlobDXIL, std::size(PostBlendDitherNoiseSubPSBlobDXIL), "PSMain", shaderFormat);
            postBlendSubNegativePixelShader = device->createShader(PostBlendDitherNoiseSubNegativePSBlobDXIL, std::size(PostBlendDitherNoiseSubNegativePSBlobDXIL), "PSMain", shaderFormat);
            break;
#   endif
        case RenderShaderFormat::SPIRV:
            postBlendAddPixelShader = device->createShader(PostBlendDitherNoiseAddPSBlobSPIRV, std::size(PostBlendDitherNoiseAddPSBlobSPIRV), "PSMain", shaderFormat);
            postBlendSubPixelShader = device->createShader(PostBlendDitherNoiseSubPSBlobSPIRV, std::size(PostBlendDitherNoiseSubPSBlobSPIRV), "PSMain", shaderFormat);
            postBlendSubNegativePixelShader = device->createShader(PostBlendDitherNoiseSubNegativePSBlobSPIRV, std::size(PostBlendDitherNoiseSubNegativePSBlobSPIRV), "PSMain", shaderFormat);
            break;
#   ifdef __APPLE__
        case RenderShaderFormat::METAL:
            postBlendAddPixelShader = device->createShader(PostBlendDitherNoiseAddPSBlobMSL, std::size(PostBlendDitherNoiseAddPSBlobMSL), "PSMain", shaderFormat);
            postBlendSubPixelShader = device->createShader(PostBlendDitherNoiseSubPSBlobMSL, std::size(PostBlendDitherNoiseSubPSBlobMSL), "PSMain", shaderFormat);
            postBlendSubNegativePixelShader = device->createShader(PostBlendDitherNoiseSubNegativePSBlobMSL, std::size(PostBlendDitherNoiseSubNegativePSBlobMSL), "PSMain", shaderFormat);
            break;
#   endif
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

        postBlendDesc.pixelShader = postBlendSubNegativePixelShader.get();
        postBlendDitherNoiseSubNegativePipeline = device->createGraphicsPipeline(postBlendDesc);
    }

    RasterShaderUber::~RasterShaderUber() {
        waitForPipelineCreation();
    }

    void RasterShaderUber::threadCreatePipelines(uint32_t threadIndex) {
        // Delay the creation of all other pipelines until the first pipeline is created. This can help the
        // driver reuse its shader cache between pipelines and achieve a much lower creation time than if
        // all threads started at the same time.
        if (threadIndex > 0) {
            std::unique_lock<std::mutex> lock(firstPipelineMutex);
            firstPipelineCondition.wait(lock, [this]() {
                return (pipelines[0] != nullptr);
            });
        }

        for (const PipelineCreation &creation : pipelineThreadCreations[threadIndex]) {
            uint32_t pipelineIndex = pipelineStateIndex(creation.zCmp, creation.zUpd, creation.cvgAdd);

            if (pipelineIndex == 0) {
                firstPipelineMutex.lock();
                pipelines[pipelineIndex] = RasterShader::createPipeline(creation);
                firstPipelineMutex.unlock();
                firstPipelineCondition.notify_all();
            }
            else {
                pipelines[pipelineIndex] = RasterShader::createPipeline(creation);
            }
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

    uint32_t RasterShaderUber::pipelineStateIndex(bool zCmp, bool zUpd, bool cvgAdd) const {
        return
            (uint32_t(zCmp)         << 0) |
            (uint32_t(zUpd)         << 1) |
            (uint32_t(cvgAdd)       << 2);
    }

    const RenderPipeline *RasterShaderUber::getPipeline(bool zCmp, bool zUpd, bool cvgAdd) const {
        return pipelines[pipelineStateIndex(zCmp, zUpd, cvgAdd)].get();
    }
};
