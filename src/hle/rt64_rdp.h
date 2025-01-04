//
// RT64
//

#pragma once

#include "common/rt64_common.h"

#include <array>
#include <stdint.h>

#include "../include/rt64_extended_gbi.h"

#include "gbi/rt64_display_list.h"
#include "shared/rt64_color_combiner.h"
#include "shared/rt64_other_mode.h"

#include "rt64_draw_call.h"
#include "rt64_framebuffer_manager.h"

#define RDP_TMEM_WORDS              512
#define RDP_TMEM_BYTES              4096
#define RDP_TMEM_MASK8              4095
#define RDP_TMEM_MASK16             2047
#define RDP_TMEM_MASK32             1023
#define RDP_TMEM_MASK64             511
#define RDP_TMEM_MASK128            255
#define RDP_TILES                   8
#define RDP_ADDRESS_MASK            0xFFFFFF
#define RDP_EXTENDED_STACK_SIZE     16

#ifdef __GNUC__
#   define __forceinline __attribute__((always_inline)) inline
#endif

namespace RT64 {
    struct State;
    struct GBI;

    enum class RDPTriangle {
        Base = G_RDPTRI_BASE,
        Depth = 1 << 0,
        Textured = 1 << 1,
        Shaded = 1 << 2,
        MaxValue = Base | Depth | Textured | Shaded
    };

    struct ExtendedAlignment {
        uint16_t leftOrigin = G_EX_ORIGIN_NONE;
        uint16_t rightOrigin = G_EX_ORIGIN_NONE;
        int32_t leftOffset = 0;
        int32_t topOffset = 0;
        int32_t rightOffset = 0;
        int32_t bottomOffset = 0;
        int32_t leftBound = INT32_MIN;
        int32_t topBound = INT32_MIN;
        int32_t rightBound = INT32_MAX;
        int32_t bottomBound = INT32_MAX;
    };

    // In multiples of 8 bytes
    constexpr size_t triangleBaseWords = 4;
    constexpr size_t triangleShadeWords = 8;
    constexpr size_t triangleTexWords = 8;
    constexpr size_t triangleDepthWords = 2;

    struct RDP {
        enum class CrashReason {
            None
        };

        uint64_t TMEM[RDP_TMEM_WORDS] = {};
        LoadTexture texture = {};
        LoadTile tiles[RDP_TILES] = {};
        uint64_t tileReplacementHashes[RDP_TILES] = {};

        struct {
            uint32_t address = 0;
            uint8_t fmt = 0;
            uint8_t siz = 0;
            uint16_t width = 0;
            bool changed = false;
        } colorImage;

        struct {
            uint32_t address = 0;
            bool changed = false;
        } depthImage;

        struct {
            // For stateful methods.
            struct {
                ExtendedAlignment rect;
                ExtendedAlignment scissor;
            } global;

            uint16_t scissorLeftOrigin;
            uint16_t scissorRightOrigin;
            DrawExtendedFlags drawExtendedFlags;
        } extended;

        struct {
            LoadOperation lastLoadOpByTMEM[RDP_TMEM_WORDS] = {};
        } rice;

        GBI *gbi;
        std::array<uint8_t, 256> commandWordLengths;
        State *state;
        interop::OtherMode otherMode;
        interop::ColorCombiner colorCombinerStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float4 envColorStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float4 primColorStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float2 primLODStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float2 primDepthStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float4 blendColorStack[RDP_EXTENDED_STACK_SIZE];
        hlslpp::float4 fogColorStack[RDP_EXTENDED_STACK_SIZE];
        uint32_t fillColorStack[RDP_EXTENDED_STACK_SIZE];
        FixedRect scissorRectStack[RDP_EXTENDED_STACK_SIZE];
        uint8_t scissorModeStack[RDP_EXTENDED_STACK_SIZE];
        int colorCombinerStackSize;
        int envColorStackSize;
        int primColorStackSize;
        int primDepthStackSize;
        int blendColorStackSize;
        int fogColorStackSize;
        int fillColorStackSize;
        int scissorStackSize;
        int32_t convertK[6];
        hlslpp::float3 keyCenter;
        hlslpp::float3 keyScale;
        uint32_t pendingCommandCurrentBytes;
        uint32_t pendingCommandRemainingBytes;
        std::array<uint8_t, 32 * 8> pendingCommandBuffer; // Enough room for the biggest RDP command
        std::vector<const DisplayList *> triPointerBuffer;
        std::vector<interop::float4> triPosWorkBuffer;
        std::vector<interop::float4> triColWorkBuffer;
        std::vector<interop::float2> triTcWorkBuffer;
        bool crashed;
        CrashReason crashReason;
        std::vector<FramebufferManager::RegionIterator> regionIterators;

