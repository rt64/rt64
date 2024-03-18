//
// RT64
//

#pragma once

#include <array>
#include <bitset>
#include <stdint.h>

#include "common/rt64_common.h"
#include "gbi/rt64_display_list.h"
#include "shared/rt64_other_mode.h"
#include "shared/rt64_rsp_fog.h"
#include "shared/rt64_rsp_light.h"
#include "shared/rt64_rsp_lookat.h"
#include "shared/rt64_rsp_viewport.h"

#include "rt64_projection.h"
#include "rt64_transform_group.h"

#define RSP_DMA_MASK 0xFFFFF8 // Only bits 3-23 (0-indexed) are used by the DMA hardware.

#define RSP_MAX_LIGHTS              7
#define RSP_MATRIX_STACK_SIZE       32
#define RSP_MAX_VERTICES            256
#define RSP_MAX_SEGMENTS            16
#define RSP_MATRIX_ID_STACK_SIZE    256

namespace RT64 {
    struct State;
    struct GBI;

    struct RSP {
        struct Vertex {
            int16_t y;
            int16_t x;

            uint16_t flag;
            int16_t z;

            int16_t t;
            int16_t s;

            union {
                struct {
                    uint8_t a;
                    uint8_t b;
                    uint8_t g;
                    uint8_t r;
                } color;

                struct {
                    int8_t a;
                    int8_t z;
                    int8_t y;
                    int8_t x;
                } normal;
            };
        };

        struct VertexPD {
            int16_t y, x;
            uint16_t ci;
            int16_t z;
            int16_t t, s;
        };

        struct RawLight {
            uint32_t words[4];
        };

        struct PosLight {
            uint8_t kc;
            uint8_t colb;
            uint8_t colg;
            uint8_t colr;
            uint8_t kl;
            uint8_t colcb;
            uint8_t colcg;
            uint8_t colcr;
            int16_t posy;
            int16_t posx;
            uint8_t reserved1;
            uint8_t kq;
            int16_t posz;
        };

        struct DirLight {
            uint8_t pad1;
            uint8_t colb;
            uint8_t colg;
            uint8_t colr;
            uint8_t pad2;
            uint8_t colcb;
            uint8_t colcg;
            uint8_t colcr;
            uint8_t pad3;
            int8_t dirz;
            int8_t diry;
            int8_t dirx;
        };

        union Light {
            RawLight raw;
            PosLight pos;
            DirLight dir;
        };

        struct Texcoord {
            float s;
            float t;
        };

        State *state;
        std::array<hlslpp::float4x4, RSP_MATRIX_STACK_SIZE> modelMatrixStack;
        std::array<uint32_t, RSP_MATRIX_STACK_SIZE> modelMatrixSegmentedAddressStack;
        std::array<uint32_t, RSP_MATRIX_STACK_SIZE> modelMatrixPhysicalAddressStack;
        int modelMatrixStackSize;
        hlslpp::float4x4 viewMatrix;
        hlslpp::float4x4 projMatrix;
        hlslpp::float4x4 viewProjMatrix;
        hlslpp::float4x4 invViewProjMatrix;
        hlslpp::float4x4 modelViewProjMatrix;
        uint16_t curViewProjIndex;
        uint16_t curTransformIndex;
        uint16_t curFogIndex;
        uint16_t curLightIndex;
        uint16_t curLookAtIndex;
        uint8_t curLightCount;
        bool projectionMatrixChanged;
        bool projectionMatrixInversed;
        bool viewportChanged;
        bool modelViewProjChanged;
        bool modelViewProjInserted;
        int projectionIndex;
        interop::RSPViewport viewport;
        std::array<Vertex, RSP_MAX_VERTICES> vertices;
        std::array<uint32_t, RSP_MAX_VERTICES> indices;
        std::bitset<RSP_MAX_VERTICES> used;
        std::array<Light, RSP_MAX_LIGHTS + 1> lights;
        int lightCount;
        uint32_t vertexFogIndex;
        uint32_t vertexLightIndex;
        uint32_t vertexLightCount;
        uint32_t vertexLookAtIndex;
        uint32_t vertexColorPDAddress;
        bool fogChanged;
        bool lightsChanged;
        bool lookAtChanged;
        interop::RSPLookAt lookAt;
        interop::OtherMode otherMode;
        uint32_t geometryMode;
        uint32_t objRenderMode;
        interop::RSPFog fog;
        bool NoN;
        uint32_t cullBothMask;
        uint32_t cullFrontMask;
        uint32_t projMask;
        uint32_t loadMask;
        uint32_t pushMask;
        uint32_t shadingSmoothMask;
        std::array<uint32_t, RSP_MAX_SEGMENTS> segments;

