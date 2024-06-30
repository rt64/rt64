//
// RT64
//

#include "rt64_camera_controller.h"

#include "common/rt64_math.h"
#include "imgui/imgui.h"

namespace RT64 {
    CameraController::CameraController() {
        lastCursorPos = { 0, 0 };
    }

    void CameraController::moveCursor(DebuggerCamera &camera, const hlslpp::int2 cursorPos, const hlslpp::int2 windowSize) {
        const bool middleMouseButton = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        /*
        // Automatically enable camera control if the user is pressing the middle mouse button over the viewport.
        const uint32_t freeCamIndex = freeCam.sceneIndex;
        const bool validIndex = (freeCamIndex < gameFrame.perspectiveScenes.size());
        if (!freeCam.enabled && validIndex && !ImGui::GetIO().WantCaptureMouse && middleMouseButton) {
            gameFrame.enableFreeCamera(freeCamIndex);
        }
        */
        if (camera.enabled && !ImGui::GetIO().WantCaptureMouse) {
            float cameraSpeed = (camera.farPlane - camera.nearPlane) / 5.0f;
            bool leftAlt = ImGui::IsKeyDown(ImGuiKey_LeftAlt);
            bool leftCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
            const hlslpp::float2 delta = hlslpp::float2(cursorPos - lastCursorPos) / hlslpp::float2(windowSize);
            if (middleMouseButton) {
                if (leftCtrl) {
                    movePerspective(camera, { 0.0f, 0.0f, (delta.x + delta.y) * cameraSpeed });
                }
                else if (leftAlt) {
                    const float CameraRotationSpeed = 5.0f;
                    rotatePerspective(camera, delta.x * CameraRotationSpeed, delta.y * CameraRotationSpeed);
                }
                else {
                    movePerspective(camera, { delta.x * cameraSpeed, -delta.y * cameraSpeed, 0.0f });
                }
            }
        }

        lastCursorPos = cursorPos;
    }

    void CameraController::movePerspective(DebuggerCamera &camera, hlslpp::float3 translation) {
        hlslpp::float4x4 translationMatrix = matrixTranslation(translation);
        camera.viewMatrix = mul(camera.viewMatrix, translationMatrix);
        camera.invViewMatrix = hlslpp::inverse(camera.viewMatrix);
    }

    void CameraController::rotatePerspective(DebuggerCamera &camera, float yaw, float pitch) {
        hlslpp::float3x3 yaw3x3 = matrixRotationY(yaw);
        hlslpp::float3x3 pitch3x3 = matrixRotationZ(pitch);
        hlslpp::float3x3 inv3x3 = extract3x3(camera.invViewMatrix);
        inv3x3 = hlslpp::mul(inv3x3, yaw3x3);
        inv3x3 = hlslpp::mul(pitch3x3, inv3x3);
        camera.invViewMatrix[0].xyz = inv3x3[0];
        camera.invViewMatrix[1].xyz = inv3x3[1];
        camera.invViewMatrix[2].xyz = inv3x3[2];
        camera.viewMatrix = hlslpp::inverse(camera.invViewMatrix);
    }

    void CameraController::lookAtPerspective(DebuggerCamera &camera, hlslpp::float3 position, hlslpp::float3 focus) {
        const hlslpp::float3 UpVector = hlslpp::float3(0.0f, 1.0f, 0.0f);
        const hlslpp::float3 viewForwardDir = hlslpp::normalize(position - focus);
        const hlslpp::float3 viewSideDir = hlslpp::normalize(hlslpp::cross(UpVector, viewForwardDir));
        const hlslpp::float3 viewUpDir = hlslpp::normalize(hlslpp::cross(viewForwardDir, viewSideDir));
        camera.invViewMatrix[0] = hlslpp::float4(viewSideDir, 0.0f);
        camera.invViewMatrix[1] = hlslpp::float4(viewUpDir, 0.0f);
        camera.invViewMatrix[2] = hlslpp::float4(viewForwardDir, 0.0f);
        camera.invViewMatrix[3] = hlslpp::float4(position, 1.0f);
        camera.viewMatrix = hlslpp::inverse(camera.invViewMatrix);
    }
};