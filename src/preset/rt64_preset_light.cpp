//
// RT64
//

#include "rt64_preset_light.h"

#include "im3d/im3d.h"
#include "im3d/im3d_math.h"
#include "imgui/imgui.h"

#include "common/rt64_common.h"
#include "gui/rt64_file_dialog.h"
#include "shared/rt64_hlsl_json.h"

namespace interop {
    void to_json(json &j, const PointLight &l) {
        j = {
            { "position", l.position },
            { "direction", l.direction },
            { "attenuationRadius", l.attenuationRadius },
            { "pointRadius", l.pointRadius },
            { "spotFalloffCosine", l.spotFalloffCosine },
            { "spotMaxCosine", l.spotMaxCosine },
            { "diffuseColor", l.diffuseColor },
            { "specularColor", l.specularColor },
            { "shadowOffset", l.shadowOffset },
            { "attenuationExponent", l.attenuationExponent },
            { "flickerIntensity", l.flickerIntensity },
            { "groupBits", l.groupBits }
        };
    }

    void from_json(const json &j, PointLight &l) {
        l.position = j.value("position", hlslpp::float3(0.0f, 0.0f, 0.0f));
        l.direction = j.value("direction", hlslpp::float3(0.0f, -10.0f, 0.0f));
        l.attenuationRadius = j.value("attenuationRadius", 500.0f);
        l.pointRadius = j.value("pointRadius", 5.0f);
        l.spotFalloffCosine = j.value("spotFalloffCosine", 1.0f);
        l.spotMaxCosine = j.value("spotMaxCosine", 1.0f);
        l.diffuseColor = j.value("diffuseColor", hlslpp::float3(1.0f, 1.0f, 1.0f));
        l.specularColor = j.value("specularColor", hlslpp::float3(0.5f, 0.5f, 0.5f));
        l.shadowOffset = j.value("shadowOffset", 5.0f);
        l.attenuationExponent = j.value("attenuationExponent", 1.0f);
        l.flickerIntensity = j.value("flickerIntensity", 0.0f);
        l.groupBits = j.value("groupBits", 1UL);
    }
};

namespace RT64 {
    // PresetLights

    bool PresetLights::readJson(const json &jsonObj) {
        if (!PresetBase::readJson(jsonObj)) {
            return false;
        }

        auto it = jsonObj.find("lights");
        if (it == jsonObj.end()) {
            return false;
        }

        for (const json &jlight : (*it)) {
            std::string name = jlight["name"];
            if (name.empty()) {
                continue;
            }

            Light light;
            light.description = jlight["description"];
            light.enabled = jlight["enabled"];
            lightMap[name] = light;
        }

        return true;
    }

    bool PresetLights::writeJson(json &jsonObj) const {
        if (!PresetBase::writeJson(jsonObj)) {
            return false;
        }

        for (const auto &it : lightMap) {
            json jlight;
            jlight["name"] = it.first;
            jlight["description"] = it.second.description;
            jlight["enabled"] = it.second.enabled;
            jsonObj["lights"].push_back(jlight);
        }

        return true;
    }

    // PresetLightsLibrary

