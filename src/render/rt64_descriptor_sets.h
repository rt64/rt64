//
// RT64
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "common/rt64_common.h"
#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct BicubicScalingDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;
        uint32_t gOutput;
        uint32_t gSampler;

        BicubicScalingDescriptorSet(const RenderSampler *nearestClampSampler, RenderDevice *device = nullptr) {
            assert(nearestClampSampler != nullptr);

            builder.begin();
            gInput = builder.addTexture(1);
            gOutput = builder.addReadWriteTexture(2);
            gSampler = builder.addImmutableSampler(3, nearestClampSampler);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct BoxFilterDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;
        uint32_t gOutput;

        BoxFilterDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gInput = builder.addTexture(1);
            gOutput = builder.addReadWriteTexture(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferClearChangesDescriptorSet : RenderDescriptorSetBase {
        uint32_t gOutputCount;

        FramebufferClearChangesDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gOutputCount = builder.addReadWriteStructuredBuffer(0);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferDrawChangesDescriptorSet : RenderDescriptorSetBase {
        uint32_t gColor;
        uint32_t gDepth;
        uint32_t gBoolean;

        FramebufferDrawChangesDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gColor = builder.addTexture(1);
            gDepth = builder.addTexture(2);
            gBoolean = builder.addTexture(3);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferReadChangesDescriptorBufferSet : RenderDescriptorSetBase {
        uint32_t gNewInput;
        uint32_t gCurInput;
        uint32_t gOutputCount;

        FramebufferReadChangesDescriptorBufferSet(RenderDevice *device = nullptr) {
            builder.begin();
            gNewInput = builder.addFormattedBuffer(1);
            gCurInput = builder.addFormattedBuffer(2);
            gOutputCount = builder.addReadWriteStructuredBuffer(3);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferReadChangesDescriptorChangesSet : RenderDescriptorSetBase {
        uint32_t gOutputChangeColor;
        uint32_t gOutputChangeDepth;
        uint32_t gOutputChangeBoolean;

        FramebufferReadChangesDescriptorChangesSet(RenderDevice *device = nullptr) {
            builder.begin();
            gOutputChangeColor = builder.addReadWriteTexture(0);
            gOutputChangeDepth = builder.addReadWriteTexture(1);
            gOutputChangeBoolean = builder.addReadWriteTexture(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferWriteDescriptorBufferSet : RenderDescriptorSetBase {
        uint32_t gOutput;

        FramebufferWriteDescriptorBufferSet(RenderDevice *device = nullptr) {
            builder.begin();
            gOutput = builder.addReadWriteFormattedBuffer(1);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferWriteDescriptorTextureSet : RenderDescriptorSetBase {
        uint32_t gInput;

        FramebufferWriteDescriptorTextureSet(RenderDevice *device = nullptr) {
            builder.begin();
            gInput = builder.addTexture(0);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferRendererDescriptorCommonSet : RenderDescriptorSetBase {
        uint32_t FrParams;
        uint32_t instanceRDPParams;
        uint32_t RDPTiles;
        uint32_t GPUTiles;
        uint32_t instanceRenderIndices;
        uint32_t DynamicRenderParams;
        uint32_t RtParams;
        uint32_t SceneBVH;
        uint32_t posBuffer;
        uint32_t normBuffer;
        uint32_t velBuffer;
        uint32_t genTexCoordBuffer;
        uint32_t shadedColBuffer;
        uint32_t srcFogIndices;
        uint32_t srcLightIndices;
        uint32_t srcLightCounts;
        uint32_t indexBuffer;
        uint32_t RSPFogVector;
        uint32_t RSPLightVector;
        uint32_t instanceExtraParams;
        uint32_t SceneLights;
        uint32_t interleavedRasters;
        uint32_t gHitVelocityDistance;
        uint32_t gHitColor;
        uint32_t gHitNormalFog;
        uint32_t gHitInstanceId;
        uint32_t gViewDirection;
        uint32_t gShadingPosition;
        uint32_t gShadingNormal;
        uint32_t gShadingSpecular;
        uint32_t gDiffuse;
        uint32_t gInstanceId;
        uint32_t gDirectLightAccum;
        uint32_t gIndirectLightAccum;
        uint32_t gReflection;
        uint32_t gRefraction;
        uint32_t gTransparent;
        uint32_t gFlow;
        uint32_t gReactiveMask;
        uint32_t gLockMask;
        uint32_t gNormal;
        uint32_t gDepth;
        uint32_t gPrevNormal;
        uint32_t gPrevDepth;
        uint32_t gPrevDirectLightAccum;
        uint32_t gPrevIndirectLightAccum;
        uint32_t gFilteredDirectLight;
        uint32_t gFilteredIndirectLight;
        uint32_t gBackgroundClampSampler;
        uint32_t gBackgroundMirrorSampler;
        uint32_t gBlueNoise;

        FramebufferRendererDescriptorCommonSet(const RenderSampler *linearClampSampler, const RenderSampler *linearMirrorSampler, bool raytracing, RenderDevice *device = nullptr) {
            assert(linearClampSampler != nullptr);

            builder.begin();
            FrParams = builder.addConstantBuffer(1);
            instanceRDPParams = builder.addStructuredBuffer(2);
            RDPTiles = builder.addStructuredBuffer(3);
            GPUTiles = builder.addStructuredBuffer(4);
            instanceRenderIndices = builder.addStructuredBuffer(5);
            DynamicRenderParams = builder.addStructuredBuffer(6);
            RtParams = builder.addConstantBuffer(7);
            SceneBVH = raytracing ? builder.addAccelerationStructure(8) : 0;
            posBuffer = builder.addByteAddressBuffer(10);
            normBuffer = builder.addByteAddressBuffer(11);
            velBuffer = builder.addByteAddressBuffer(12);
            genTexCoordBuffer = builder.addByteAddressBuffer(13);
            shadedColBuffer = builder.addByteAddressBuffer(14);
            srcFogIndices = builder.addByteAddressBuffer(15);
            srcLightIndices = builder.addByteAddressBuffer(16);
            srcLightCounts = builder.addByteAddressBuffer(17);
            indexBuffer = builder.addByteAddressBuffer(18);
            RSPFogVector = builder.addStructuredBuffer(19);
            RSPLightVector = builder.addStructuredBuffer(20);
            instanceExtraParams = builder.addStructuredBuffer(21);
            SceneLights = builder.addStructuredBuffer(22);
            interleavedRasters = builder.addStructuredBuffer(23);
            gHitVelocityDistance = builder.addReadWriteFormattedBuffer(24);
            gHitColor = builder.addReadWriteFormattedBuffer(25);
            gHitNormalFog = builder.addReadWriteFormattedBuffer(26);
            gHitInstanceId = builder.addReadWriteFormattedBuffer(27);
            gViewDirection = builder.addReadWriteTexture(28);
            gShadingPosition = builder.addReadWriteTexture(29);
            gShadingNormal = builder.addReadWriteTexture(30);
            gShadingSpecular = builder.addReadWriteTexture(31);
            gDiffuse = builder.addReadWriteTexture(32);
            gInstanceId = builder.addReadWriteTexture(33);
            gDirectLightAccum = builder.addReadWriteTexture(34);
            gIndirectLightAccum = builder.addReadWriteTexture(35);
            gReflection = builder.addReadWriteTexture(36);
            gRefraction = builder.addReadWriteTexture(37);
            gTransparent = builder.addReadWriteTexture(38);
            gFlow = builder.addReadWriteTexture(39);
            gReactiveMask = builder.addReadWriteTexture(40);
            gLockMask = builder.addReadWriteTexture(41);
            gNormal = builder.addReadWriteTexture(42);
            gDepth = builder.addReadWriteTexture(43);
            gPrevNormal = builder.addReadWriteTexture(44);
            gPrevDepth = builder.addReadWriteTexture(45);
            gPrevDirectLightAccum = builder.addReadWriteTexture(46);
            gPrevIndirectLightAccum = builder.addReadWriteTexture(47);
            gFilteredDirectLight = builder.addReadWriteTexture(48);
            gFilteredIndirectLight = builder.addReadWriteTexture(49);
            gBackgroundClampSampler = builder.addImmutableSampler(50, linearClampSampler);
            gBackgroundMirrorSampler = builder.addImmutableSampler(51, linearMirrorSampler);
            gBlueNoise = builder.addTexture(52);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferRendererDescriptorTextureSet : RenderDescriptorSetBase {
        static const int UpperRange = 0x1FFF;

        uint32_t textureCacheSize = 0;
        uint32_t gTextures;
        uint32_t gTMEM;

        FramebufferRendererDescriptorTextureSet(RenderDevice *device = nullptr, uint32_t textureCacheSize = 0) {
            this->textureCacheSize = textureCacheSize;

            builder.begin();
            gTextures = builder.addTexture(0, UpperRange);
            gTMEM = gTextures;
            builder.end(true, textureCacheSize);

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct FramebufferRendererDescriptorFramebufferSet : RenderDescriptorSetBase {
        uint32_t FbParams;
        uint32_t gBackgroundColor;
        uint32_t gBackgroundDepth;

        FramebufferRendererDescriptorFramebufferSet(RenderDevice *device = nullptr) {
            builder.begin();
            FbParams = builder.addConstantBuffer(0);
            gBackgroundColor = builder.addTexture(1);
            gBackgroundDepth = builder.addTexture(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct GaussianFilterDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;
        uint32_t gOutput;
        uint32_t gSampler;

        GaussianFilterDescriptorSet(const RenderSampler *nearestClampSampler, RenderDevice *device = nullptr) {
            assert(nearestClampSampler != nullptr);

            builder.begin();
            gInput = builder.addTexture(1);
            gOutput = builder.addReadWriteTexture(2);
            gSampler = builder.addImmutableSampler(3, nearestClampSampler);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct HistogramAverageDescriptorSet : RenderDescriptorSetBase {
        uint32_t LuminanceHistogram;
        uint32_t LuminanceOutput;

        HistogramAverageDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            LuminanceHistogram = builder.addByteAddressBuffer(1);
            LuminanceOutput = builder.addReadWriteTexture(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct HistogramClearDescriptorSet : RenderDescriptorSetBase {
        uint32_t LuminanceHistogram;

        HistogramClearDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            LuminanceHistogram = builder.addReadWriteByteAddressBuffer(0);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct HistogramSetDescriptorSet : RenderDescriptorSetBase {
        uint32_t LuminanceOutput;

        HistogramSetDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            LuminanceOutput = builder.addReadWriteTexture(1);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct LuminanceHistogramDescriptorSet : RenderDescriptorSetBase {
        uint32_t HDRTexture;
        uint32_t LuminanceHistogram;

        LuminanceHistogramDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            HDRTexture = builder.addTexture(1);
            LuminanceHistogram = builder.addReadWriteByteAddressBuffer(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct PostProcessDescriptorSet : RenderDescriptorSetBase {
        uint32_t RtParams;
        uint32_t gInput;
        uint32_t gFlow;
        uint32_t gLumaAvg;
        uint32_t gSampler;

        PostProcessDescriptorSet(const RenderSampler *linearClampSampler, RenderDevice *device = nullptr) {
            assert(linearClampSampler != nullptr);

            builder.begin();
            RtParams = builder.addConstantBuffer(0);
            gInput = builder.addTexture(1);
            gFlow = builder.addTexture(2);
            gLumaAvg = builder.addTexture(3);
            gSampler = builder.addImmutableSampler(4, linearClampSampler);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RaytracingComposeDescriptorSet : RenderDescriptorSetBase {
        uint32_t gSampler;
        uint32_t gFlow;
        uint32_t gDiffuse;
        uint32_t gDirectLight;
        uint32_t gIndirectLight;
        uint32_t gReflection;
        uint32_t gRefraction;
        uint32_t gTransparent;

        RaytracingComposeDescriptorSet(const RenderSampler *linearClampSampler, RenderDevice *device = nullptr) {
            assert(linearClampSampler != nullptr);

            builder.begin();
            gSampler = builder.addImmutableSampler(0, linearClampSampler);
            gFlow = builder.addTexture(1);
            gDiffuse = builder.addTexture(2);
            gDirectLight = builder.addTexture(3);
            gIndirectLight = builder.addTexture(4);
            gReflection = builder.addTexture(5);
            gRefraction = builder.addTexture(6);
            gTransparent = builder.addTexture(7);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct ReinterpretDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInputColor;
        uint32_t gInputTLUT;
        uint32_t gOutput;

        ReinterpretDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gInputColor = builder.addTexture(1);
            gInputTLUT = builder.addTexture(2);
            gOutput = builder.addReadWriteTexture(3);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RenderTargetCopyDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;

        RenderTargetCopyDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gInput = builder.addTexture(0);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RSPModifyDescriptorSet : RenderDescriptorSetBase {
        uint32_t srcModifyPos;
        uint32_t screenPos;

        RSPModifyDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            srcModifyPos = builder.addFormattedBuffer(1);
            screenPos = builder.addReadWriteStructuredBuffer(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RSPProcessDescriptorSet : RenderDescriptorSetBase {
        uint32_t srcPos;
        uint32_t srcVel;
        uint32_t srcTc;
        uint32_t srcCol;
        uint32_t srcNorm;
        uint32_t srcViewProjIndices;
        uint32_t srcWorldIndices;
        uint32_t srcFogIndices;
        uint32_t srcLightIndices;
        uint32_t srcLightCounts;
        uint32_t srcLookAtIndices;
        uint32_t rspViewportVector;
        uint32_t rspFogVector;
        uint32_t rspLightVector;
        uint32_t rspLookAtVector;
        uint32_t viewProjTransforms;
        uint32_t worldTransforms;
        uint32_t dstPos;
        uint32_t dstTc;
        uint32_t dstCol;

        RSPProcessDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            srcPos = builder.addFormattedBuffer(1);
            srcVel = builder.addFormattedBuffer(2);
            srcTc = builder.addFormattedBuffer(3);
            srcCol = builder.addFormattedBuffer(4);
            srcNorm = builder.addFormattedBuffer(5);
            srcViewProjIndices = builder.addFormattedBuffer(6);
            srcWorldIndices = builder.addFormattedBuffer(7);
            srcFogIndices = builder.addFormattedBuffer(8);
            srcLightIndices = builder.addFormattedBuffer(9);
            srcLightCounts = builder.addFormattedBuffer(10);
            srcLookAtIndices = builder.addFormattedBuffer(11);
            rspViewportVector = builder.addStructuredBuffer(12);
            rspFogVector = builder.addStructuredBuffer(13);
            rspLightVector = builder.addStructuredBuffer(14);
            rspLookAtVector = builder.addStructuredBuffer(15);
            viewProjTransforms = builder.addStructuredBuffer(16);
            worldTransforms = builder.addStructuredBuffer(17);
            dstPos = builder.addReadWriteStructuredBuffer(18);
            dstTc = builder.addReadWriteStructuredBuffer(19);
            dstCol = builder.addReadWriteStructuredBuffer(20);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RSPSmoothNormalDescriptorSet : RenderDescriptorSetBase {
        uint32_t srcWorldPos;
        uint32_t srcCol;
        uint32_t srcFaceIndices;
        uint32_t dstWorldNorm;

        RSPSmoothNormalDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            srcWorldPos = builder.addStructuredBuffer(1);
            srcCol = builder.addStructuredBuffer(2);
            srcFaceIndices = builder.addStructuredBuffer(3);
            dstWorldNorm = builder.addReadWriteStructuredBuffer(4);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RSPVertexTestZDescriptorSet : RenderDescriptorSetBase {
        uint32_t screenPos;
        uint32_t srcFaceIndices;
        uint32_t dstFaceIndices;

        RSPVertexTestZDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            screenPos = builder.addStructuredBuffer(1);
            srcFaceIndices = builder.addStructuredBuffer(2);
            dstFaceIndices = builder.addReadWriteStructuredBuffer(3);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct RSPWorldDescriptorSet : RenderDescriptorSetBase {
        uint32_t srcPos;
        uint32_t srcVel;
        uint32_t srcNorm;
        uint32_t srcIndices;
        uint32_t worldMats;
        uint32_t invTWorldMats;
        uint32_t prevWorldMats;
        uint32_t dstPos;
        uint32_t dstNorm;
        uint32_t dstVel;

        RSPWorldDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            srcPos = builder.addFormattedBuffer(1);
            srcVel = builder.addFormattedBuffer(2);
            srcNorm = builder.addFormattedBuffer(3);
            srcIndices = builder.addFormattedBuffer(4);
            worldMats = builder.addStructuredBuffer(5);
            invTWorldMats = builder.addStructuredBuffer(6);
            prevWorldMats = builder.addStructuredBuffer(7);
            dstPos = builder.addReadWriteStructuredBuffer(8);
            dstNorm = builder.addReadWriteStructuredBuffer(9);
            dstVel = builder.addReadWriteStructuredBuffer(10);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct TextureCopyDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;

        TextureCopyDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            gInput = builder.addTexture(1);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct TextureDecodeDescriptorSet : RenderDescriptorSetBase {
        uint32_t TMEM;
        uint32_t RGBA32;

        TextureDecodeDescriptorSet(RenderDevice *device = nullptr) {
            builder.begin();
            TMEM = builder.addTexture(1);
            RGBA32 = builder.addReadWriteTexture(2);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };

    struct VideoInterfaceDescriptorSet : RenderDescriptorSetBase {
        uint32_t gInput;
        uint32_t gSampler;

        VideoInterfaceDescriptorSet(const RenderSampler *sampler, RenderDevice *device = nullptr) {
            builder.begin();
            gInput = builder.addTexture(1);
            gSampler = builder.addImmutableSampler(2, &sampler);
            builder.end();

            if (device != nullptr) {
                create(device);
            }
        }
    };
};