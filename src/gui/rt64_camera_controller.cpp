//
// RT64
//

#include "rt64_camera_controller.h"

#include "common/rt64_math.h"
#include "imgui/imgui.h"

namespace RT64 {
    /*
    CameraController::CameraController() {
        lastCursorPos = { 0, 0 };
    }

    void CameraController::moveCursor(GameFrame &gameFrame, const hlslpp::int2 cursorPos, const hlslpp::int2 windowSize) {
        const bool middleMouseButton = GetAsyncKeyState(VK_MBUTTON) & 0x8000;

        // Automatically enable camera control if the user is pressing the middle mouse button over the viewport.
        const auto &freeCam = gameFrame.freeCamera;
        const uint32_t freeCamIndex = freeCam.sceneIndex;
        const bool validIndex = (freeCamIndex < gameFrame.perspectiveScenes.size());
        if (!freeCam.enabled && validIndex && !ImGui::GetIO().WantCaptureMouse && middleMouseButton) {
            gameFrame.enableFreeCamera(freeCamIndex);
        }

        if (freeCam.enabled && !ImGui::GetIO().WantCaptureMouse) {
            float cameraSpeed = (freeCam.farPlane - freeCam.nearPlane) / 5.0f;
            bool leftAlt = GetAsyncKeyState(VK_LMENU) & 0x8000;
            bool leftCtrl = GetAsyncKeyState(VK_LCONTROL) & 0x8000;
            const hlslpp::float2 delta = hlslpp::float2(cursorPos - lastCursorPos) / hlslpp::float2(windowSize);
            if (middleMouseButton) {
                if (leftCtrl) {
                    movePerspective(gameFrame, { 0.0f, 0.0f, (delta.x + delta.y) * cameraSpeed });
                }
                else if (leftAlt) {
                    const float CameraRotationSpeed = 5.0f;
                    rotatePerspective(gameFrame, delta.x * CameraRotationSpeed, delta.y * CameraRotationSpeed);
                }
                else {
                    movePerspective(gameFrame, { delta.x * cameraSpeed, -delta.y * cameraSpeed, 0.0f });
                }
            }
        }

        lastCursorPos = cursorPos;
    }

    void CameraController::movePerspective(GameFrame &gameFrame, hlslpp::float3 translation) {
        auto &freeCam = gameFrame.freeCamera;
        hlslpp::float4x4 translationMatrix = matrixTranslation(translation);
        freeCam.viewMatrix = mul(freeCam.viewMatrix, translationMatrix);
        freeCam.invViewMatrix = hlslpp::inverse(freeCam.viewMatrix);
    }

    void CameraController::rotatePerspective(GameFrame &gameFrame, float yaw, float pitch) {
        auto &freeCam = gameFrame.freeCamera;
        hlslpp::float3x3 yaw3x3 = matrixRotationY(yaw);
        hlslpp::float3x3 pitch3x3 = matrixRotationZ(pitch);
        hlslpp::float3x3 inv3x3 = extract3x3(freeCam.invViewMatrix);
        inv3x3 = hlslpp::mul(inv3x3, yaw3x3);
        inv3x3 = hlslpp::mul(pitch3x3, inv3x3);
        freeCam.invViewMatrix[0].xyz = inv3x3[0];
        freeCam.invViewMatrix[1].xyz = inv3x3[1];
        freeCam.invViewMatrix[2].xyz = inv3x3[2];
        freeCam.viewMatrix = hlslpp::inverse(freeCam.invViewMatrix);
    }

    void CameraController::lookAtPerspective(GameFrame &gameFrame, hlslpp::float3 position, hlslpp::float3 focus) {
        auto &freeCam = gameFrame.freeCamera;
        const hlslpp::float3 UpVector = hlslpp::float3(0.0f, 1.0f, 0.0f);
        const hlslpp::float3 viewForwardDir = hlslpp::normalize(position - focus);
        const hlslpp::float3 viewSideDir = hlslpp::normalize(hlslpp::cross(UpVector, viewForwardDir));
        const hlslpp::float3 viewUpDir = hlslpp::normalize(hlslpp::cross(viewForwardDir, viewSideDir));
        freeCam.invViewMatrix[0] = hlslpp::float4(viewSideDir, 0.0f);
        freeCam.invViewMatrix[1] = hlslpp::float4(viewUpDir, 0.0f);
        freeCam.invViewMatrix[2] = hlslpp::float4(viewForwardDir, 0.0f);
        freeCam.invViewMatrix[3] = hlslpp::float4(position, 1.0f);
        freeCam.viewMatrix = hlslpp::inverse(freeCam.invViewMatrix);
    }
    */
};