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
        void inspect(const VI &vi, Workload &workload, FramebufferManager &fbManager, TextureCache &textureCache, DrawCallKey &outDrawCallKey, bool &outCreateDrawCallKey, RenderWindow window);
        void rightClick(const Workload &workload, hlslpp::float2 cursorPos);
        void enableFreeCamera(const Workload &workload, uint32_t fbPairIndex, uint32_t projIndex);
    };
};