        struct {
            // Storage for struct data loaded by S2D commands.
            std::array<uint8_t, 256> struct_buffer;
            // Status tracking for sid/mask/flag.
            std::array<uint32_t, 4> statuses;
            // Seems to hold an id corresponding to the last command run.
            int8_t data_02AE;
        } S2D;

        struct {
            uint8_t tile = 0;
            uint8_t levels = 0;
            uint8_t on = 0;
            uint16_t sc = 0;
            uint16_t tc = 0;
        } texture;

        struct {
            // For stateful methods.
            struct {
                uint16_t viewportOrigin;
                int16_t viewportOffsetX;
                int16_t viewportOffsetY;
            } global;

            DrawExtendedType drawExtendedType;
            DrawExtendedData drawExtendedData;
            uint16_t viewportOrigin;
            std::array<TransformGroup, RSP_MATRIX_ID_STACK_SIZE> modelMatrixIdStack;
            int modelMatrixIdStackSize;
            bool modelMatrixIdStackChanged;
            int curModelMatrixIdStackIndex;
            TransformGroup viewProjMatrixId;
            bool viewProjMatrixIdChanged;
            int curViewProjMatrixIdIndex;
            bool forceBranch;
        } extended;

        RSP(State *state);
        void reset();
        Projection::Type getCurrentProjectionType() const;
        void addCurrentProjection(Projection::Type type);
        uint32_t fromSegmented(uint32_t segAddress);
        void setSegment(uint32_t seg, uint32_t address);
        void matrix(uint32_t address, uint8_t params);
        void popMatrix(uint32_t count);
        void insertMatrix(uint32_t address, uint32_t value);
        void forceMatrix(uint32_t address);
        void computeModelViewProj();
        void specialComputeModelViewProj();
        void setModelViewProjChanged(bool changed);
        void setVertex(uint32_t address, uint8_t vtxCount, uint8_t dstIndex);
        void setVertexPD(uint32_t address, uint8_t vtxCount, uint8_t dstIndex);
        void setVertexColorPD(uint32_t address);
        void setVertexCommon(uint8_t dstIndex, uint8_t dstMax);
        void modifyVertex(uint16_t dstIndex, uint16_t dstAttribute, uint32_t value);
        void setGeometryMode(uint32_t mask);
        void clearGeometryMode(uint32_t mask);
        void modifyGeometryMode(uint32_t offMask, uint32_t onMask);
        void setObjRenderMode(uint32_t value);
        void setViewport(uint32_t address);
        void setViewport(uint32_t address, uint16_t ori, int16_t offx, int16_t offy);
        void setLight(uint8_t index, uint32_t address);
        void setLightColor(uint8_t index, uint32_t value);
        void setLightCount(uint8_t count);
        void setClipRatio(uint32_t clipRatio);
        void setPerspNorm(uint32_t perspNorm);
        void setLookAt(uint8_t index, uint32_t address);
        void setLookAtVectors(interop::float3 x, interop::float3 y);
        void setFog(int16_t mul, int16_t offset);
        void branchZ(uint32_t branchDl, uint16_t vtxIndex, uint32_t zValue, DisplayList **dl);
        void branchW(uint32_t branchDl, uint16_t vtxIndex, uint32_t wValue, DisplayList **dl);
        void setTexture(uint8_t tile, uint8_t level, uint8_t on, uint16_t sc, uint16_t tc);
        void setOtherMode(uint32_t high, uint32_t low);
        void setOtherModeL(uint32_t size, uint32_t off, uint32_t data);
        void setOtherModeH(uint32_t size, uint32_t off, uint32_t data);
        void setColorImage(uint8_t fmt, uint8_t siz, uint16_t width, uint32_t segAddress);
        void setDepthImage(uint32_t segAddress);
        void setTextureImage(uint8_t fmt, uint8_t siz, uint16_t width, uint32_t segAddress);
        void drawIndexedTri(uint32_t a, uint32_t b, uint32_t c, bool rawGlobalIndices);
        void drawIndexedTri(uint32_t a, uint32_t b, uint32_t c);
        void setViewportAlign(uint16_t ori, int16_t offx, int16_t offy);
        void vertexTestZ(uint8_t vtxIndex);
        void endVertexTestZ();
        void matrixId(uint32_t id, bool push, bool proj, bool decompose, uint8_t pos, uint8_t rot, uint8_t scale, uint8_t skew, uint8_t persp, uint8_t vert, uint8_t tile, uint8_t order);
        void popMatrixId(uint8_t count);
        void forceBranch(bool force);
        void clearExtended();
        void setGBI(GBI *gbi);
    };
};