        RDP(State *state);
        void setGBI();
        void reset();
        void crash(CrashReason reason);
        void checkFramebufferPair();
        void checkFramebufferOverlap(uint32_t tmemStart, uint32_t tmemWords, uint32_t tmemMask, uint32_t addressStart, uint32_t addressEnd, uint32_t tileWidth, uint32_t tileHeight, bool RGBA32, bool makeTileCopy);
        void checkImageOverlap(uint32_t addressStart, uint32_t addressEnd);
        int32_t movedFromOrigin(int32_t x, uint16_t ori);
        uint32_t maskAddress(uint32_t address);
        void setColorImage(uint8_t fmt, uint8_t siz, uint16_t width, uint32_t address);
        void setDepthImage(uint32_t address);
        void setTextureImage(uint8_t fmt, uint8_t siz, uint16_t width, uint32_t address);
        void setCombine(uint64_t combine);
        void pushCombine();
        void popCombine();
        void setTile(uint8_t tile, uint8_t fmt, uint8_t siz, uint16_t line, uint16_t tmem, uint8_t palette, uint8_t cmt, uint8_t cms, uint8_t maskt, uint8_t masks, uint8_t shiftt, uint8_t shifts);
        void setTileSize(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt);
        void clearTileReplacementHash(uint8_t tile);
        void setTileReplacementHash(uint8_t tile, uint64_t replacementHash);
        void loadTileOperation(const LoadTile &loadTile, const LoadTexture &loadTexture, bool deferred);
        void loadBlockOperation(const LoadTile &loadTile, const LoadTexture &loadTexture, bool deferred);
        void loadTLUTOperation(const LoadTile &loadTile, const LoadTexture &loadTexture, bool deferred);
        void loadTile(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt);
        bool loadTileCopyCheck(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt);
        bool loadTileReplacementCheck(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt, uint8_t imageSiz, uint8_t imageFmt, uint16_t imageLoad, uint16_t imagePal, uint64_t &replacementHash);
        void loadBlock(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t dxt);
        void loadTLUT(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt);
        void setEnvColor(uint32_t color);
        void pushEnvColor();
        void popEnvColor();
        void setPrimColor(uint8_t lodFrac, uint8_t lodMin, uint32_t color);
        void pushPrimColor();
        void popPrimColor();
        void setBlendColor(uint32_t color);
        void pushBlendColor();
        void popBlendColor();
        void setFogColor(uint32_t color);
        void pushFogColor();
        void popFogColor();
        void setFillColor(uint32_t color);
        void pushFillColor();
        void popFillColor();
        void setOtherMode(uint32_t high, uint32_t low);
        void setPrimDepth(uint16_t z, uint16_t dz);
        void setScissor(uint8_t mode, int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);
        void setScissor(uint8_t mode, int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, const ExtendedAlignment &extAlignment);
        void pushScissor();
        void popScissor();
        void setConvert(int32_t k0, int32_t k1, int32_t k2, int32_t k3, int32_t k4, int32_t k5);
        void setKeyR(uint32_t cR, uint32_t sR, uint32_t wR);
        void setKeyGB(uint32_t cG, uint32_t sG, uint32_t wG, uint32_t cB, uint32_t sB, uint32_t wB);
        void fillRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);
        void fillRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, const ExtendedAlignment &extAlignment);
        void setRectAlign(const ExtendedAlignment &extAlignment);
        void setScissorAlign(const ExtendedAlignment &extAlignment);
        void forceUpscale2D(bool force);
        void forceTrueBilerp(uint8_t mode);
        void forceScaleLOD(bool force);
        void clearExtended();

        // The expected size and order for the elements in each array are:
        // pos: triCount * 12, three (X, Y, Z, W) elements
        // tc: triCount * 6, three (U, V) elements
        // col: triCount * 12, three (R, G, B, A) elements
        void drawTris(uint32_t triCount, const float *pos, const float *tc, const float *col, uint8_t tile, uint8_t levels);
        void drawRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip);
        void drawRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip, const ExtendedAlignment &extAlignment);
        void drawTexRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, uint8_t tile, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip);
        void drawTexRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, uint8_t tile, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip, const ExtendedAlignment &extAlignment);
        void updateCallTexcoords(float u, float v);
    };
};