    void PresetLightsLibraryInspector::inspectLibrary(PresetLightsLibrary &library, RenderWindow window) {
        Im3d::AppData &appData = Im3d::GetAppData();
        Im3d::PushEnableSorting(true);

        /*
        auto focusViewOnPoint = [&appData, &projectionInspector](const hlslpp::float3 &point) {
            const float DistanceFromView = 150.0f;
            hlslpp::float3 viewDir = { appData.m_viewDirection.x, appData.m_viewDirection.y, appData.m_viewDirection.z };
            hlslpp::float3 viewPosition = point + viewDir * DistanceFromView;
            projectionInspector.lookAtPerspective(viewPosition, point);
            projectionInspector.cameraControl.enabled = true;
        };
        */

        // Preset list.
        ImGui::BeginChild("##lightPresets", ImVec2(0, -64));
        auto presetIt = library.presetMap.begin();
        while (presetIt != library.presetMap.end()) {
            ImGui::PushID(presetIt->first.c_str());

            inspectPresetBegin(library, presetIt, window);
            ImGui::SameLine();
            bool expandedHeader = ImGui::CollapsingHeader(presetIt->first.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap);
            inspectPresetEnd(library, presetIt, window);

            if (expandedHeader) {
                ImGui::Indent();

                Im3d::Ray cursorRay(appData.m_cursorRayOrigin, appData.m_cursorRayDirection);
                for (auto &lightIt : presetIt->second.lightMap) {
                    // Im3d
                    const auto &srcPos = lightIt.second.description.position;
                    const auto &srcCol = lightIt.second.description.diffuseColor;
                    const hlslpp::float3 pos = { srcPos.x, srcPos.y, srcPos.z };
                    const hlslpp::float3 col = { srcCol.x, srcCol.y, srcCol.z };
                    const float SphereScale = 0.01f;
                    const float SphereAlpha = 0.5f;
                    const float SphereHighlightAlpha = 1.0f;
                    float colLength = hlslpp::length(col);
                    Im3d::Vec3 lightPos = { pos.x, pos.y, pos.z };
                    Im3d::Color lightColor = { col.x / colLength, col.y / colLength, col.z / colLength };
                    Im3d::SetColor(lightColor);

                    float radius = Im3d::GetContext().pixelsToWorldSize(lightPos, appData.m_viewportSize.y * SphereScale);
                    if (Im3d::Intersects(cursorRay, Im3d::Sphere(lightPos, radius))) {
                        Im3d::SetAlpha(SphereHighlightAlpha);
                        if (ImGui::IsMouseClicked(0)) {
                            selectedPresetName = presetIt->first;
                            selectedLightName = lightIt.first;
                            skipGizmos = true;
                        }

                        if (ImGui::IsMouseDoubleClicked(0)) {
                            //focusViewOnPoint(pos);
                        }
                    }
                    else {
                        Im3d::SetAlpha(SphereAlpha);
                    }

                    Im3d::DrawSphereFilled(lightPos, radius);
                    Im3d::SetAlpha(1.0f);

                    // ImGui
                    ImGui::PushID(lightIt.first.c_str());
                    ImGui::Checkbox("##enabled", &lightIt.second.enabled);
                    ImGui::SameLine();
                    if (ImGui::Selectable(lightIt.first.c_str(), (presetIt->first == selectedPresetName) && (lightIt.first == selectedLightName), ImGuiSelectableFlags_AllowDoubleClick)) {
                        selectedPresetName = presetIt->first;
                        selectedLightName = lightIt.first;

                        if (ImGui::IsMouseDoubleClicked(0)) {
                            //focusViewOnPoint(pos);
                        }
                    }

                    ImGui::PopID();
                }

                auto &lightMap = presetIt->second.lightMap;
                auto generateLightName = [&](const std::string baseName) {
                    // If the name already ends with a number, we try incrementing it instead.
                    std::string newLightName = baseName;
                    if (!newLightName.empty()) {
                        size_t lastDigit = (newLightName.size() - 1);
                        if (isdigit(newLightName[lastDigit])) {
                            bool stopIncrementing = false;
                            do {
                                int attemptDigit = static_cast<int>(lastDigit);
                                while ((newLightName[attemptDigit] == '9') && (attemptDigit >= 0)) {
                                    newLightName[attemptDigit] = '0';
                                    attemptDigit--;

                                    if (attemptDigit < 0) {
                                        stopIncrementing = true;
                                        break;
                                    }
                                    // Insert a new base 10 right before this digit.
                                    else if (!isdigit(newLightName[attemptDigit])) {
                                        newLightName = newLightName.substr(0, attemptDigit + 1) + "1" + newLightName.substr(attemptDigit + 1);
                                        lastDigit++;
                                        break;
                                    }
                                }

                                if (isdigit(newLightName[attemptDigit]) && (newLightName[attemptDigit] < '9')) {
                                    newLightName[attemptDigit] += 1;
                                }
                            } while (!stopIncrementing && (lightMap.find(newLightName) != lightMap.end()));

                            if (lightMap.find(newLightName) == lightMap.end()) {
                                return newLightName;
                            }
                        }
                    }

                    // Generate a new light name automatically by appending a number suffix to the base name.
                    unsigned int newLightCounter = 0;
                    do {
                        const size_t LeadingZeroes = 3;
                        std::string numberSuffix = std::to_string(newLightCounter);
                        numberSuffix = std::string(LeadingZeroes - std::min(LeadingZeroes, numberSuffix.length()), '0') + numberSuffix;
                        newLightName = baseName + "_" + numberSuffix;
                        newLightCounter++;
                    } while (lightMap.find(newLightName) != lightMap.end());

                    return newLightName;
                };

                if (ImGui::Button("New##light")) {
                    std::string newLightName = generateLightName("point_00");
                    lightMap[newLightName] = PresetLights::Light();
                    auto &desc = lightMap[newLightName].description;
                    const float DistanceFromView = -150.0f;
                    hlslpp::float3 viewPos = { appData.m_viewOrigin.x, appData.m_viewOrigin.y, appData.m_viewOrigin.z };
                    hlslpp::float3 viewDir = { appData.m_viewDirection.x, appData.m_viewDirection.y, appData.m_viewDirection.z };
                    desc.position = viewPos + viewDir * DistanceFromView;
                    desc.direction = { 0.0f, -1.0f, 0.0f };
                    desc.diffuseColor = { 1.0f, 1.0f, 1.0f };
                    desc.attenuationRadius = 500.0f;
                    desc.pointRadius = 15.0f;
                    desc.spotFalloffCosine = 1.0f;
                    desc.spotMaxCosine = 1.0f;
                    desc.specularColor = { 0.5f, 0.5f, 0.5f };
                    desc.shadowOffset = 5.0f;
                    desc.attenuationExponent = 1.0f;
                    desc.groupBits = 1;
                    lightMap[newLightName].enabled = true;
                    selectedLightName = newLightName;
                    selectedPresetName = presetIt->first;
                }

                bool lightSelected = (selectedPresetName == presetIt->first) && (lightMap.find(selectedLightName) != lightMap.end());
                ImGui::BeginDisabled(!lightSelected);
                ImGui::SameLine();
                if (ImGui::Button("Duplicate##light")) {
                    std::string newLightName = generateLightName(selectedLightName);
                    lightMap[newLightName] = lightMap[selectedLightName];
                    selectedLightName = newLightName;
                    selectedPresetName = presetIt->first;
                }

                ImGui::SameLine();
                if (ImGui::Button("Delete##light")) {
                    lightMap.erase(selectedLightName);
                    selectedLightName = std::string();
                    selectedPresetName = std::string();
                }

                ImGui::EndDisabled();
                ImGui::Unindent();
            }

            ImGui::PopID();

            presetIt++;
        }

        ImGui::EndChild();
        Im3d::PopEnableSorting();

        // Bottom section.
        inspectBottom(library, window);
    }

