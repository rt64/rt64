//
// RT64
//

#pragma once

#include <map>
#include <set>
#include <vector>

#include "render/rt64_descriptor_sets.h"
#include "render/rt64_render_target_manager.h"
#include "render/rt64_render_worker.h"
#include "render/rt64_shader_library.h"
#include "render/rt64_texture_cache.h"
#include "shared/rt64_fb_common.h"
#include "shared/rt64_fb_reinterpret.h"
#include "shared/rt64_texture_copy.h"

#include "rt64_framebuffer.h"
#include "rt64_framebuffer_changes.h"
#include "rt64_framebuffer_storage.h"

namespace RT64 {
    struct FramebufferOperation {
        enum class Type {
            None,
            WriteChanges,
            CreateTileCopy,
            ReinterpretTile
        };

        Type type;

        union {
            struct {
                uint32_t address;
                uint64_t id;
                FramebufferTile fbTile;
            } createTileCopy;

            struct {
                uint32_t address;
                uint64_t id;
            } writeChanges;

            struct {
                uint64_t srcId;
                uint8_t srcSiz;
                uint8_t srcFmt;
                uint64_t dstId;
                uint8_t dstSiz;
                uint8_t dstFmt;
                bool ulScaleS;
                bool ulScaleT;
                interop::uint2 texelShift;
                interop::uint2 texelMask;
                uint64_t tlutHash;
                uint32_t tlutFormat;
            } reinterpretTile;
        };
    };

    struct FramebufferManager {
        struct alignas(16) RtCommonCB {
            uint32_t Offset[2];
        };

        struct RegionTMEM {
            uint32_t tmemStart;
            uint32_t tmemEnd;
            FramebufferTile fbTile;
            uint64_t tileCopyId;
            bool syncRequired;
        };

        struct TileCopy {
            uint64_t id = 0;
            std::unique_ptr<RenderTexture> texture;
            uint32_t textureWidth = 0;
            uint32_t textureHeight = 0;
            uint32_t address = 0;
            uint32_t left = 0;
            uint32_t top = 0;
            uint32_t usedWidth = 0;
            uint32_t usedHeight = 0;
            uint64_t usedTimestamp = 0;
            bool ulScaleS = true;
            bool ulScaleT = true;
            interop::uint2 texelShift = { 0, 0 };
            interop::uint2 texelMask = { UINT_MAX, UINT_MAX };
            interop::uint2 ditherOffset = { 0, 0 };
            uint32_t ditherPattern = 0;
            float sampleScale = 1.0f;
            bool readColorFromStorage = false;
            bool readDepthFromStorage = false;
            bool ignore = false;
            std::unique_ptr<RenderFramebuffer> framebuffer;
        };

        struct CheckCopyResult {
            uint64_t tileId = 0;
            uint32_t tileWidth = 0;
            uint32_t tileHeight = 0;
            uint32_t lineWidth = 0;
            uint8_t siz = 0;
            uint8_t fmt = 0;
            bool reinterpret = false;
            bool syncRequired = false;

            CheckCopyResult() = default;

            bool valid() const {
                return (tileWidth > 0);
            }
        };

        struct CommandListCopyRegion {
            RenderTexture *srcTexture;
            RenderTexture *dstTexture;
            RenderFramebuffer *dstFramebuffer;
            RenderDescriptorSet *descriptorSet;
            interop::TextureCopyCB pushConstants;
        };

        struct CommandListCopies {
            std::vector<CommandListCopyRegion> cmdListCopyRegions;
            std::set<RenderTarget *> copyRegionTargets;

            void clear() {
                cmdListCopyRegions.clear();
                copyRegionTargets.clear();
            }
        };

        struct CommandListReinterpretDispatch {
            RenderTexture *srcTexture;
            RenderTexture *dstTexture;
            interop::FbReinterpretCB reinterpretCB;
            RenderDescriptorSet *descriptorSet;
        };

        typedef std::pair<const RenderTexture *, RenderFormat> TextureFormatPair;

        struct CommandListReinterpretations {
            std::vector<CommandListReinterpretDispatch> cmdListDispatches;

            void clear() {
                cmdListDispatches.clear();
            }
        };

        std::unordered_map<uint32_t, Framebuffer> framebuffers;
        std::unordered_map<uint64_t, TileCopy> tileCopies;
        std::unique_ptr<RenderTexture> dummyTLUTTexture;
        std::vector<std::unique_ptr<ReinterpretDescriptorSet>> descriptorReinterpretSets;
        uint32_t descriptorReinterpretSetsCount = 0;
        std::list<RegionTMEM> activeRegionsTMEM;
        FramebufferChangePool scratchChangePool;
        uint64_t usedTimestamp = 0;
        uint64_t writeTimestamp = 0;

        typedef std::list<RegionTMEM>::iterator RegionIterator;

