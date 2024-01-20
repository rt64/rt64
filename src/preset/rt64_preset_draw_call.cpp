//
// RT64
//

#include "rt64_preset_draw_call.h"

#include "imgui/imgui.h"
#include "xxHash/xxh3.h"

#include "gui/rt64_file_dialog.h"

namespace RT64 {
    // DrawCallKey

    void to_json(json &j, const DrawCallKey &k) {
        std::vector<uint64_t> tmemHashVector(std::begin(k.tmemHashes), std::end(k.tmemHashes));
        j = {
            { "tmemHashes", tmemHashVector },
            { "combineL", k.colorCombiner.L },
            { "combineH", k.colorCombiner.H },
            { "otherModeL", k.otherMode.L },
            { "otherModeH", k.otherMode.H },
            { "geometryMode", k.geometryMode }
        };
    }

    void from_json(const json &j, DrawCallKey &k) {
        // Get rid of old texture hashes.
        if (j.contains("textureHashes")) {
            memset(k.tmemHashes, 0xFF, sizeof(k.tmemHashes));
        }
        else {
            std::vector<uint64_t> textureHashVector = j.value("tmemHashes", std::vector<uint64_t>{ });
            textureHashVector.resize(8, 0);
            memcpy(k.tmemHashes, textureHashVector.data(), sizeof(k.tmemHashes));
        }

        if (j.contains("combine")) {
            k.colorCombiner.L = j.value("combine", 0ULL) & 0xFFFFFFFFULL;
            k.colorCombiner.H = (j.value("combine", 0ULL) >> 32ULL) & 0xFFFFFFFFULL;
        }
        else {
            k.colorCombiner.L = j.value("combineL", 0UL);
            k.colorCombiner.H = j.value("combineH", 0UL);
        }

        k.otherMode.L = j.value("otherModeL", 0UL);
        k.otherMode.H = j.value("otherModeH", 0UL);
        k.geometryMode = j.value("geometryMode", 0UL);
    }

    uint64_t DrawCallKey::hash() const {
        return XXH3_64bits(this, sizeof(DrawCallKey));
    }

    DrawCallKey DrawCallKey::fromDrawCall(const DrawCall &call, const DrawData &drawData, const TextureCache &textureCache) {
        DrawCallKey key;
        memset(key.tmemHashes, 0, sizeof(key.tmemHashes));
        key.colorCombiner = call.colorCombiner;
        key.otherMode = call.otherMode;
        key.geometryMode = call.geometryMode;
        
        for (uint32_t t = 0; t < call.tileCount; t++) {
            const auto &callTile = drawData.callTiles[call.tileIndex + t];
            const uint64_t tmemHash = callTile.tmemHashOrID;
            if (!callTile.tileCopyUsed && (tmemHash > 0)) {
                key.tmemHashes[t] = tmemHash;
            }
        }

        return key;
    }

    bool DrawCallKey::operator==(const DrawCallKey &k) const {
        return memcmp(this, &k, sizeof(DrawCallKey));
    }

    // DrawCallMask

    void to_json(json &j, const DrawCallMask &m) {
        j = {
            { "otherModeL", m.otherModeL },
            { "otherModeH", m.otherModeH },
            { "geometryMode", m.geometryMode },
            { "attribute", m.attribute }
        };
    }

    void from_json(const json &j, DrawCallMask &m) {
        m.otherModeL = j.value("otherModeL", 0UL);
        m.otherModeH = j.value("otherModeH", 0UL);
        m.geometryMode = j.value("geometryMode", 0UL);
        m.attribute = j.value("attribute", 0ULL);
    }

    DrawCallMask DrawCallMask::defaultAll() {
        DrawCallMask m;
        m.otherModeL = 0xFFFFFFFFU;
        m.otherModeH = 0xFFFFFFFFU;
        m.geometryMode = 0xFFFFFFFFU;
        m.attribute = 0xFFFFFFFFFFFFFFFFULL;
        return m;
    }

    // PresetDrawCall

