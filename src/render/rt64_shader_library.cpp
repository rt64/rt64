//
// RT64
//

#include "rt64_shader_library.h"

#include "common/rt64_common.h"
#include "shared/rt64_render_target_copy.h"
#include "shared/rt64_rsp_vertex_test_z.h"

#include "shaders/FbChangesClearCS.hlsl.spirv.h"
#include "shaders/FbChangesDrawColorPS.hlsl.spirv.h"
#include "shaders/FbChangesDrawDepthPS.hlsl.spirv.h"
#include "shaders/FbReadAnyChangesCS.hlsl.spirv.h"
#include "shaders/FbReadAnyFullCS.hlsl.spirv.h"
#include "shaders/FbReinterpretCS.hlsl.spirv.h"
#include "shaders/FbWriteColorCS.hlsl.spirv.h"
#include "shaders/FbWriteDepthCS.hlsl.spirv.h"
#include "shaders/FbWriteDepthCSMS.hlsl.spirv.h"
#include "shaders/GaussianFilterRGB3x3CS.hlsl.spirv.h"
#include "shaders/BoxFilterCS.hlsl.spirv.h"
#include "shaders/BicubicScalingCS.hlsl.spirv.h"
#include "shaders/HistogramAverageCS.hlsl.spirv.h"
#include "shaders/HistogramClearCS.hlsl.spirv.h"
#include "shaders/HistogramSetCS.hlsl.spirv.h"
#include "shaders/IdleCS.hlsl.spirv.h"
#include "shaders/LuminanceHistogramCS.hlsl.spirv.h"
#include "shaders/RSPModifyCS.hlsl.spirv.h"
#include "shaders/RSPProcessCS.hlsl.spirv.h"
#include "shaders/RSPSmoothNormalCS.hlsl.spirv.h"
#include "shaders/RSPVertexTestZCS.hlsl.spirv.h"
#include "shaders/RSPVertexTestZCSMS.hlsl.spirv.h"
#include "shaders/RSPWorldCS.hlsl.spirv.h"
#include "shaders/RtCopyColorToDepthPS.hlsl.spirv.h"
#include "shaders/RtCopyColorToDepth2XPS.hlsl.spirv.h"
#include "shaders/RtCopyColorToDepth4XPS.hlsl.spirv.h"
#include "shaders/RtCopyColorToDepth8XPS.hlsl.spirv.h"
#include "shaders/RtCopyDepthToColorPS.hlsl.spirv.h"
#include "shaders/RtCopyDepthToColor2XPS.hlsl.spirv.h"
#include "shaders/RtCopyDepthToColor4XPS.hlsl.spirv.h"
#include "shaders/RtCopyDepthToColor8XPS.hlsl.spirv.h"
#include "shaders/TextureCopyPS.hlsl.spirv.h"
#include "shaders/TextureDecodeCS.hlsl.spirv.h"
#include "shaders/TextureResolveSamples2XPS.hlsl.spirv.h"
#include "shaders/TextureResolveSamples4XPS.hlsl.spirv.h"
#include "shaders/TextureResolveSamples8XPS.hlsl.spirv.h"
#include "shaders/VideoInterfacePSRegular.hlsl.spirv.h"
#include "shaders/VideoInterfacePSPixel.hlsl.spirv.h"
#include "shaders/FullScreenVS.hlsl.spirv.h"
#include "shaders/Im3DVS.hlsl.spirv.h"
#include "shaders/ComposePS.hlsl.spirv.h"
#include "shaders/DebugPS.hlsl.spirv.h"
#include "shaders/Im3DPS.hlsl.spirv.h"
#include "shaders/PostProcessPS.hlsl.spirv.h"

