//
// RT64
//

#include "rt64_preset_material.h"

#include "imgui/imgui.h"

#include "shared/rt64_hlsl_json.h"

namespace interop {
    void to_json(json &j, const ExtraParams &e) {
        j = {
            { "ignoreNormalFactor", e.ignoreNormalFactor },
            { "uvDetailScale", e.uvDetailScale },
            { "reflectionFactor", e.reflectionFactor },
            { "reflectionFresnelFactor", e.reflectionFresnelFactor },
            { "roughnessFactor", e.roughnessFactor },
            { "refractionFactor", e.refractionFactor },
            { "shadowCatcherFactor", e.shadowCatcherFactor },
            { "specularColor", e.specularColor },
            { "specularExponent", e.specularExponent },
            { "solidAlphaMultiplier", e.solidAlphaMultiplier },
            { "shadowAlphaMultiplier", e.shadowAlphaMultiplier },
            { "depthOrderBias", e.depthOrderBias },
            { "depthDecalBias", e.depthDecalBias },
            { "shadowRayBias", e.shadowRayBias },
            { "selfLight", e.selfLight },
            { "lightGroupMaskBits", e.lightGroupMaskBits },
            { "diffuseColorMix", e.diffuseColorMix },
            { "rspLightDiffuseMix", e.rspLightDiffuseMix },
            { "enabledAttributes", e.enabledAttributes }
        };
    }

    void from_json(const json &j, ExtraParams &e) {
        e.ignoreNormalFactor = j.value("ignoreNormalFactor", 0.0f);
        e.uvDetailScale = j.value("uvDetailScale", 0.0f);
        e.reflectionFactor = j.value("reflectionFactor", 0.0f);
        e.reflectionFresnelFactor = j.value("reflectionFresnelFactor", 0.0f);
        e.roughnessFactor = j.value("roughnessFactor", 0.0f);
        e.refractionFactor = j.value("refractionFactor", 0.0f);
        e.shadowCatcherFactor = j.value("shadowCatcherFactor", 0.0f);
        e.specularColor = j.value("specularColor", float3(1.0f, 1.0f, 1.0f));
        e.specularExponent = j.value("specularExponent", 1.0f);
        e.solidAlphaMultiplier = j.value("solidAlphaMultiplier", 1.0f);
        e.shadowAlphaMultiplier = j.value("shadowAlphaMultiplier", 1.0f);
        e.depthOrderBias = j.value("depthOrderBias", 0.0f);
        e.depthDecalBias = j.value("depthDecalBias", 0.0f);
        e.shadowRayBias = j.value("shadowRayBias", 0.0f);
        e.selfLight = j.value("selfLight", float3(0.0f, 0.0f, 0.0f));
        e.lightGroupMaskBits = j.value("lightGroupMaskBits", 0UL);
        e.diffuseColorMix = j.value("diffuseColorMix", float4(0.0f, 0.0f, 0.0f, 0.0f));
        e.rspLightDiffuseMix = j.value("rspLightDiffuseMix", 0.0f);
        e.enabledAttributes = j.value("enabledAttributes", 0L);
    }
};

namespace RT64 {
    // PresetMaterial

    PresetMaterial::PresetMaterial() {
        memset(&description, 0, sizeof(description));
        light.primColorTint = 0.0f;
        light.primAlphaAttenuation = 0.0f;
        light.envColorTint = 0.0f;
        light.envAlphaAttenuation = 0.0f;
        light.scale = 1.0f;
        interpolation.enabled = true;
#   if SCRIPT_ENABLED
        interpolation.callMatchCallback = nullptr;
#   endif
    }

    bool PresetMaterial::readJson(const json &jsonObj) {
        if (!PresetBase::readJson(jsonObj)) {
            return false;
        }

        auto it = jsonObj.find("description");
        if (it == jsonObj.end()) {
            return false;
        }

        description = *it;
        light.presetName = jsonObj.value("lightPresetName", "");
        light.primColorTint = jsonObj.value("lightPrimColorTint", 0.0f);
        light.primAlphaAttenuation = jsonObj.value("lightPrimAlphaAttenuation", 0.0f);
        light.envColorTint = jsonObj.value("lightEnvColorTint", 0.0f);
        light.envAlphaAttenuation = jsonObj.value("lightEnvAlphaAttenuation", 0.0f);
        light.scale = jsonObj.value("lightScale", 1.0f);
        interpolation.enabled = jsonObj.value("interpolationEnabled", true);

        return true;
    }