    void PresetLightsLibraryInspector::inspectSelection(PresetLightsLibrary &library) {
        // Light inspector.
        if (selectedPresetName.empty() || selectedLightName.empty()) {
            return;
        }

        auto presetIt = library.presetMap.find(selectedPresetName);
        if (presetIt == library.presetMap.end()) {
            return;
        }

        auto &lightMap = presetIt->second.lightMap;
        auto lightIt = lightMap.find(selectedLightName);
        if (lightIt == lightMap.end()) {
            return;
        }

        Im3d::AppData &appData = Im3d::GetAppData();
        auto &lightDesc = lightIt->second.description;
        const std::string lightInspectorId = selectedPresetName + "_" + selectedLightName;
        char lightName[256];
        strncpy(lightName, lightIt->first.c_str(), sizeof(lightName));
        ImGui::InputText("Name", lightName, sizeof(lightName));
        if (strcmp(lightIt->first.c_str(), lightName) != 0) {
            if (lightMap.find(std::string(lightName)) != lightMap.end()) {
                ImGui::Text("That name is already being used.");
            }
            else if (strlen(lightName) == 0) {
                ImGui::Text("Please enter a valid name.");
            }
            else {
                auto extractedLight = lightMap.extract(lightIt->first);
                extractedLight.key() = std::string(lightName);
                lightMap.insert(std::move(extractedLight));
                selectedLightName = std::string(lightName);
                lightIt = lightMap.find(lightName);
                lightDesc = lightIt->second.description;
            }
        }

        // This helps prevent widgets that are being edited from applying their changes to
        // other lights when the selection is changed.
        ImGui::PushID(lightInspectorId.c_str());

        const int SphereDetail = 64;
        ImGui::DragFloat3("Position", &lightDesc.position.x);
        ImGui::DragFloat3("Direction", &lightDesc.direction.x);
        ImGui::DragFloat3("Diffuse color", &lightDesc.diffuseColor.x, 0.01f);
        ImGui::DragFloat("Attenuation radius", &lightDesc.attenuationRadius, 1.0f, 0.0f, FLT_MAX);
        ImGui::DragFloat("Point radius", &lightDesc.pointRadius, 1.0f, 0.0f, FLT_MAX);
        ImGui::DragFloat("Spot maximum cosine", &lightDesc.spotMaxCosine, 0.005f, -1.0f, 1.0f);
        ImGui::DragFloat("Spot falloff cosine", &lightDesc.spotFalloffCosine, 0.005f, -1.0f, lightDesc.spotMaxCosine);
        ImGui::DragFloat3("Specular color", &lightDesc.specularColor.x, 0.01f);
        ImGui::DragFloat("Shadow offset", &lightDesc.shadowOffset, 1.0f, 0.0f, FLT_MAX);
        ImGui::DragFloat("Attenuation exponent", &lightDesc.attenuationExponent, 0.01f, 0.0f, 256.0f);
        ImGui::DragFloat("Flicker intensity", &lightDesc.flickerIntensity, 0.01f, 0.0f, 1.0f);
        ImGui::InputInt("Group bits", (int *)(&lightDesc.groupBits));

        lightDesc.spotFalloffCosine = std::min(lightDesc.spotFalloffCosine, lightDesc.spotMaxCosine);

        // Skip drawing gizmos for a single frame to skip mouse clicks that were used by other events in the frame.
        if (skipGizmos) {
            skipGizmos = false;
        }
        else {
            Im3d::SetAlpha(1.0f);
            Im3d::GizmoTranslation("GizmoPosition", &lightDesc.position.x);

            if ((lightDesc.spotFalloffCosine < 1.0f) || (lightDesc.spotMaxCosine < 1.0f)) {
                const Im3d::Vec3 lightPos = { lightDesc.position.x, lightDesc.position.y, lightDesc.position.z };
                const Im3d::Vec3 focusDir = { lightDesc.direction.x, lightDesc.direction.y, lightDesc.direction.z };
                const Im3d::Vec3 spherePos = lightPos + focusDir;
                float sphereRadius = Im3d::GetContext().pixelsToWorldSize(spherePos, appData.m_viewportSize.y * 0.01f);
                Im3d::DrawLine(lightPos, spherePos, 5.0f, Im3d::Color(0.6f, 0.75f, 0.9f, 1.0f));
                Im3d::SetColor(0.9f, 0.75f, 0.6f, 1.0f);
                Im3d::DrawSphereFilled(spherePos, sphereRadius);

                auto drawConeCircles = [&](const float cosine) {
                    const auto &srcDir = lightDesc.direction;
                    const hlslpp::float3 lightDir = { srcDir.x, srcDir.y, srcDir.z };
                    float spotRadius = hlslpp::length(lightDir) * tanf(acosf(-cosine));
                    const int Segments = 20;
                    const float SegmentMult = (3.0f / Segments);
                    for (int i = 0; i < Segments; i++) {
                        const float iF = static_cast<float>(i);
                        Im3d::DrawCircle(lightPos + focusDir * SegmentMult * iF, focusDir, spotRadius * SegmentMult * iF);
                    }
                };

                if (lightDesc.spotMaxCosine < 0.0f) {
                    Im3d::SetColor(1.0f, 0.6f, 0.6f, 1.0f);
                    drawConeCircles(lightDesc.spotMaxCosine);
                }

                if (lightDesc.spotFalloffCosine < 0.0f) {
                    Im3d::SetColor(0.6f, 1.0f, 0.6f, 1.0f);
                    drawConeCircles(lightDesc.spotFalloffCosine);
                }
            }
        }

        const auto &srcCol = lightDesc.diffuseColor;
        const hlslpp::float3 col = { srcCol.x, srcCol.y, srcCol.z };
        float colLength = hlslpp::length(col);
        const Im3d::Vec3 lightPos(lightDesc.position.x, lightDesc.position.y, lightDesc.position.z);
        const Im3d::Color lightCol(lightDesc.diffuseColor.x / colLength, lightDesc.diffuseColor.y / colLength, lightDesc.diffuseColor.z / colLength);
        Im3d::SetColor(lightCol);
        Im3d::SetAlpha(1.0f);
        Im3d::DrawSphere(lightPos, lightDesc.attenuationRadius, SphereDetail);

        Im3d::SetAlpha(0.75f);
        Im3d::DrawSphere(lightPos, lightDesc.pointRadius, SphereDetail);

        Im3d::SetAlpha(0.5f);
        Im3d::DrawSphere(lightPos, lightDesc.shadowOffset, SphereDetail);

        ImGui::PopID();
    }
};