#ifdef _WIN32
#   include "shaders/FbChangesClearCS.hlsl.dxil.h"
#   include "shaders/FbChangesDrawColorPS.hlsl.dxil.h"
#   include "shaders/FbChangesDrawDepthPS.hlsl.dxil.h"
#   include "shaders/FbReadAnyChangesCS.hlsl.dxil.h"
#   include "shaders/FbReadAnyFullCS.hlsl.dxil.h"
#   include "shaders/FbReinterpretCS.hlsl.dxil.h"
#   include "shaders/FbWriteColorCS.hlsl.dxil.h"
#   include "shaders/FbWriteDepthCS.hlsl.dxil.h"
#   include "shaders/FbWriteDepthCSMS.hlsl.dxil.h"
#   include "shaders/GaussianFilterRGB3x3CS.hlsl.dxil.h"
#   include "shaders/BoxFilterCS.hlsl.dxil.h"
#   include "shaders/BicubicScalingCS.hlsl.dxil.h"
#   include "shaders/HistogramAverageCS.hlsl.dxil.h"
#   include "shaders/HistogramClearCS.hlsl.dxil.h"
#   include "shaders/HistogramSetCS.hlsl.dxil.h"
#   include "shaders/IdleCS.hlsl.dxil.h"
#   include "shaders/LuminanceHistogramCS.hlsl.dxil.h"
#   include "shaders/RSPModifyCS.hlsl.dxil.h"
#   include "shaders/RSPProcessCS.hlsl.dxil.h"
#   include "shaders/RSPSmoothNormalCS.hlsl.dxil.h"
#   include "shaders/RSPVertexTestZCS.hlsl.dxil.h"
#   include "shaders/RSPVertexTestZCSMS.hlsl.dxil.h"
#   include "shaders/RSPWorldCS.hlsl.dxil.h"
#   include "shaders/RtCopyColorToDepthPS.hlsl.dxil.h"
#   include "shaders/RtCopyColorToDepth2XPS.hlsl.dxil.h"
#   include "shaders/RtCopyColorToDepth4XPS.hlsl.dxil.h"
#   include "shaders/RtCopyColorToDepth8XPS.hlsl.dxil.h"
#   include "shaders/RtCopyDepthToColorPS.hlsl.dxil.h"
#   include "shaders/RtCopyDepthToColor2XPS.hlsl.dxil.h"
#   include "shaders/RtCopyDepthToColor4XPS.hlsl.dxil.h"
#   include "shaders/RtCopyDepthToColor8XPS.hlsl.dxil.h"
#   include "shaders/TextureCopyPS.hlsl.dxil.h"
#   include "shaders/TextureDecodeCS.hlsl.dxil.h"
#   include "shaders/TextureResolveSamples2XPS.hlsl.dxil.h"
#   include "shaders/TextureResolveSamples4XPS.hlsl.dxil.h"
#   include "shaders/TextureResolveSamples8XPS.hlsl.dxil.h"
#   include "shaders/VideoInterfacePSRegular.hlsl.dxil.h"
#   include "shaders/VideoInterfacePSPixel.hlsl.dxil.h"
#   include "shaders/FullScreenVS.hlsl.dxil.h"
#   include "shaders/Im3DVS.hlsl.dxil.h"
#   include "shaders/ComposePS.hlsl.dxil.h"
#   include "shaders/DebugPS.hlsl.dxil.h"
#   include "shaders/Im3DPS.hlsl.dxil.h"
#   include "shaders/PostProcessPS.hlsl.dxil.h"
#elif defined(__APPLE__)
#   include "shaders/FbChangesClearCS.hlsl.metal.h"
#   include "shaders/FbChangesDrawColorPS.hlsl.metal.h"
#   include "shaders/FbChangesDrawDepthPS.hlsl.metal.h"
#   include "shaders/FbReadAnyChangesCS.hlsl.metal.h"
#   include "shaders/FbReadAnyFullCS.hlsl.metal.h"
#   include "shaders/FbReinterpretCS.hlsl.metal.h"
#   include "shaders/FbWriteColorCS.hlsl.metal.h"
#   include "shaders/FbWriteDepthCS.hlsl.metal.h"
#   include "shaders/FbWriteDepthCSMS.hlsl.metal.h"
#   include "shaders/GaussianFilterRGB3x3CS.hlsl.metal.h"
#   include "shaders/BoxFilterCS.hlsl.metal.h"
#   include "shaders/BicubicScalingCS.hlsl.metal.h"
#   include "shaders/HistogramAverageCS.hlsl.metal.h"
#   include "shaders/HistogramClearCS.hlsl.metal.h"
#   include "shaders/HistogramSetCS.hlsl.metal.h"
#   include "shaders/IdleCS.hlsl.metal.h"
#   include "shaders/LuminanceHistogramCS.hlsl.metal.h"
#   include "shaders/RSPModifyCS.hlsl.metal.h"
#   include "shaders/RSPProcessCS.hlsl.metal.h"
#   include "shaders/RSPSmoothNormalCS.hlsl.metal.h"
#   include "shaders/RSPVertexTestZCS.hlsl.metal.h"
#   include "shaders/RSPVertexTestZCSMS.hlsl.metal.h"
#   include "shaders/RSPWorldCS.hlsl.metal.h"
#   include "shaders/RtCopyColorToDepthPS.hlsl.metal.h"
#   include "shaders/RtCopyColorToDepth2XPS.hlsl.metal.h"
#   include "shaders/RtCopyColorToDepth4XPS.hlsl.metal.h"
#   include "shaders/RtCopyColorToDepth8XPS.hlsl.metal.h"
#   include "shaders/RtCopyDepthToColorPS.hlsl.metal.h"
#   include "shaders/RtCopyDepthToColor2XPS.hlsl.metal.h"
#   include "shaders/RtCopyDepthToColor4XPS.hlsl.metal.h"
#   include "shaders/RtCopyDepthToColor8XPS.hlsl.metal.h"
#   include "shaders/TextureCopyPS.hlsl.metal.h"
#   include "shaders/TextureDecodeCS.hlsl.metal.h"
#   include "shaders/TextureResolveSamples2XPS.hlsl.metal.h"
#   include "shaders/TextureResolveSamples4XPS.hlsl.metal.h"
#   include "shaders/TextureResolveSamples8XPS.hlsl.metal.h"
#   include "shaders/VideoInterfacePSRegular.hlsl.metal.h"
#   include "shaders/VideoInterfacePSPixel.hlsl.metal.h"
#   include "shaders/FullScreenVS.hlsl.metal.h"
#   include "shaders/Im3DVS.hlsl.metal.h"
#   include "shaders/ComposePS.hlsl.metal.h"
#   include "shaders/DebugPS.hlsl.metal.h"
#   include "shaders/Im3DPS.hlsl.metal.h"
#   include "shaders/PostProcessPS.hlsl.metal.h"
#endif

#include "shared/rt64_fb_common.h"
#include "shared/rt64_fb_reinterpret.h"
#include "shared/rt64_texture_copy.h"
#include "shared/rt64_video_interface.h"

#include "rt64_descriptor_sets.h"
#include "rt64_render_target.h"

