//
// RT64
//

#pragma once

#include <mutex>

#include "rt64_preset.h"

#include "gui/rt64_file_dialog.h"

#include "imgui/imgui.h"

#include "common/rt64_plume.h"

namespace RT64 {
    template <
        class B, 
        class L,
        class = std::enable_if_t<std::is_base_of_v<PresetBase, B>>,
        class = std::enable_if_t<std::is_base_of_v<PresetLibrary<B>, L>>
    >
    struct PresetLibraryInspector {
        const char NewPresetNameModalId[32] = "New preset name";
        const char DeletePresetModalId[32] = "Delete preset";
        std::string selectedPresetName = "";
        std::string deletePresetName = "";
        char newPresetName[256] = "";
        bool renameRequested = false;

        struct FileDialogResults {
            // Hold the results of file dialog functions in a shared pointer that can remain alive even after the
            // inspector is deleted, as the callback can run later on a different thread depending on the platform.
            std::recursive_mutex mutex;
            std::filesystem::path openLibraryPath;
            std::filesystem::path saveLibraryPath;
        };

        std::shared_ptr<FileDialogResults> dialogResults;

        PresetLibraryInspector() {
            dialogResults = std::make_shared<FileDialogResults>();
        }

        bool inspectPresetBegin(L &library, typename std::map<std::string, B>::iterator &presetIt, RenderWindow window) {
            return ImGui::Checkbox("##enabled", &presetIt->second.enabled);
        }

        bool inspectPresetEnd(L &library, typename std::map<std::string, B>::iterator &presetIt, RenderWindow window) {
            bool changed = false;
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const float FixedButtonWidth = 130.0f;
            ImGui::SameLine(windowSize.x - FixedButtonWidth);
            if (ImGui::Button("Rename##preset")) {
                renameRequested = true;
                selectedPresetName = presetIt->first;
                strncpy(newPresetName, selectedPresetName.c_str(), sizeof(newPresetName));
                changed = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete##preset")) {
                ImGui::OpenPopup(DeletePresetModalId);
            }

            if (ImGui::BeginPopupModal(DeletePresetModalId)) {
                ImGui::Text("Are you sure you want to delete this preset?");

                if (ImGui::Button("OK##deletePopup")) {
                    ImGui::CloseCurrentPopup();

                    // Assigning this will delete the preset when the common inspector is called.
                    deletePresetName = presetIt->first;
                    changed = true;
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel##deletePopup")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            return changed;
        }

        bool inspectBottom(L &library, RenderWindow window, const B &newTemplate = B()) {
            std::lock_guard lock(dialogResults->mutex);
            bool changed = false;

            // If a preset was specified to be deleted, erase it.
            if (!deletePresetName.empty()) {
                library.presetMap.erase(deletePresetName);

                if (deletePresetName == selectedPresetName) {
                    selectedPresetName.clear();
                }

                deletePresetName.clear();
                changed = true;
            }

            ImGui::NewLine();
            const bool newPreset = ImGui::Button("New preset");
            ImGui::SameLine();

            const FileFilter jsonFilter("JSON", "json");
            if (ImGui::Button("Load library")) {
                FileDialog::getOpenFilename({ jsonFilter }, [=](const std::filesystem::path &path) {
                    std::lock_guard lock(dialogResults->mutex);
                    dialogResults->openLibraryPath = path;
                });
            }

            ImGui::SameLine();

            if (ImGui::Button("Save library")) {
                FileDialog::getSaveFilename({ jsonFilter }, [=](const std::filesystem::path &path) {
                    std::lock_guard lock(dialogResults->mutex);
                    dialogResults->saveLibraryPath = path;
                });
            }

            if (newPreset) {
                ImGui::OpenPopup(NewPresetNameModalId);
                selectedPresetName = std::string();
                strcpy(newPresetName, "");
            }

            if (renameRequested) {
                ImGui::OpenPopup(NewPresetNameModalId);
                renameRequested = false;
            }

            if (!dialogResults->openLibraryPath.empty()) {
                library.presetMap.clear();
                library.load(dialogResults->openLibraryPath);
                changed = true;
                dialogResults->openLibraryPath.clear();
            }

            if (!dialogResults->saveLibraryPath.empty()) {
                library.save(dialogResults->saveLibraryPath);
                changed = true;
                dialogResults->saveLibraryPath.clear();
            }

            if (ImGui::BeginPopupModal(NewPresetNameModalId)) {
                ImGui::InputText("##newPresetName", newPresetName, sizeof(newPresetName));
                const std::string newPresetNameStd(newPresetName);
                bool emptyName = strlen(newPresetName) == 0;
                bool sameName = strcmp(newPresetName, selectedPresetName.c_str()) == 0;
                bool duplicateName = !sameName && (library.presetMap.find(newPresetNameStd) != library.presetMap.end());
                bool okEnabled = !emptyName && !duplicateName;
                ImGui::BeginDisabled(!okEnabled);
                if (ImGui::Button("OK")) {
                    // Rename an existing preset.
                    if (!sameName && !selectedPresetName.empty()) {
                        auto extractedPreset = library.presetMap.extract(selectedPresetName);
                        extractedPreset.key() = newPresetNameStd;
                        library.presetMap.insert(std::move(extractedPreset));
                        selectedPresetName = newPresetNameStd;
                        changed = true;
                    }
                    // Make a new preset.
                    else if (selectedPresetName.empty()) {
                        library.presetMap[newPresetNameStd] = newTemplate;
                        changed = true;
                    }

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndDisabled();

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }

                if (emptyName) {
                    ImGui::Text("Please enter a valid name.");
                }

                if (duplicateName) {
                    ImGui::Text("That name is already being used.");
                }

                ImGui::EndPopup();
            }

            return changed;
        }
    };
};