    bool PresetDrawCall::readJson(const json &jsonObj) {
        if (!PresetBase::readJson(jsonObj)) {
            return false;
        }

        // Backwards compatibility with older design.
        if (jsonObj.contains("filter")) {
            key = jsonObj.value("filter", DrawCallKey());
            mask = DrawCallMask::defaultAll();
            mask.attribute = jsonObj.at("filter").value("enabledAttributes", 0L);
        }
        else {
            key = jsonObj.value("key", DrawCallKey());
            mask = jsonObj.value("mask", DrawCallMask());
        }

        materialPresetName = jsonObj.value("materialPresetName", std::string());

        return true;
    }

    bool PresetDrawCall::writeJson(json &jsonObj) const {
        if (!PresetBase::writeJson(jsonObj)) {
            return false;
        }

        jsonObj["key"] = key;
        jsonObj["mask"] = mask;
        jsonObj["materialPresetName"] = materialPresetName;

        return true;
    }

    bool PresetDrawCall::matches(const DrawCallKey &otherKey) const {
        if (mask.attribute & (1ULL << static_cast<uint32_t>(DrawAttribute::Texture))) {
            auto keyIncludesHashesOf = [](const DrawCallKey &A, const DrawCallKey &B) {
                const int HashCount = int(std::size(A.tmemHashes));
                for (int i = 0; i < HashCount; i++) {
                    if (A.tmemHashes[i] != 0) {
                        bool found = false;
                        for (int j = 0; (j < HashCount) && !found && (B.tmemHashes[j] != 0); j++) {
                            found = (A.tmemHashes[i] == B.tmemHashes[j]);
                        }

                        if (!found) {
                            return false;
                        }
                    }
                }

                return true;
            };

            if (!keyIncludesHashesOf(key, otherKey) || !keyIncludesHashesOf(otherKey, key)) {
                return false;
            }
        }

        if (mask.attribute & (1ULL << static_cast<uint32_t>(DrawAttribute::Combine))) {
            if ((key.colorCombiner.L != otherKey.colorCombiner.L) && (key.colorCombiner.H != otherKey.colorCombiner.H)) {
                return false;
            }
        }

        if (mask.attribute & (1ULL << static_cast<uint32_t>(DrawAttribute::OtherMode))) {
            if ((key.otherMode.L & mask.otherModeL) != (otherKey.otherMode.L & mask.otherModeL)) {
                return false;
            }

            if ((key.otherMode.H & mask.otherModeH) != (otherKey.otherMode.H & mask.otherModeH)) {
                return false;
            }
        }

        if (mask.attribute & (1ULL << static_cast<uint32_t>(DrawAttribute::GeometryMode))) {
            if ((key.geometryMode & mask.geometryMode) != (otherKey.geometryMode & mask.geometryMode)) {
                return false;
            }
        }

        return true;
    }

    // PresetDrawCallLibrary

    void PresetDrawCallLibrary::clearCache() {
        materialCache.clear();
        nameIndexMap.clear();
        cachedKeyMaterialMap.clear();
        cachedKeyMap.clear();
    }

    void PresetDrawCallLibrary::updateMaterialInCache(const std::string &materialName, const PresetMaterial &material) {
        auto indexIt = nameIndexMap.find(materialName);
        if (indexIt != nameIndexMap.end()) {
            materialCache[indexIt->second] = material;
        }
    }

    PresetDrawCallLibrary::KeyIndexMapRange PresetDrawCallLibrary::findMaterialsInCache(const DrawCallKey &drawCallKey, const PresetMaterialLibrary &materialLibrary) {
        // Quick lookup case.
        const uint64_t hash = drawCallKey.hash();
        auto &keyProcessed = cachedKeyMap[hash];
        if (keyProcessed) {
            return cachedKeyMaterialMap.equal_range(hash);
        }

        // Build material matches for the cache.
        keyProcessed = true;

        for (const auto &preset : presetMap) {
            if (!preset.second.enabled || !preset.second.matches(drawCallKey)) {
                continue;
            }

            const std::string &materialName = preset.second.materialPresetName;
            auto presetIt = materialLibrary.presetMap.find(materialName);
            if (presetIt != materialLibrary.presetMap.end()) {
                auto indexIt = nameIndexMap.find(materialName);
                if (indexIt != nameIndexMap.end()) {
                    cachedKeyMaterialMap.insert({ hash, indexIt->second });
                }
                else {
                    const uint32_t materialIndex = static_cast<uint32_t>(materialCache.size());
                    materialCache.push_back(presetIt->second);
                    nameIndexMap[materialName] = materialIndex;
                    cachedKeyMaterialMap.insert({ hash, materialIndex });
                }
            }
        }

        return cachedKeyMaterialMap.equal_range(hash);
    }