#ifdef _WIN32
#   define CREATE_SHADER_INPUTS(DXIL_BLOB, SPIRV_BLOB, MSL_BLOB, ENTRY_NAME, SHADER_FORMAT)\
        (SHADER_FORMAT == RenderShaderFormat::DXIL) ? DXIL_BLOB : (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? SPIRV_BLOB : nullptr,\
        (SHADER_FORMAT == RenderShaderFormat::DXIL) ? sizeof(DXIL_BLOB) : (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? sizeof(SPIRV_BLOB) : 0,\
        ENTRY_NAME,\
        SHADER_FORMAT
#elif defined(__APPLE__)
#   define CREATE_SHADER_INPUTS(DXIL_BLOB, SPIRV_BLOB, MSL_BLOB, ENTRY_NAME, SHADER_FORMAT)\
        (SHADER_FORMAT == RenderShaderFormat::METAL) ? MSL_BLOB : (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? SPIRV_BLOB : nullptr,\
        (SHADER_FORMAT == RenderShaderFormat::METAL) ? sizeof(MSL_BLOB) : (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? sizeof(SPIRV_BLOB) : 0,\
        ENTRY_NAME,\
        SHADER_FORMAT
#else
#   define CREATE_SHADER_INPUTS(DXIL_BLOB, SPIRV_BLOB, MSL_BLOB, ENTRY_NAME, SHADER_FORMAT)\
        (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? SPIRV_BLOB : nullptr,\
        (SHADER_FORMAT == RenderShaderFormat::SPIRV) ? sizeof(SPIRV_BLOB) : 0,\
        ENTRY_NAME,\
        SHADER_FORMAT
#endif

namespace RT64 {
    // ShaderLibrary

    ShaderLibrary::ShaderLibrary(bool usesHDR, bool usesHardwareResolve) {
        this->usesHDR = usesHDR;
        this->usesHardwareResolve = usesHardwareResolve;
    }

    ShaderLibrary::~ShaderLibrary() { }

    void ShaderLibrary::setupCommonShaders(RenderInterface *rhi, RenderDevice *device) {
        assert(rhi != nullptr);
        assert(device != nullptr);

        // Retrieve the shader format used by the RHI.
        const RenderInterfaceCapabilities interfaceCapabilities = rhi->getCapabilities();
        const RenderDeviceCapabilities deviceCapabilities = device->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;
        RenderPipelineLayoutBuilder layoutBuilder;

        // Create shaders shared across all pipelines.
        std::unique_ptr<RenderShader> fullScreenVertexShader = device->createShader(CREATE_SHADER_INPUTS(FullScreenVSBlobDXIL, FullScreenVSBlobSPIRV, FullScreenVSBlobMSL, "VSMain", shaderFormat));
        
        auto fillSamplerSet = [&](SamplerSet &set, RenderFilter filter, bool anisotropyEnabled) {
            RenderSamplerDesc samplerDesc;
            samplerDesc.anisotropyEnabled = anisotropyEnabled;
            samplerDesc.minFilter = filter;
            samplerDesc.magFilter = filter;
            samplerDesc.addressW = RenderTextureAddressMode::CLAMP;

            // This matches the bias used in the shader when native samplers are not used.
            samplerDesc.mipLODBias = -0.25f;

            // WRAP.
            samplerDesc.addressU = RenderTextureAddressMode::WRAP;
            samplerDesc.addressV = RenderTextureAddressMode::WRAP;
            set.wrapWrap = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::WRAP;
            samplerDesc.addressV = RenderTextureAddressMode::MIRROR;
            set.wrapMirror = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::WRAP;
            samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
            set.wrapClamp = device->createSampler(samplerDesc);

            // MIRROR.
            samplerDesc.addressU = RenderTextureAddressMode::MIRROR;
            samplerDesc.addressV = RenderTextureAddressMode::WRAP;
            set.mirrorWrap = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::MIRROR;
            samplerDesc.addressV = RenderTextureAddressMode::MIRROR;
            set.mirrorMirror = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::MIRROR;
            samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
            set.mirrorClamp = device->createSampler(samplerDesc);

            // CLAMP.
            samplerDesc.addressU = RenderTextureAddressMode::CLAMP;
            samplerDesc.addressV = RenderTextureAddressMode::WRAP;
            set.clampWrap = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::CLAMP;
            samplerDesc.addressV = RenderTextureAddressMode::MIRROR;
            set.clampMirror = device->createSampler(samplerDesc);

            samplerDesc.addressU = RenderTextureAddressMode::CLAMP;
            samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
            set.clampClamp = device->createSampler(samplerDesc);

            // BORDER.
            samplerDesc.addressU = RenderTextureAddressMode::BORDER;
            samplerDesc.addressV = RenderTextureAddressMode::BORDER;
            set.borderBorder = device->createSampler(samplerDesc);
        };

        fillSamplerSet(samplerLibrary.nearest, RenderFilter::NEAREST, false);
        fillSamplerSet(samplerLibrary.linear, RenderFilter::LINEAR, true);

        // Bicubic scaling.
        {
            BicubicScalingDescriptorSet descriptorSet(samplerLibrary);
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 4, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            bicubicScaling.pipelineLayout = layoutBuilder.create(device);
            
            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(BicubicScalingCSBlobDXIL, BicubicScalingCSBlobSPIRV, BicubicScalingCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(bicubicScaling.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            bicubicScaling.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Box filter.
        {
            BoxFilterDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 6, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            boxFilter.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(BoxFilterCSBlobDXIL, BoxFilterCSBlobSPIRV, BoxFilterCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(boxFilter.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            boxFilter.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Raytracing compose.
        {
            RaytracingComposeDescriptorSet descriptorSet(samplerLibrary);
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            compose.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> pixelShader = device->createShader(CREATE_SHADER_INPUTS(ComposePSBlobDXIL, ComposePSBlobSPIRV, ComposePSBlobMSL, "PSMain", shaderFormat));
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = compose.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
            pipelineDesc.renderTargetFormat[0] = RenderFormat::R32G32B32A32_FLOAT;
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = pixelShader.get();
            compose.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }

        /*
        RT64_LOG_PRINTF("Creating the Im3d pipeline state");
        {
            im3dTriangle.reflection.setupShaders(dxcUtils, { SHADER_COMPILER_BLOB(Im3DVSBlob), SHADER_COMPILER_BLOB(Im3DPSBlob) });
            im3dTriangle.rootSignature.addDescriptorTable(FramebufferRendererDescriptorHeap::Definition);

            im3dPoint.reflection.setupShaders(dxcUtils, { SHADER_COMPILER_BLOB(Im3DVSBlob), SHADER_COMPILER_BLOB(Im3DPSBlob), SHADER_COMPILER_BLOB(Im3DGSPointsBlob) });
            im3dPoint.rootSignature.addDescriptorTable(FramebufferRendererDescriptorHeap::Definition);

            im3dLine.reflection.setupShaders(dxcUtils, { SHADER_COMPILER_BLOB(Im3DVSBlob), SHADER_COMPILER_BLOB(Im3DPSBlob), SHADER_COMPILER_BLOB(Im3DGSLinesBlob) });
            im3dLine.rootSignature.addDescriptorTable(FramebufferRendererDescriptorHeap::Definition);

            // Define the vertex input layout.
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION_SIZE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0  }
            };

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            setPsoDefaults(psoDesc, alphaBlendDesc, DXGI_FORMAT_R8G8B8A8_UNORM);

            psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            psoDesc.pRootSignature = im3dTriangle.rootSignature.generate(d3dDevice, false, true);
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(Im3DVSBlob, sizeof(Im3DVSBlob));
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(Im3DPSBlob, sizeof(Im3DPSBlob));

            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            D3D12_CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&im3dTriangle.pipelineState)));

            psoDesc.pRootSignature = im3dPoint.rootSignature.generate(d3dDevice, false, true);
            psoDesc.GS = CD3DX12_SHADER_BYTECODE(Im3DGSPointsBlob, sizeof(Im3DGSPointsBlob));
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            D3D12_CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&im3dPoint.pipelineState)));

            psoDesc.pRootSignature = im3dLine.rootSignature.generate(d3dDevice, false, true);
            psoDesc.GS = CD3DX12_SHADER_BYTECODE(Im3DGSLinesBlob, sizeof(Im3DGSLinesBlob));
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            D3D12_CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&im3dLine.pipelineState)));
        }
        */

        // Idle.
        {
            layoutBuilder.begin();
            layoutBuilder.end();
            idle.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(IdleCSBlobDXIL, IdleCSBlobSPIRV, IdleCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(idle.pipelineLayout.get(), computeShader.get(), 1, 1, 1);
            idle.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Framebuffer changes clear.
        {
            FramebufferClearChangesDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            fbChangesClear.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(FbChangesClearCSBlobDXIL, FbChangesClearCSBlobSPIRV, FbChangesClearCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(fbChangesClear.pipelineLayout.get(), computeShader.get(), 1, 1, 1);
            fbChangesClear.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Framebuffer read any changes and full.
        {
            FramebufferReadChangesDescriptorBufferSet descriptorBufferSet;
            FramebufferReadChangesDescriptorChangesSet descriptorChangesSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::FbCommonCB), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorBufferSet);
            layoutBuilder.addDescriptorSet(descriptorChangesSet);
            layoutBuilder.end();
            fbReadAnyChanges.pipelineLayout = layoutBuilder.create(device);
            fbReadAnyFull.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> anyChangesShader = device->createShader(CREATE_SHADER_INPUTS(FbReadAnyChangesCSBlobDXIL, FbReadAnyChangesCSBlobSPIRV, FbReadAnyChangesCSBlobMSL, "CSMain", shaderFormat));
            fbReadAnyChanges.pipeline = device->createComputePipeline(RenderComputePipelineDesc(fbReadAnyChanges.pipelineLayout.get(), anyChangesShader.get(), 8, 8, 1));

            std::unique_ptr<RenderShader> fullShader = device->createShader(CREATE_SHADER_INPUTS(FbReadAnyFullCSBlobDXIL, FbReadAnyFullCSBlobSPIRV, FbReadAnyFullCSBlobMSL, "CSMain", shaderFormat));
            fbReadAnyFull.pipeline = device->createComputePipeline(RenderComputePipelineDesc(fbReadAnyFull.pipelineLayout.get(), fullShader.get(), 8, 8, 1));
        }

        // Framebuffer Reinterpretation.
        {
            ReinterpretDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::FbReinterpretCB), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            fbReinterpret.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(FbReinterpretCSBlobDXIL, FbReinterpretCSBlobSPIRV, FbReinterpretCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(fbReinterpret.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            fbReinterpret.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Framebuffer write color or depth.
        {
            FramebufferWriteDescriptorBufferSet descriptorBufferSet;
            FramebufferWriteDescriptorTextureSet descriptorTextureSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::FbCommonCB), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorBufferSet);
            layoutBuilder.addDescriptorSet(descriptorTextureSet);
            layoutBuilder.end();
            fbWriteColor.pipelineLayout = layoutBuilder.create(device);
            fbWriteDepth.pipelineLayout = layoutBuilder.create(device);
            fbWriteDepthMS.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> colorShader = device->createShader(CREATE_SHADER_INPUTS(FbWriteColorCSBlobDXIL, FbWriteColorCSBlobSPIRV, FbWriteColorCSBlobMSL, "CSMain", shaderFormat));
            fbWriteColor.pipeline = device->createComputePipeline(RenderComputePipelineDesc(fbWriteColor.pipelineLayout.get(), colorShader.get(), 8, 8, 1));

            std::unique_ptr<RenderShader> depthShader = device->createShader(CREATE_SHADER_INPUTS(FbWriteDepthCSBlobDXIL, FbWriteDepthCSBlobSPIRV, FbWriteDepthCSBlobMSL, "CSMain", shaderFormat));
            fbWriteDepth.pipeline = device->createComputePipeline(RenderComputePipelineDesc(fbWriteDepth.pipelineLayout.get(), depthShader.get(), 8, 8, 1));

            std::unique_ptr<RenderShader> depthShaderMS = device->createShader(CREATE_SHADER_INPUTS(FbWriteDepthCSMSBlobDXIL, FbWriteDepthCSMSBlobSPIRV, FbWriteDepthCSMSBlobMSL, "CSMain", shaderFormat));
            fbWriteDepthMS.pipeline = device->createComputePipeline(RenderComputePipelineDesc(fbWriteDepthMS.pipelineLayout.get(), depthShaderMS.get(), 8, 8, 1));
        }

        // Gaussian filter.
        {
            GaussianFilterDescriptorSet descriptorSet(samplerLibrary);
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 4, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            gaussianFilterRGB3x3.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(GaussianFilterRGB3x3CSBlobDXIL, GaussianFilterRGB3x3CSBlobSPIRV, GaussianFilterRGB3x3CSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(gaussianFilterRGB3x3.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            gaussianFilterRGB3x3.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Histogram average.
        {
            HistogramAverageDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 5, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            histogramAverage.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(HistogramAverageCSBlobDXIL, HistogramAverageCSBlobSPIRV, HistogramAverageCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(histogramAverage.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            histogramAverage.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Histogram clear.
        {
            HistogramClearDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            histogramClear.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(HistogramClearCSBlobDXIL, HistogramClearCSBlobSPIRV, HistogramClearCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(histogramClear.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            histogramClear.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Histogram set.
        {
            HistogramSetDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            histogramSet.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(HistogramSetCSBlobDXIL, HistogramSetCSBlobSPIRV, HistogramSetCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(histogramSet.pipelineLayout.get(), computeShader.get(), 1, 1, 1);
            histogramSet.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Luminance histogram.
        {
            LuminanceHistogramDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 4, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            luminanceHistogram.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(LuminanceHistogramCSBlobDXIL, LuminanceHistogramCSBlobSPIRV, LuminanceHistogramCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(luminanceHistogram.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            luminanceHistogram.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // RSP Modify.
        {
            RSPModifyDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            rspModify.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(RSPModifyCSBlobDXIL, RSPModifyCSBlobSPIRV, RSPModifyCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(rspModify.pipelineLayout.get(), computeShader.get(), 64, 1, 1);
            rspModify.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // RSP Process.
        {
            RSPProcessDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 4, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            rspProcess.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(RSPProcessCSBlobDXIL, RSPProcessCSBlobSPIRV, RSPProcessCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(rspProcess.pipelineLayout.get(), computeShader.get(), 64, 1, 1);
            rspProcess.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // RSP Smooth Normal.
        {
            RSPSmoothNormalDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 2, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            rspSmoothNormal.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(RSPSmoothNormalCSBlobDXIL, RSPSmoothNormalCSBlobSPIRV, RSPSmoothNormalCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(rspSmoothNormal.pipelineLayout.get(), computeShader.get(), 64, 1, 1);
            rspSmoothNormal.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // RSP World.
        {
            RSPWorldDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 4, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            rspWorld.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(RSPWorldCSBlobDXIL, RSPWorldCSBlobSPIRV, RSPWorldCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(rspWorld.pipelineLayout.get(), computeShader.get(), 64, 1, 1);
            rspWorld.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Texture Copy.
        {
            TextureCopyDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::TextureCopyCB), RenderShaderStageFlag::PIXEL);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            textureCopy.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> pixelShader = device->createShader(CREATE_SHADER_INPUTS(TextureCopyPSBlobDXIL, TextureCopyPSBlobSPIRV, TextureCopyPSBlobMSL, "PSMain", shaderFormat));
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = textureCopy.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = pixelShader.get();
            textureCopy.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }

        // Texture Decode.
        {
            TextureDecodeDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 8, RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            textureDecode.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(TextureDecodeCSBlobDXIL, TextureDecodeCSBlobSPIRV, TextureDecodeCSBlobMSL, "CSMain", shaderFormat));
            RenderComputePipelineDesc pipelineDesc(textureDecode.pipelineLayout.get(), computeShader.get(), 8, 8, 1);
            textureDecode.pipeline = device->createComputePipeline(pipelineDesc);
        }

        // Video Interface.
        {
            std::unique_ptr<RenderShader> regularShader = device->createShader(CREATE_SHADER_INPUTS(VideoInterfacePSRegularBlobDXIL, VideoInterfacePSRegularBlobSPIRV, VideoInterfacePSRegularBlobMSL, "PSMain", shaderFormat));
            std::unique_ptr<RenderShader> pixelShader = device->createShader(CREATE_SHADER_INPUTS(VideoInterfacePSPixelBlobDXIL, VideoInterfacePSPixelBlobSPIRV, VideoInterfacePSPixelBlobMSL, "PSMain", shaderFormat));

            VideoInterfaceDescriptorSet nearestDescriptorSet(samplerLibrary.nearest.borderBorder.get());
            VideoInterfaceDescriptorSet linearDescriptorSet(samplerLibrary.linear.borderBorder.get());
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::VideoInterfaceCB), RenderShaderStageFlag::PIXEL);
            layoutBuilder.addDescriptorSet(nearestDescriptorSet);
            layoutBuilder.end();
            videoInterfaceNearest.pipelineLayout = layoutBuilder.create(device);

            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = regularShader.get();
            pipelineDesc.renderTargetFormat[0] = RenderFormat::B8G8R8A8_UNORM; // TODO: Use whatever format the swap chain was created with.
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.pipelineLayout = videoInterfaceNearest.pipelineLayout.get();
            videoInterfaceNearest.pipeline = device->createGraphicsPipeline(pipelineDesc);

            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::VideoInterfaceCB), RenderShaderStageFlag::PIXEL);
            layoutBuilder.addDescriptorSet(linearDescriptorSet);
            layoutBuilder.end();
            videoInterfaceLinear.pipelineLayout = layoutBuilder.create(device);
            videoInterfacePixel.pipelineLayout = layoutBuilder.create(device);

            pipelineDesc.pipelineLayout = videoInterfaceLinear.pipelineLayout.get();
            videoInterfaceLinear.pipeline = device->createGraphicsPipeline(pipelineDesc);

            pipelineDesc.pixelShader = pixelShader.get();
            pipelineDesc.pipelineLayout = videoInterfacePixel.pipelineLayout.get();
            videoInterfacePixel.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }
    }

    void ShaderLibrary::setupMultisamplingShaders(RenderInterface *rhi, RenderDevice *device, const RenderMultisampling &multisampling) {
        assert(rhi != nullptr);
        assert(device != nullptr);

        // Retrieve the shader format used by the RHI.
        const RenderInterfaceCapabilities interfaceCapabilities = rhi->getCapabilities();
        const RenderDeviceCapabilities deviceCapabilities = device->getCapabilities();
        const RenderShaderFormat shaderFormat = interfaceCapabilities.shaderFormat;
        RenderPipelineLayoutBuilder layoutBuilder;

        // Create shaders shared across all pipelines.
        std::unique_ptr<RenderShader> fullScreenVertexShader = device->createShader(CREATE_SHADER_INPUTS(FullScreenVSBlobDXIL, FullScreenVSBlobSPIRV, FullScreenVSBlobMSL, "VSMain", shaderFormat));

        // Select shader blobs based on sample count.
        const void *TextureResolvePSBlob = nullptr;
        const void *RtCopyDepthToColorPSBlob = nullptr;
        const void *RtCopyDepthToColorPSMSBlob = nullptr;
        const void *RtCopyColorToDepthPSBlob = nullptr;
        const void *RtCopyColorToDepthPSMSBlob = nullptr;
        uint32_t TextureResolvePSBlobSize = 0;
        uint32_t RtCopyDepthToColorPSBlobSize = 0;
        uint32_t RtCopyDepthToColorPSMSBlobSize = 0;
        uint32_t RtCopyColorToDepthPSBlobSize = 0;
        uint32_t RtCopyColorToDepthPSMSBlobSize = 0;
#   ifdef _WIN32
        if (shaderFormat == RenderShaderFormat::DXIL) {
            RtCopyDepthToColorPSBlob = RtCopyDepthToColorPSBlobDXIL;
            RtCopyDepthToColorPSBlobSize = std::size(RtCopyDepthToColorPSBlobDXIL);
            RtCopyColorToDepthPSBlob = RtCopyColorToDepthPSBlobDXIL;
            RtCopyColorToDepthPSBlobSize = std::size(RtCopyColorToDepthPSBlobDXIL);

            switch (multisampling.sampleCount) {
            case RenderSampleCount::COUNT_2:
                TextureResolvePSBlob = TextureResolveSamples2XPSBlobDXIL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples2XPSBlobDXIL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor2XPSBlobDXIL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor2XPSBlobDXIL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth2XPSBlobDXIL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth2XPSBlobDXIL);
                break;
            case RenderSampleCount::COUNT_4:
                TextureResolvePSBlob = TextureResolveSamples4XPSBlobDXIL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples4XPSBlobDXIL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor4XPSBlobDXIL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor4XPSBlobDXIL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth4XPSBlobDXIL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth4XPSBlobDXIL);
                break;
            case RenderSampleCount::COUNT_8:
                TextureResolvePSBlob = TextureResolveSamples8XPSBlobDXIL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples8XPSBlobDXIL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor8XPSBlobDXIL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor8XPSBlobDXIL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth8XPSBlobDXIL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth8XPSBlobDXIL);
                break;
            }
        }
        else
#   endif
#   if defined(__APPLE__)
        if (shaderFormat == RenderShaderFormat::METAL) {
            RtCopyDepthToColorPSBlob = RtCopyDepthToColorPSBlobMSL;
            RtCopyDepthToColorPSBlobSize = std::size(RtCopyDepthToColorPSBlobMSL);
            RtCopyColorToDepthPSBlob = RtCopyColorToDepthPSBlobMSL;
            RtCopyColorToDepthPSBlobSize = std::size(RtCopyColorToDepthPSBlobMSL);

            switch (multisampling.sampleCount) {
            case RenderSampleCount::COUNT_2:
                TextureResolvePSBlob = TextureResolveSamples2XPSBlobMSL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples2XPSBlobMSL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor2XPSBlobMSL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor2XPSBlobMSL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth2XPSBlobMSL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth2XPSBlobMSL);
                break;
            case RenderSampleCount::COUNT_4:
                TextureResolvePSBlob = TextureResolveSamples4XPSBlobMSL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples4XPSBlobMSL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor4XPSBlobMSL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor4XPSBlobMSL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth4XPSBlobMSL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth4XPSBlobMSL);
                break;
            case RenderSampleCount::COUNT_8:
                TextureResolvePSBlob = TextureResolveSamples8XPSBlobMSL;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples8XPSBlobMSL);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor8XPSBlobMSL;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor8XPSBlobMSL);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth8XPSBlobMSL;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth8XPSBlobMSL);
                break;
            }
        }
        else
#   endif
        if (shaderFormat == RenderShaderFormat::SPIRV) {
            RtCopyDepthToColorPSBlob = RtCopyDepthToColorPSBlobSPIRV;
            RtCopyDepthToColorPSBlobSize = std::size(RtCopyDepthToColorPSBlobSPIRV);
            RtCopyColorToDepthPSBlob = RtCopyColorToDepthPSBlobSPIRV;
            RtCopyColorToDepthPSBlobSize = std::size(RtCopyColorToDepthPSBlobSPIRV);

            switch (multisampling.sampleCount) {
            case RenderSampleCount::COUNT_2:
                TextureResolvePSBlob = TextureResolveSamples2XPSBlobSPIRV;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples2XPSBlobSPIRV);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor2XPSBlobSPIRV;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor2XPSBlobSPIRV);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth2XPSBlobSPIRV;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth2XPSBlobSPIRV);
                break;
            case RenderSampleCount::COUNT_4:
                TextureResolvePSBlob = TextureResolveSamples4XPSBlobSPIRV;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples4XPSBlobSPIRV);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor4XPSBlobSPIRV;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor4XPSBlobSPIRV);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth4XPSBlobSPIRV;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth4XPSBlobSPIRV);
                break;
            case RenderSampleCount::COUNT_8:
                TextureResolvePSBlob = TextureResolveSamples8XPSBlobSPIRV;
                TextureResolvePSBlobSize = std::size(TextureResolveSamples8XPSBlobSPIRV);
                RtCopyDepthToColorPSMSBlob = RtCopyDepthToColor8XPSBlobSPIRV;
                RtCopyDepthToColorPSMSBlobSize = std::size(RtCopyDepthToColor8XPSBlobSPIRV);
                RtCopyColorToDepthPSMSBlob = RtCopyColorToDepth8XPSBlobSPIRV;
                RtCopyColorToDepthPSMSBlobSize = std::size(RtCopyColorToDepth8XPSBlobSPIRV);
                break;
            }
        }
        else {
            assert(false && "Unknown shader format.");
        }

        // Framebuffer changes draw color and depth.
        {
            FramebufferDrawChangesDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(uint32_t) * 2, RenderShaderStageFlag::PIXEL);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            fbChangesDrawColor.pipelineLayout = layoutBuilder.create(device);
            fbChangesDrawDepth.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> colorShader = device->createShader(CREATE_SHADER_INPUTS(FbChangesDrawColorPSBlobDXIL, FbChangesDrawColorPSBlobSPIRV, FbChangesDrawColorPSBlobMSL, "PSMain", shaderFormat));
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = fbChangesDrawColor.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = colorShader.get();
            pipelineDesc.multisampling = multisampling;
            fbChangesDrawColor.pipeline = device->createGraphicsPipeline(pipelineDesc);

            std::unique_ptr<RenderShader> depthShader = device->createShader(CREATE_SHADER_INPUTS(FbChangesDrawDepthPSBlobDXIL, FbChangesDrawDepthPSBlobSPIRV, FbChangesDrawDepthPSBlobMSL, "PSMain", shaderFormat));
            pipelineDesc.pipelineLayout = fbChangesDrawDepth.pipelineLayout.get();
            pipelineDesc.pixelShader = depthShader.get();
            pipelineDesc.depthEnabled = true;
            pipelineDesc.depthFunction = RenderComparisonFunction::ALWAYS;
            pipelineDesc.depthWriteEnabled = true;
            pipelineDesc.depthTargetFormat = RenderFormat::D32_FLOAT;
            fbChangesDrawDepth.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }

        // Copy color to depth and depth to color.
        {
            RenderTargetCopyDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::RenderTargetCopyCB), RenderShaderStageFlag::PIXEL);
            layoutBuilder.end();
            rtCopyDepthToColor.pipelineLayout = layoutBuilder.create(device);
            rtCopyDepthToColorMS.pipelineLayout = layoutBuilder.create(device);
            rtCopyColorToDepth.pipelineLayout = layoutBuilder.create(device);
            rtCopyColorToDepthMS.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> depthToColorShader = device->createShader(RtCopyDepthToColorPSBlob, RtCopyDepthToColorPSBlobSize, "PSMain", shaderFormat);
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pipelineLayout = rtCopyDepthToColor.pipelineLayout.get();
            pipelineDesc.pixelShader = depthToColorShader.get();
            rtCopyDepthToColor.pipeline = device->createGraphicsPipeline(pipelineDesc);

            if (RtCopyDepthToColorPSMSBlob != nullptr) {
                std::unique_ptr<RenderShader> depthToColorMSShader = device->createShader(RtCopyDepthToColorPSMSBlob, RtCopyDepthToColorPSMSBlobSize, "PSMain", shaderFormat);
                pipelineDesc.pixelShader = depthToColorMSShader.get();
                pipelineDesc.multisampling = multisampling;
                rtCopyDepthToColorMS.pipeline = device->createGraphicsPipeline(pipelineDesc);
            }

            std::unique_ptr<RenderShader> colorToDepthShader = device->createShader(RtCopyColorToDepthPSBlob, RtCopyColorToDepthPSBlobSize, "PSMain", shaderFormat);
            pipelineDesc.depthEnabled = true;
            pipelineDesc.depthFunction = RenderComparisonFunction::ALWAYS;
            pipelineDesc.depthWriteEnabled = true;
            pipelineDesc.depthTargetFormat = RenderFormat::D32_FLOAT;
            pipelineDesc.pipelineLayout = rtCopyColorToDepth.pipelineLayout.get();
            pipelineDesc.pixelShader = colorToDepthShader.get();
            pipelineDesc.multisampling = RenderMultisampling();
            rtCopyColorToDepth.pipeline = device->createGraphicsPipeline(pipelineDesc);

            if (RtCopyColorToDepthPSMSBlob != nullptr) {
                std::unique_ptr<RenderShader> colorToDepthMSShader = device->createShader(RtCopyColorToDepthPSMSBlob, RtCopyColorToDepthPSMSBlobSize, "PSMain", shaderFormat);
                pipelineDesc.pixelShader = colorToDepthMSShader.get();
                pipelineDesc.multisampling = multisampling;
                rtCopyColorToDepthMS.pipeline = device->createGraphicsPipeline(pipelineDesc);
            }
        }

        // Post process.
        {
            PostProcessDescriptorSet descriptorSet(samplerLibrary);
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            postProcess.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> pixelShader = device->createShader(CREATE_SHADER_INPUTS(PostProcessPSBlobDXIL, PostProcessPSBlobSPIRV, PostProcessPSBlobMSL, "PSMain", shaderFormat));
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = postProcess.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = pixelShader.get();
            pipelineDesc.multisampling = multisampling;
            postProcess.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }
        
        // Raytracing debug.
        {
            FramebufferRendererDescriptorCommonSet descriptorCommonSet(samplerLibrary, deviceCapabilities.raytracing);
            FramebufferRendererDescriptorTextureSet descriptorTextureSet;
            FramebufferRendererDescriptorFramebufferSet descriptorFramebufferSet;
            layoutBuilder.begin();
            layoutBuilder.addDescriptorSet(descriptorCommonSet);
            layoutBuilder.addDescriptorSet(descriptorTextureSet);
            layoutBuilder.addDescriptorSet(descriptorTextureSet);
            layoutBuilder.addDescriptorSet(descriptorFramebufferSet);
            layoutBuilder.end();
            debug.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> pixelShader = device->createShader(CREATE_SHADER_INPUTS(DebugPSBlobDXIL, DebugPSBlobSPIRV, DebugPSBlobMSL, "PSMain", shaderFormat));
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = debug.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = pixelShader.get();
            pipelineDesc.multisampling = multisampling;
            debug.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }

        // RSP Vertex Test Z.
        {
            RSPVertexTestZDescriptorSet descriptorTestSet;
            FramebufferRendererDescriptorFramebufferSet descriptorFramebufferSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::RSPVertexTestZCB), RenderShaderStageFlag::COMPUTE);
            layoutBuilder.addDescriptorSet(descriptorTestSet);
            layoutBuilder.addDescriptorSet(descriptorFramebufferSet);
            layoutBuilder.end();
            rspVertexTestZ.pipelineLayout = layoutBuilder.create(device);
            rspVertexTestZMS.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> computeShader = device->createShader(CREATE_SHADER_INPUTS(RSPVertexTestZCSBlobDXIL, RSPVertexTestZCSBlobSPIRV, RSPVertexTestZCSBlobMSL, "CSMain", shaderFormat));
            rspVertexTestZ.pipeline = device->createComputePipeline(RenderComputePipelineDesc(rspVertexTestZ.pipelineLayout.get(), computeShader.get(), 1, 1, 1));

            std::unique_ptr<RenderShader> computeShaderMS = device->createShader(CREATE_SHADER_INPUTS(RSPVertexTestZCSMSBlobDXIL, RSPVertexTestZCSMSBlobSPIRV, RSPVertexTestZCSMSBlobMSL, "CSMain", shaderFormat));
            rspVertexTestZMS.pipeline = device->createComputePipeline(RenderComputePipelineDesc(rspVertexTestZMS.pipelineLayout.get(), computeShaderMS.get(), 1, 1, 1));
        }

        // Texture Resolve.
        if (TextureResolvePSBlob != nullptr) {
            TextureCopyDescriptorSet descriptorSet;
            layoutBuilder.begin();
            layoutBuilder.addPushConstant(0, 0, sizeof(interop::TextureCopyCB), RenderShaderStageFlag::PIXEL);
            layoutBuilder.addDescriptorSet(descriptorSet);
            layoutBuilder.end();
            textureResolve.pipelineLayout = layoutBuilder.create(device);

            std::unique_ptr<RenderShader> pixelShader = device->createShader(TextureResolvePSBlob, TextureResolvePSBlobSize, "PSMain", shaderFormat);
            RenderGraphicsPipelineDesc pipelineDesc;
            pipelineDesc.pipelineLayout = textureResolve.pipelineLayout.get();
            pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
            pipelineDesc.renderTargetFormat[0] = RenderTarget::colorBufferFormat(usesHDR);
            pipelineDesc.renderTargetCount = 1;
            pipelineDesc.vertexShader = fullScreenVertexShader.get();
            pipelineDesc.pixelShader = pixelShader.get();
            textureResolve.pipeline = device->createGraphicsPipeline(pipelineDesc);
        }
    }
};
