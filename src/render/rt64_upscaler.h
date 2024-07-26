//
// RT64
//

#pragma once

#include "common/rt64_common.h"

#include "rt64_render_worker.h"

namespace RT64 {
    struct RenderWorker;

    struct Upscaler {
        enum class QualityMode : int {
            UltraPerformance = 0,
            Performance,
            Balanced,
            Quality,
            UltraQuality,
            Native,
            Auto,
            MAX
        };

        struct UpscaleParameters {
            RectI inRect;
            RenderTexture *inDiffuseAlbedo;
            RenderTexture *inSpecularAlbedo;
            RenderTexture *inNormalRoughness;
            RenderTexture *inColor;
            RenderTexture *inFlow;
            RenderTexture *inReactiveMask;
            RenderTexture *inLockMask;
            RenderTexture *inDepth;
            RenderTexture *outColor;
            float jitterX;
            float jitterY;
            float deltaTime;
            float nearPlane;
            float farPlane;
            float fovY;
            bool resetAccumulation;
        };

        virtual void set(RenderWorker *worker, QualityMode inQuality, int renderWidth, int renderHeight, int displayWidth, int displayHeight) = 0;
        virtual bool getQualityInformation(QualityMode quality, int displayWidth, int displayHeight, int &renderWidth, int &renderHeight) = 0;
        virtual int getJitterPhaseCount(int renderWidth, int displayWidth) = 0;
        virtual void upscale(RenderWorker *worker, const UpscaleParameters &p) = 0;
        virtual bool isInitialized() const = 0;
        virtual bool requiresNonShaderResourceInputs() const = 0;

        static QualityMode getQualityAuto(int displayWidth, int displayHeight);
    };
};