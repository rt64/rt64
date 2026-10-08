//
// RT64
//

#pragma once

#include <set>

#include "preset/rt64_preset_draw_call.h"
#include "hle/rt64_game_frame.h"
#include "hle/rt64_vi.h"

namespace RT64 {
    struct DebuggerInspector {
        struct CallIndices {
            uint32_t fbPairIndex;
            uint32_t projectionIndex;
            uint32_t drawCallIndex;
        };

        struct FileDialogResults {
            // Hold the results of file dialog functions in a shared pointer that can remain alive even after the
            // inspector is deleted, as the callback can run later on a different thread depending on the platform.
            std::recursive_mutex mutex;
            std::filesystem::path replaceTextureFilename;
            uint64_t replaceTextureHash;
            std::filesystem::path dumpPNGPath;
            std::filesystem::path dumpTMEMPath;
            LoadTile dumpPNGLoadTile;
            uint32_t dumpPNGTlut;
            uint64_t dumpTextureHash;
        };

        std::shared_ptr<FileDialogResults> dialogResults;

        int32_t openLoadIndex;
        int32_t openTileIndex;
        CallIndices openCallIndices;
        CallIndices highightCallIndices;
        bool openCall;
        std::vector<CallIndices> popupCalls;
        bool paused;
        DebuggerRenderer renderer;
        DebuggerCamera camera;
        bool viewTransformGroups;
        bool viewNativeSamplers;

        DebuggerInspector();
        std::string framebufferPairName(const Workload &workload, uint32_t fbPairIndex);
        std::string projectionName(const Workload &workload, uint32_t fbPairIndex, uint32_t projectionIndex);
        void highlightDrawCall(Workload &workload, CallIndices call);
        bool checkPopup(Workload &workload);
        void inspect(RenderWorker *directWorker, const VI &vi, Workload &workload, FramebufferManager &fbManager, TextureCache &textureCache, DrawCallKey &outDrawCallKey, bool &outCreateDrawCallKey, RenderWindow window);
        void rightClick(const Workload &workload, hlslpp::float2 cursorPos);
        void enableFreeCamera(const Workload &workload, uint32_t fbPairIndex, uint32_t projIndex);
    };
};