    bool PresetMaterial::writeJson(json &jsonObj) const {
        if (!PresetBase::writeJson(jsonObj)) {
            return false;
        }

        jsonObj["description"] = description;
        jsonObj["lightPresetName"] = light.presetName;
        jsonObj["lightPrimColorTint"] = light.primColorTint;
        jsonObj["lightPrimAlphaAttenuation"] = light.primAlphaAttenuation;
        jsonObj["lightEnvColorTint"] = light.envColorTint;
        jsonObj["lightEnvAlphaAttenuation"] = light.envAlphaAttenuation;
        jsonObj["lightScale"] = light.scale;
        jsonObj["interpolationEnabled"] = interpolation.enabled;

        return true;
    }

    // PresetMaterialLibraryInspector

    void PresetMaterialLibraryInspector::inspectLibrary(PresetMaterialLibrary &library, RenderWindow window) {
        // Preset list.
        ImGui::BeginChild("##materialPresets", ImVec2(0, -64));
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

    bool PresetMaterialLibraryInspector::inspectSelection(PresetMaterialLibrary &library, const PresetLightsLibrary &lightsLibrary) {
        // Material inspector.
        if (selectedPresetName.empty()) {
            return false;
        }

        auto presetIt = library.presetMap.find(selectedPresetName);
        if (presetIt == library.presetMap.end()) {
            return false;
        }

        auto &mat = presetIt->second;
        auto &matDesc = mat.description;
        const std::string materialInspectorId = selectedPresetName;

        // This helps prevent widgets that are being edited from applying their changes to
        // other materials when the selection is changed.
        ImGui::PushID(materialInspectorId.c_str());

        auto pushCommon = [](const char *name, int mask, interop::uint &attributes, bool &changed) {
            bool checkboxValue = attributes & mask;
            ImGui::PushID(name);

            changed = ImGui::Checkbox("", &checkboxValue) || changed;
            if (checkboxValue) {
                attributes |= mask;
            }
            else {
                attributes &= ~(mask);
            }

            ImGui::SameLine();

            if (checkboxValue) {
                ImGui::Text("%s", name);
            }
            else {
                ImGui::TextDisabled("%s", name);
            }

            return checkboxValue;
        };

        auto pushFloat = [pushCommon](const char *name, int mask, float *v, interop::uint &attributes, bool &changed, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f) {
            if (pushCommon(name, mask, attributes, changed)) {
                ImGui::SameLine();
                changed = ImGui::DragFloat("##value", v, v_speed, v_min, v_max) || changed;
            }

            ImGui::PopID();
        };

        auto pushVector3 = [pushCommon](const char *name, int mask, interop::float3 *v, interop::uint &attributes, bool &changed, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f) {
            if (pushCommon(name, mask, attributes, changed)) {
                ImGui::SameLine();
                changed = ImGui::DragFloat3("##value", &v->x, v_speed, v_min, v_max) || changed;
            }

            ImGui::PopID();
        };

        auto pushVector4 = [pushCommon](const char *name, int mask, interop::float4 *v, interop::uint &attributes, bool &changed, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f) {
            if (pushCommon(name, mask, attributes, changed)) {
                ImGui::SameLine();
                changed = ImGui::DragFloat4("##value", &v->x, v_speed, v_min, v_max) || changed;
            }

            ImGui::PopID();
        };

        auto pushInt = [pushCommon](const char *name, int mask, int *v, interop::uint &attributes, bool &changed) {
            if (pushCommon(name, mask, attributes, changed)) {
                ImGui::SameLine();
                changed = ImGui::InputInt("##value", v) || changed;
            }

            ImGui::PopID();
        };

        bool materialChanged = false;
        pushFloat("Ignore normal factor", RT64_ATTRIBUTE_IGNORE_NORMAL_FACTOR, &matDesc.ignoreNormalFactor, matDesc.enabledAttributes, materialChanged, 1.0f, 0.0f, 1.0f);
        pushFloat("UV detail scale", RT64_ATTRIBUTE_UV_DETAIL_SCALE, &matDesc.uvDetailScale, matDesc.enabledAttributes, materialChanged, 0.01f, -50.0f, 50.0f);
        pushFloat("Reflection factor", RT64_ATTRIBUTE_REFLECTION_FACTOR, &matDesc.reflectionFactor, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 1.0f);
        pushFloat("Reflection fresnel factor", RT64_ATTRIBUTE_REFLECTION_FRESNEL_FACTOR, &matDesc.reflectionFresnelFactor, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 1.0f);
        pushFloat("Roughness factor", RT64_ATTRIBUTE_ROUGHNESS_FACTOR, &matDesc.roughnessFactor, matDesc.enabledAttributes, materialChanged, 0.001f, 0.0f, 1.0f);
        pushFloat("Refraction factor", RT64_ATTRIBUTE_REFRACTION_FACTOR, &matDesc.refractionFactor, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 2.0f);
        pushFloat("Shadow catcher factor", RT64_ATTRIBUTE_SHADOW_CATCHER_FACTOR, &matDesc.shadowCatcherFactor, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 1.0f);
        pushVector3("Specular color", RT64_ATTRIBUTE_SPECULAR_COLOR, &matDesc.specularColor, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 100.0f);
        pushFloat("Specular exponent", RT64_ATTRIBUTE_SPECULAR_EXPONENT, &matDesc.specularExponent, matDesc.enabledAttributes, materialChanged, 0.1f, 0.0f, 1000.0f);
        pushFloat("Solid alpha multiplier", RT64_ATTRIBUTE_SOLID_ALPHA_MULTIPLIER, &matDesc.solidAlphaMultiplier, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 10.0f);
        pushFloat("Shadow alpha multiplier", RT64_ATTRIBUTE_SHADOW_ALPHA_MULTIPLIER, &matDesc.shadowAlphaMultiplier, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 10.0f);
        pushFloat("Depth order bias", RT64_ATTRIBUTE_DEPTH_ORDER_BIAS, &matDesc.depthOrderBias, matDesc.enabledAttributes, materialChanged, 0.01f, -10000.0f, 10000.0f);
        pushFloat("Depth decal bias", RT64_ATTRIBUTE_DEPTH_DECAL_BIAS, &matDesc.depthDecalBias, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 1000.0f);
        pushFloat("Shadow ray bias", RT64_ATTRIBUTE_SHADOW_RAY_BIAS, &matDesc.shadowRayBias, matDesc.enabledAttributes, materialChanged, 1.0f, 0.0f, 1000.0f);
        pushVector3("Self light", RT64_ATTRIBUTE_SELF_LIGHT, &matDesc.selfLight, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 10.0f);
        pushVector4("Diffuse color mix", RT64_ATTRIBUTE_DIFFUSE_COLOR_MIX, &matDesc.diffuseColorMix, matDesc.enabledAttributes, materialChanged, 0.01f, 0.0f, 1.0f);
        pushInt("Light group mask bits", RT64_ATTRIBUTE_LIGHT_GROUP_MASK_BITS, (int *)(&matDesc.lightGroupMaskBits), matDesc.enabledAttributes, materialChanged);

        ImGui::NewLine();
        if (ImGui::BeginCombo("Light preset", mat.light.presetName.c_str())) {
            if (ImGui::Selectable("Clear", false)) {
                mat.light.presetName = std::string();
                materialChanged = true;
            }

            for (const auto &it : lightsLibrary.presetMap) {
                const bool isSelected = !mat.light.presetName.empty() && (mat.light.presetName == it.first);
                if (ImGui::Selectable(it.first.c_str(), isSelected)) {
                    mat.light.presetName = it.first;
                    materialChanged = true;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (!mat.light.presetName.empty()) {
            ImGui::Text("Light scale");
            ImGui::SameLine();
            materialChanged = ImGui::DragFloat("##lightScale", &mat.light.scale, 0.5f, 0.0f, FLT_MAX) || materialChanged;

            ImGui::Text("Light prim color tint");
            ImGui::SameLine();
            materialChanged = ImGui::DragFloat("##lightPrimColorTint", &mat.light.primColorTint, 0.01f, 0.0f, 1.0f) || materialChanged;

            ImGui::Text("Light prim alpha attenuation");
            ImGui::SameLine();
            materialChanged = ImGui::DragFloat("##lightPrimAlphaAttenuation", &mat.light.primAlphaAttenuation, 0.01f, 0.0f, 1.0f) || materialChanged;

            ImGui::Text("Light env color tint");
            ImGui::SameLine();
            materialChanged = ImGui::DragFloat("##lightEnvColorTint", &mat.light.envColorTint, 0.01f, 0.0f, 1.0f) || materialChanged;

            ImGui::Text("Light env alpha attenuation");
            ImGui::SameLine();
            materialChanged = ImGui::DragFloat("##lightEnvAlphaAttenuation", &mat.light.envAlphaAttenuation, 0.01f, 0.0f, 1.0f) || materialChanged;
        }

        ImGui::Text("Interpolation");
        ImGui::SameLine();
        materialChanged = ImGui::Checkbox("##enabled", &mat.interpolation.enabled) || materialChanged;

        ImGui::PopID();

        return materialChanged;
    }
};