        FramebufferManager();
        ~FramebufferManager();
        Framebuffer &get(uint32_t address, uint8_t siz, uint32_t width, uint32_t height);
        Framebuffer *find(uint32_t address) const;
        Framebuffer *findMostRecentContaining(uint32_t addressStart, uint32_t addressEnd);
        void writeChanges(RenderWorker *renderWorker, const FramebufferChangePool &fbChangePool, const FramebufferOperation &op, RenderTargetManager &targetManager, const ShaderLibrary *shaderLibrary);
        void clearUsedTileCopies();
        uint64_t findTileCopyId(uint32_t width, uint32_t height);
        void createTileCopySetup(RenderWorker *renderWorker, const FramebufferOperation &op, hlslpp::float2 resolutionScale, RenderTargetManager &targetManager, std::unordered_set<RenderTarget *> *resizedTargets);
        void createTileCopyRecord(RenderWorker *renderWorker, const FramebufferOperation &op, const FramebufferStorage &fbStorage, RenderTargetManager &targetManager, 
            hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex, CommandListCopies &cmdListCopies, const ShaderLibrary *shaderLibrary);

        void reinterpretTileSetup(RenderWorker *renderWorker, const FramebufferOperation &op, hlslpp::float2 resolutionScale, bool usesHDR);
        void reinterpretTileRecord(RenderWorker *renderWorker, const FramebufferOperation &op, TextureCache &textureCache, hlslpp::float2 resolutionScale,
            uint64_t submissionFrame, bool usesHDR, CommandListReinterpretations &cmdListReinterpretations);

        bool makeFramebufferTile(Framebuffer *fb, uint32_t addressStart, uint32_t addressEnd, uint32_t lineWidth, uint32_t tileHeight, FramebufferTile &outTile, bool RGBA32);

        FramebufferOperation makeTileCopyTMEM(uint64_t dstTileId, const FramebufferTile &fbTile);

        FramebufferOperation makeTileReintepretation(uint64_t srcTileId, uint8_t srcSiz, uint8_t srcFmt, uint64_t dstTileId, uint8_t dstSiz, uint8_t dstFmt, bool ulScaleS, bool ulScaleT,
            interop::uint2 texelShift, interop::uint2 texelMask, uint64_t tlutHash, uint32_t tlutFormat);

        CheckCopyResult checkTileCopyTMEM(uint32_t tmem, uint32_t lineWidth, uint8_t siz, uint8_t fmt, uint16_t uls);
        void insertRegionsTMEM(uint32_t addressStart, uint32_t tmemStart, uint32_t tmemWords, uint32_t tmemMask, bool RGBA32, bool syncRequired, std::vector<RegionIterator> *resultRegions);
        void discardRegionsTMEM(uint32_t tmemStart, uint32_t tmemWords, uint32_t tmemMask);
        void storeRAM(FramebufferStorage &fbStorage, const uint8_t *RDRAM, uint32_t fbPairIndex);
        void checkRAM(const uint8_t *RDRAM, std::vector<Framebuffer *> &differentFbs, bool updateHashes);
        void uploadRAM(RenderWorker *renderWorker, Framebuffer **differentFbs, size_t differentFbsCount, FramebufferChangePool &fbChangePool, const uint8_t *RDRAM, bool canDiscard, std::vector<FramebufferOperation> &fbOps,
            std::vector<uint32_t> &fbDiscards, const ShaderLibrary *shaderLibrary);

        void resetTracking();
        void hashTracking(const uint8_t *RDRAM);
        void changeRAM(Framebuffer *changedFb, uint32_t addressStart, uint32_t addressEnd);

        void resetOperations();

        void setupOperations(RenderWorker *renderWorker, const std::vector<FramebufferOperation> &operations, hlslpp::float2 resolutionScale, RenderTargetManager &targetManager, std::unordered_set<RenderTarget *> *resizedTargets);

        void recordOperations(RenderWorker *renderWorker, const FramebufferChangePool *fbChangePool, const FramebufferStorage *fbStorage, const ShaderLibrary *shaderLibrary, TextureCache *textureCache,
            const std::vector<FramebufferOperation> &operations, RenderTargetManager &targetManager, hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex,
            uint64_t submissionFrame);
        
        // Execution must finish before calling this again. A convenience function around reset, setup and record.
        void performOperations(RenderWorker *renderWorker, const FramebufferChangePool *fbChangePool, const FramebufferStorage *fbStorage, const ShaderLibrary *shaderLibrary, TextureCache *textureCache, 
            const std::vector<FramebufferOperation> &operations, RenderTargetManager &targetManager, hlslpp::float2 resolutionScale, uint32_t maxFbPairIndex,
            uint64_t submissionFrame, std::unordered_set<RenderTarget *> *resizedTargets = nullptr);

        void performDiscards(const std::vector<uint32_t> &discards);

        void destroyAllTileCopies();
        uint64_t nextWriteTimestamp();
    };
};