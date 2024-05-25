//
// RT64
//

#pragma once

#include "hle/rt64_workload.h"

namespace RT64 {
    struct CameraController {
        hlslpp::int2 lastCursorPos;

        CameraController();
        void moveCursor(DebuggerCamera &camera, const hlslpp::int2 cursorPos, const hlslpp::int2 windowSize);
        void movePerspective(DebuggerCamera &camera, const hlslpp::float3 translation);
        void rotatePerspective(DebuggerCamera &camera, float yaw, float pitch);
        void lookAtPerspective(DebuggerCamera &camera, const hlslpp::float3 position, const hlslpp::float3 focus);
    };
};