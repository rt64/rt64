//
// RT64
//

#pragma once

#include "rt64_shader_common.h"

#include "rhi/rt64_render_interface.h"
#include "shared/rt64_blender.h"
#include "shared/rt64_color_combiner.h"
#include "shared/rt64_other_mode.h"

#include "rt64_shader_library.h"
#include "rt64_shader_compiler.h"

namespace RT64 {
    struct PipelineCreation {
        RenderDevice *device;
        const RenderPipelineLayout *pipelineLayout;
        const RenderShader *vertexShader;
        const RenderShader *pixelShader;
        bool alphaBlend;
        bool culling;
        bool NoN;
        bool zCmp;
        bool zUpd;
        bool zDecal;
        bool cvgAdd;
        std::vector<RenderSpecConstant> specConstants;
        RenderMultisampling multisampling;
    };

    struct RasterShaderText {
        std::string vertexShader;
        std::string pixelShader;
    };

    struct RasterShader {
        ShaderDescription desc;
        RenderDevice *device;
        std::unique_ptr<RenderPipeline> pipeline;

        RasterShader(RenderDevice *device, const ShaderDescription &desc, const RenderPipelineLayout *pipelineLayout, RenderShaderFormat shaderForma, const RenderMultisampling &multisamplingt, const ShaderCompiler *shaderCompiler);
        ~RasterShader();
        static RasterShaderText generateShaderText(const ShaderDescription &desc, bool multisampling);
        static std::unique_ptr<RenderPipeline> createPipeline(const PipelineCreation &c);
        static RenderMultisampling generateMultisamplingPattern(uint32_t sampleCount, bool sampleLocationsSupported);
    };

    struct RasterShaderUber {
        std::unique_ptr<RenderPipeline> pipelines[64];
        std::unique_ptr<RenderPipelineLayout> pipelineLayout;

        RasterShaderUber(RenderDevice *device, RenderShaderFormat shaderFormat, const RenderMultisampling &multisampling, const ShaderLibrary *shaderLibrary);
        ~RasterShaderUber();
        uint32_t pipelineStateIndex(bool alphaBlend, bool culling, bool zCmp, bool zUpd, bool zDecal, bool cvgAdd) const;
        const RenderPipeline *getPipeline(bool alphaBlend, bool culling, bool zCmp, bool zUpd, bool zDecal, bool cvgAdd) const;
    };
};