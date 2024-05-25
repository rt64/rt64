//
// RT64
//

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "common/rt64_load_types.h"
#include "shared/rt64_color_combiner.h"
#include "shared/rt64_extra_params.h"
#include "shared/rt64_rdp_params.h"
#include "shared/rt64_rdp_tile.h"

#include "rt64_framebuffer.h"

namespace RT64 {
    // The values of the attributes must be preserved as they are for draw call filters that were already saved using these constants.
    // Any missing gaps are purely intentional for the sake of keeping backwards compatibility.

    enum class DrawAttribute : uint32_t {
        Zero = 0,
        UID = 1,
        Tris = 2,
        Scissor = 5,
        Combine = 7,
        Texture = 8,
        OtherMode = 9,
        GeometryMode = 11,
        PrimColor = 12,
        EnvColor = 13,
        FogColor = 14,
        FillColor = 15,
        BlendColor = 16,
        Lights = 18,
        FramebufferPair = 21,
        PrimDepth = 22,
        Convert = 23,
        Key = 24,
        ObjRenderMode = 25,
        ExtendedType = 26,
        ExtendedFlags = 27,
        Count = 28
    };

    enum class DrawExtendedType : uint8_t {
        None,
        VertexTestZ,
        EndVertexTestZ
    };

    struct DrawVertexTestZ {
        uint32_t vertexIndex;
    };

    struct DrawExtendedData {
        union {
            DrawVertexTestZ vertexTestZ;
        };
    };

    struct DrawExtendedFlags {
        uint32_t forceUpscale2D : 1;
        uint32_t forceTrueBilerp : 2;
        uint32_t forceScaleLOD : 1;
    };

    struct DrawCall {
        // Global frame call index.
        uint32_t uid;
        uint32_t callIndex;
        uint16_t minWorldMatrix;
        uint16_t maxWorldMatrix;
        uint32_t triangleCount;
        FixedRect rect;
        int16_t rectDsdx;
        int16_t rectDtdy;
        uint16_t rectLeftOrigin;
        uint16_t rectRightOrigin;
        FixedRect scissorRect;
        uint8_t scissorMode;
        uint16_t scissorLeftOrigin;
        uint16_t scissorRightOrigin;
        interop::ColorCombiner colorCombiner;
        interop::OtherMode otherMode;
        interop::RDPParams rdpParams;
        uint32_t fillColor;
        uint32_t tileIndex;
        uint32_t tileCount;
        uint32_t loadIndex;
        uint32_t loadCount;
        uint8_t textureOn;
        uint8_t textureTile;
        uint8_t textureLevels;

        // RSP specific parameters.
        uint32_t geometryMode;
        uint32_t objRenderMode;

        // GBI specific parameters.
        uint32_t cullBothMask;
        uint32_t shadingSmoothMask;
        bool NoN;

        // GBI extended parameters.
        DrawExtendedType extendedType;
        DrawExtendedData extendedData;
        DrawExtendedFlags extendedFlags;

        // Raytracing parameters.
        // TODO: This is likely better pushed elsewhere and using an index
        // instead of adding this structure per call.
        interop::ExtraParams extraParams;

        // Debugging parameters.
        uint32_t drawStatusChanges;

        static std::string attributeName(DrawAttribute attribute);
        bool identityRectScale() const;
    };
    
    struct DrawStatus {
        uint32_t changed;
        
        DrawStatus();
        DrawStatus(uint32_t v);
        void reset();
        void clearChanges();
        void clearChange(DrawAttribute attribute);
        void setChanged(DrawAttribute attribute);
        bool isChanged(DrawAttribute attribute) const;
        bool isChanged() const;
    };

    struct DrawCallTile {
        LoadTile loadTile;
        uint64_t tmemHashOrID;
        uint16_t sampleWidth;
        uint16_t sampleHeight;
        uint16_t lineWidth;
        uint32_t tlut;
        interop::int2 minTexcoord;
        interop::int2 maxTexcoord;
        bool syncRequired;
        bool tileCopyUsed;
        uint16_t tileCopyWidth;
        uint16_t tileCopyHeight;
        bool reinterpretTile;
        uint8_t reinterpretSiz;
        uint8_t reinterpretFmt;
        bool rawTMEM;
        bool valid;

        bool validTexcoords() const {
            return (minTexcoord.x <= maxTexcoord.x) && (minTexcoord.y <= maxTexcoord.y);
        }
    };
};