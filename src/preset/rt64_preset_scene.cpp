//
// RT64
//

#include "rt64_preset_scene.h"

#include "imgui/imgui.h"

namespace RT64 {
    // PresetScene
    
    PresetScene::PresetScene() {
        estimateAmbientLight = true;
        ambientLightIntensity = 0.03f;
        ambientBaseColor = { 0.03f, 0.03f, 0.03f };
        ambientNoGIColor = { 0.03f, 0.03f, 0.03f };
        eyeLightDiffuseColor = { 0.008f, 0.008f, 0.008f };
        eyeLightSpecularColor = { 0.004f, 0.004f, 0.004f };
        giDiffuseStrength = 1.5f;
        giBackgroundStrength = 0.5f;
        tonemapExposure = 0.35f;
        tonemapWhite = 1.05f;
        tonemapBlack = 0.0f;
        minLuminance = 0.3f;
        luminanceRange = 0.0f;
        lumaUpdateTime = 1.1f;
    }

    bool PresetScene::readJson(const json &jsonObj) {
        if (!PresetBase::readJson(jsonObj)) {
            return false;
        }

        estimateAmbientLight = jsonObj.value("estimateAmbientLight", true);
        ambientLightIntensity = jsonObj.value("ambientLightIntensity", 0.03f);
        ambientBaseColor = jsonObj.value("ambientBaseColor", hlslpp::float3(0.0f, 0.0f, 0.0f));
        ambientNoGIColor = jsonObj.value("ambientNoGIColor", hlslpp::float3(0.0f, 0.0f, 0.0f));
        eyeLightDiffuseColor = jsonObj.value("eyeLightDiffuseColor", hlslpp::float3(0.0f, 0.0f, 0.0f));
        eyeLightSpecularColor = jsonObj.value("eyeLightSpecularColor", hlslpp::float3(0.0f, 0.0f, 0.0f));
        giDiffuseStrength = jsonObj.value("giDiffuseStrength", 0.0f);
        giBackgroundStrength = jsonObj.value("giBackgroundStrength", 0.0f);
        tonemapExposure = jsonObj.value("tonemapExposure", 0.0f);
        tonemapWhite = jsonObj.value("tonemapWhite", 0.0f);
        tonemapBlack = jsonObj.value("tonemapBlack", 0.0f);
        minLuminance = jsonObj.value("minLuminance", 0.0f);
        luminanceRange = jsonObj.value("luminanceRange", 0.0f);
        lumaUpdateTime = jsonObj.value("lumaUpdateTime", 0.0f);

        return true;
    }

    bool PresetScene::writeJson(json &jsonObj) const {
        if (!PresetBase::writeJson(jsonObj)) {
            return false;
        }

        jsonObj["estimateAmbientLight"] = estimateAmbientLight;
        jsonObj["ambientLightIntensity"] = ambientLightIntensity;
        jsonObj["ambientBaseColor"] = ambientBaseColor;
        jsonObj["ambientNoGIColor"] = ambientNoGIColor;
        jsonObj["eyeLightDiffuseColor"] = eyeLightDiffuseColor;
        jsonObj["eyeLightSpecularColor"] = eyeLightSpecularColor;
        jsonObj["giDiffuseStrength"] = giDiffuseStrength;
        jsonObj["giBackgroundStrength"] = giBackgroundStrength;
        jsonObj["tonemapExposure"] = tonemapExposure;
        jsonObj["tonemapWhite"] = tonemapWhite;
        jsonObj["tonemapBlack"] = tonemapBlack;
        jsonObj["minLuminance"] = minLuminance;
        jsonObj["luminanceRange"] = luminanceRange;
        jsonObj["lumaUpdateTime"] = lumaUpdateTime;

        return true;
    }

    // PresetSceneLibraryInspector

    void PresetSceneLibraryInspector::inspectLibrary(PresetSceneLibrary &library, RenderWindow window) {
        // Preset list.
        ImGui::BeginChild("##scenePresets", ImVec2(0, -64));
        auto presetIt = library.presetMap.begin();
        while (presetIt != library.presetMap.end()) {
            ImGui::PushID(presetIt->first.c_str());

            inspectPresetBegin(library, presetIt, window);
            ImGui::SameLine();
            if (ImGui::Selectable(presetIt->first.c_str(), presetIt->first == selectedPresetName, ImGuiSelectableFlags_AllowItemOverlap)) {
                selectedPresetName = presetIt->first;
            }

            inspectPresetEnd(library, presetIt, window);

            ImGui::PopID();

            presetIt++;
        }

        ImGui::EndChild();

        inspectBottom(library, window);
    }

    void PresetSceneLibraryInspector::inspectSelection(PresetSceneLibrary &library) {
        // Scene inspector.
        if (selectedPresetName.empty()) {
            return;
        }

        auto presetIt = library.presetMap.find(selectedPresetName);
        if (presetIt == library.presetMap.end()) {
            return;
        }

        auto &scene = presetIt->second; 
        const std::string sceneInspectorId = selectedPresetName;

        // This helps prevent widgets that are being edited from applying their changes to
        // other materials when the selection is changed.
        ImGui::PushID(sceneInspectorId.c_str());

        ImGui::Checkbox("Estimate Ambient", & scene.estimateAmbientLight);
        if (scene.estimateAmbientLight) {
            ImGui::DragFloat("Ambient Intensity", &scene.ambientLightIntensity, 0.01f, 0.0f, FLT_MAX);
        }
        else {
            ImGui::DragFloat3("Ambient Base Color", &scene.ambientBaseColor[0], 0.01f, 0.0f, 100.0f);
            ImGui::DragFloat3("Ambient No GI Color", &scene.ambientNoGIColor[0], 0.01f, 0.0f, 100.0f);
        }

        ImGui::DragFloat3("Eye Light Diffuse Color", &scene.eyeLightDiffuseColor[0], 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat3("Eye Light Specular Color", &scene.eyeLightSpecularColor[0], 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("GI Diffuse Strength", &scene.giDiffuseStrength, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("GI Background Strength", &scene.giBackgroundStrength, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("Exposure", &scene.tonemapExposure, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat("White Point", &scene.tonemapWhite, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Black Level", &scene.tonemapBlack, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Eye Adaption Minimum", &scene.minLuminance, 0.01f, -20.0f, 20.0f);
        ImGui::DragFloat("Eye Adaption Range", &scene.luminanceRange, 0.01f, -20.0f, 20.0f);
        ImGui::DragFloat("Eye Adaption Update Time", &scene.lumaUpdateTime, 0.01f, 0.0f, 4.0f);

        ImGui::PopID();
    }
};