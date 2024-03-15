//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct ShaderRecord {
        std::unique_ptr<RenderPipeline> pipeline;
        std::unique_ptr<RenderPipelineLayout> pipelineLayout;
    };

    struct ShaderLibrary {
        std::unique_ptr<RenderSampler> nearestClampSampler;
        std::unique_ptr<RenderSampler> linearClampSampler;
        std::unique_ptr<RenderSampler> nearestBorderSampler;
        std::unique_ptr<RenderSampler> linearBorderSampler;
        std::unique_ptr<RenderSampler> nearestMirrorSampler;
        std::unique_ptr<RenderSampler> linearMirrorSampler;

        ShaderRecord bicubicScaling;
        ShaderRecord boxFilter;
        ShaderRecord compose;
        ShaderRecord debug;
        ShaderRecord fbChangesClear;
        ShaderRecord fbChangesDrawColor;
        ShaderRecord fbChangesDrawDepth;
        ShaderRecord fbReadAnyChanges;
        ShaderRecord fbReadAnyFull;
        ShaderRecord fbReinterpret;
        ShaderRecord fbWriteColor;
        ShaderRecord fbWriteDepth;
        ShaderRecord fbWriteDepthMS;
        ShaderRecord gaussianFilterRGB3x3;
        ShaderRecord histogramAverage;
        ShaderRecord histogramClear;
        ShaderRecord histogramSet;
        ShaderRecord idle;
        ShaderRecord im3dLine;
        ShaderRecord im3dPoint;
        ShaderRecord im3dTriangle;
        ShaderRecord luminanceHistogram;
        ShaderRecord postProcess;
        ShaderRecord rspModify;
        ShaderRecord rspProcess;
        ShaderRecord rspSmoothNormal;
        ShaderRecord rspVertexTestZ;
        ShaderRecord rspVertexTestZMS;
        ShaderRecord rspWorld;
        ShaderRecord rtCopyColorToDepth;
        ShaderRecord rtCopyDepthToColor;
        ShaderRecord rtCopyColorToDepthMS;
        ShaderRecord rtCopyDepthToColorMS;
        ShaderRecord textureDecode;
        ShaderRecord textureCopy;
        ShaderRecord videoInterfaceLinear;
        ShaderRecord videoInterfaceNearest;
        ShaderRecord videoInterfacePixel;

        ShaderLibrary();
        ~ShaderLibrary();
        void setupCommonShaders(RenderInterface *rhi, RenderDevice *device);
        void setupMultisamplingShaders(RenderInterface *rhi, RenderDevice *device, const RenderMultisampling &multisampling);
    };
};