    // PresetDrawCallLibraryInspector

    PresetDrawCallLibraryInspector::PresetDrawCallLibraryInspector() {
        newPresetTemplate.enabled = true;
        newPresetTemplate.materialPresetName = std::string();
        newPresetRequested = false;
        scrollToSelection = false;
    }

    void PresetDrawCallLibraryInspector::promptForNew(const DrawCallKey &key) {
        newPresetTemplate.key = key;
        newPresetTemplate.mask = DrawCallMask::defaultAll();
        newPresetRequested = true;

        ImGui::SetWindowCollapsed(WindowName, false);
        ImGui::SetWindowFocus(WindowName);
    }

    void PresetDrawCallLibraryInspector::highlightDrawCallIngame(const std::string &presetName, PresetDrawCallLibrary &library, const TextureCache &textureCache) {
        /* TODO
        DrawCallFilter callFilter;
        for (uint32_t f = 0; f < gameFrame.fbPairCount; f++) {
            auto &fbPair = gameFrame.fbPairs[f];
            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                auto &proj = fbPair.projections[p];
                for (uint32_t d = 0; d < proj.drawCallCount; d++) {
                    callFilter = DrawCallFilter::fromDrawCall(proj.drawCalls[d].callDesc, textureCache);
                    const auto callPresets = library.findCallPresets(callFilter);
                    if (callPresets.find(presetName) != callPresets.end()) {
                        auto &drawCall = proj.drawCalls[d];
                        auto &debuggerDesc = drawCall.debuggerDesc;
                        debuggerDesc.highlightEnabled = true;
                    }
                }
            }
        }
        */
    }

    void PresetDrawCallLibraryInspector::inspectLibrary(PresetDrawCallLibrary &library, RenderWindow window, const TextureCache &textureCache) {
        bool clearMaterialCache = false;

        // Preset list.
        ImGui::BeginChild("##drawCallPresets", ImVec2(0, -64));
        auto presetIt = library.presetMap.begin();
        while (presetIt != library.presetMap.end()) {
            ImGui::PushID(presetIt->first.c_str());

            if (inspectPresetBegin(library, presetIt, window)) {
                clearMaterialCache = true;
            }

            ImGui::SameLine();

            bool selected = (presetIt->first == selectedPresetName);
            if (ImGui::Selectable(presetIt->first.c_str(), selected, ImGuiSelectableFlags_AllowItemOverlap)) {
                selectedPresetName = presetIt->first;
            }

            if (scrollToSelection && selected) {
                ImGui::SetScrollHereY(0.0f);
                scrollToSelection = false;
            }

            if (ImGui::IsItemHovered()) {
                highlightDrawCallIngame(presetIt->first, library, textureCache);
            }

            if (inspectPresetEnd(library, presetIt, window)) {
                clearMaterialCache = true;
            }

            ImGui::PopID();

            presetIt++;
        }

        ImGui::EndChild();

        // A new preset was requested externally.
        if (newPresetRequested) {
            ImGui::OpenPopup(NewPresetNameModalId);
            selectedPresetName = std::string();
            strcpy(newPresetName, "");
            newPresetRequested = false;
        }

        if (inspectBottom(library, window, newPresetTemplate)) {
            newPresetTemplate.key = DrawCallKey();
            clearMaterialCache = true;
        }

        if (clearMaterialCache) {
            library.clearCache();
        }
    }

    void PresetDrawCallLibraryInspector::inspectSelection(PresetDrawCallLibrary &library, const PresetMaterialLibrary &materialLibrary) {
        // Material inspector.
        if (selectedPresetName.empty()) {
            return;
        }

        auto presetIt = library.presetMap.find(selectedPresetName);
        if (presetIt == library.presetMap.end()) {
            return;
        }

        // This helps prevent widgets that are being edited from applying their changes to
        // other draw calls when the selection is changed.
        const std::string drawCallInspectorId = selectedPresetName;
        ImGui::PushID(drawCallInspectorId.c_str());

        bool clearMaterialCache = false;
        auto checkboxAttribute = [&clearMaterialCache](const char *name, uint64_t mask, uint64_t *attributes) {
            ImGui::PushID(name);

            bool checkboxValue = *attributes & mask;
            if (ImGui::Checkbox("##Checkbox", &checkboxValue)) {
                clearMaterialCache = true;
            }

            if (checkboxValue) {
                *attributes |= mask;
            }
            else {
                *attributes &= ~(mask);
            }

            ImGui::SameLine();

            if (checkboxValue) {
                ImGui::Text("%s", name);
            }
            else {
                ImGui::TextDisabled("%s", name);
            }

            ImGui::PopID();

            return checkboxValue;
        };

        auto &drawCall = presetIt->second;
        auto &key = drawCall.key;
        auto &mask = drawCall.mask;
        if (checkboxAttribute("Texture hashes", 1ULL << static_cast<uint32_t>(DrawAttribute::Texture), &mask.attribute)) {
            ImGui::Indent();
            for (int i = 0; i < 2; i++) {
                ImGui::PushID(i);
                if (ImGui::DragScalar("Hash", ImGuiDataType_U64, &key.tmemHashes[i])) {
                    clearMaterialCache = true;
                }

                ImGui::PopID();
            }

            ImGui::Unindent();
        }

        if (checkboxAttribute("Combine", 1ULL << static_cast<uint32_t>(DrawAttribute::Combine), &mask.attribute)) {
            ImGui::SameLine();
            if (ImGui::DragScalar("##colorCombinerL", ImGuiDataType_U32, &key.colorCombiner.L)) {
                clearMaterialCache = true;
            }

            ImGui::SameLine();
            if (ImGui::DragScalar("##colorCombinerH", ImGuiDataType_U32, &key.colorCombiner.H)) {
                clearMaterialCache = true;
            }
        }

        if (checkboxAttribute("Other mode", 1ULL << static_cast<uint32_t>(DrawAttribute::OtherMode), &mask.attribute)) {
            ImGui::SameLine();
            if (ImGui::DragScalar("##otherModeL", ImGuiDataType_U32, &key.otherMode.L)) {
                clearMaterialCache = true;
            }

            ImGui::SameLine();
            if (ImGui::DragScalar("##otherModeH", ImGuiDataType_U32, &key.otherMode.H)) {
                clearMaterialCache = true;
            }
        }

        if (checkboxAttribute("Geometry mode", 1ULL << static_cast<uint32_t>(DrawAttribute::GeometryMode), &mask.attribute)) {
            ImGui::SameLine();
            if (ImGui::DragScalar("##geometryMode", ImGuiDataType_U32, &key.geometryMode)) {
                clearMaterialCache = true;
            }
        }

        if (ImGui::BeginCombo("Material preset", drawCall.materialPresetName.c_str())) {
            if (ImGui::Selectable("Clear", false)) {
                drawCall.materialPresetName = std::string();
            }

            for (const auto &it : materialLibrary.presetMap) {
                const bool isSelected = !drawCall.materialPresetName.empty() && (drawCall.materialPresetName == it.first);
                if (ImGui::Selectable(it.first.c_str(), isSelected)) {
                    drawCall.materialPresetName = it.first;
                    clearMaterialCache = true;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        // Search for duplicate calls and show a list of them if any are found.
        // Selecting a duplicate will select it on the main list.
        bool firstFind = true;
        for (auto searchIt = library.presetMap.begin(); searchIt != library.presetMap.end(); searchIt++) {
            if (searchIt == presetIt) {
                continue;
            }

            if (searchIt->second.key == presetIt->second.key) {
                if (firstFind) {
                    ImGui::NewLine();
                    ImGui::Text("Duplicate calls:");
                    ImGui::Indent();
                    firstFind = false;
                }

                if (ImGui::Selectable(searchIt->first.c_str())) {
                    selectedPresetName = searchIt->first;
                    scrollToSelection = true;
                    break;
                }
            }
        }

        if (!firstFind) {
            ImGui::Unindent();
        }

        ImGui::PopID();

        if (clearMaterialCache) {
            library.clearCache();
        }
    }
};