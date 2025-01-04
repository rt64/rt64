//
// RT64
//

#include "rt64_debugger_inspector.h"

#include <string>
#include <cinttypes>

#include "imgui/imgui.h"
#include "xxHash/xxh3.h"

#include "common/rt64_common.h"
#include "common/rt64_math.h"
#include "common/rt64_tmem_hasher.h"
#include "hle/rt64_color_converter.h"
#include "hle/rt64_vi.h"
#include "gbi/rt64_f3d.h"
#include "render/rt64_raster_shader.h"
#include "shared/rt64_blender.h"

namespace RT64 {
    // DebuggerInspector

    static const uint32_t MatrixGroupIgnoredColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(0.25f, 0.25f, 0.25f, 0.5f));
    static const hlslpp::float4 MatrixGroupAutoColorBase = hlslpp::float4(0.0f, 0.0f, 0.25f, 0.5f);
    static const hlslpp::float4 MatrixGroupAssignedColorBase = hlslpp::float4(0.25f, 0.0f, 0.0f, 0.5f);
    static const uint32_t NativeSamplerNoneColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(0.75f, 0.25f, 0.25f, 0.5f));
    static const uint32_t NativeSamplerAnyColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(0.25f, 0.25f, 0.75f, 0.5f));
    static const uint32_t NativeSamplerAllColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(0.25f, 0.75f, 0.25f, 0.5f));
    static const uint32_t NativeSamplerIgnoredColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(0.25f, 0.25f, 0.25f, 0.5f));
    static const uint32_t HoverHighlightColor = ColorConverter::RGBA32::toRGBA(hlslpp::float4(1.0f, 0.0f, 1.0f, 0.5f));

    bool operator<(const DebuggerInspector::CallIndices &lhs, const DebuggerInspector::CallIndices &rhs) {
        if (lhs.fbPairIndex != rhs.fbPairIndex) {
            return lhs.fbPairIndex < rhs.fbPairIndex;
        }

        if (lhs.projectionIndex != rhs.projectionIndex) {
            return lhs.projectionIndex < rhs.projectionIndex;
        }

        return lhs.drawCallIndex < rhs.drawCallIndex;
    }

    DebuggerInspector::DebuggerInspector() {
        openCallIndices = { 0, 0, 0 };
        highightCallIndices = { 0, 0, 0 };
        openLoadIndex = -1;
        openTileIndex = -1;
        openCall = false;
        paused = false;
        renderer.framebufferAddress = 0;
        renderer.framebufferIndex = -1;
        renderer.framebufferDepth = false;
        renderer.globalDrawCallIndex = -1;
        renderer.interpolationWeight = 1.0f;
        camera.enabled = false;
        camera.sceneIndex = 0;
        camera.viewMatrix = hlslpp::float4x4::identity();
        camera.nearPlane = 1.0f;
        camera.farPlane = 1000.0f;
        camera.fov = 0.75f;
        viewTransformGroups = false;
        viewNativeSamplers = false;
    }

    std::string DebuggerInspector::framebufferPairName(const Workload &workload, uint32_t fbPairIndex) {
        assert(fbPairIndex < workload.fbPairCount);
        return "Framebuffers #" + std::to_string(fbPairIndex);
    }

    std::string DebuggerInspector::projectionName(const Workload &workload, uint32_t fbPairIndex, uint32_t projectionIndex) {
        assert(fbPairIndex < workload.fbPairCount);
        assert(projectionIndex < workload.fbPairs[fbPairIndex].projectionCount);
        const auto &proj = workload.fbPairs[fbPairIndex].projections[projectionIndex];
        std::string projectionPrefix;
        switch (proj.type) {
        case Projection::Type::Perspective:
            projectionPrefix = "Perspective";
            break;
        case Projection::Type::Orthographic:
            projectionPrefix = "Orthographic";
            break;
        case Projection::Type::Rectangle:
            projectionPrefix = "Rectangle";
            break;
        case Projection::Type::Triangle:
            projectionPrefix = "Triangle";
            break;
        default:
            projectionPrefix = "Unknown";
            break;
        }

        return projectionPrefix + " #" + std::to_string(projectionIndex);
    }

    void DebuggerInspector::highlightDrawCall(Workload &workload, CallIndices call) {
        assert(call.fbPairIndex < workload.fbPairCount);
        auto &fbPair = workload.fbPairs[call.fbPairIndex];

        assert(call.projectionIndex < fbPair.projectionCount);
        auto &proj = fbPair.projections[call.projectionIndex];

        assert(call.drawCallIndex < proj.gameCallCount);
        auto &drawCall = proj.gameCalls[call.drawCallIndex];
        auto &debuggerDesc = drawCall.debuggerDesc;
        debuggerDesc.highlightColor = HoverHighlightColor;
    }
    
    bool DebuggerInspector::checkPopup(Workload &workload) {
        highightCallIndices = { 0, 0, 0 };

        if (ImGui::BeginPopup("##popupCallindices")) {
            for (CallIndices call : popupCalls) {
                if (call.fbPairIndex >= workload.fbPairCount) {
                    continue;
                }

                const auto &fbPair = workload.fbPairs[call.fbPairIndex];
                if (call.projectionIndex >= fbPair.projectionCount) {
                    continue;
                }

                const auto &proj = fbPair.projections[call.projectionIndex];
                if (call.drawCallIndex >= proj.gameCallCount) {
                    continue;
                }

                const std::string callName = 
                    framebufferPairName(workload, call.fbPairIndex) + " - " +
                    projectionName(workload, call.fbPairIndex, call.projectionIndex) + " - " +
                    "Call #" + std::to_string(call.drawCallIndex);

                bool selected = ImGui::Selectable(callName.c_str());
                if (ImGui::IsItemHovered()) {
                    highlightDrawCall(workload, call);
                    highightCallIndices = call;
                }

                if (selected) {
                    openCallIndices = call;
                    openLoadIndex = -1;
                    openTileIndex = -1;
                    openCall = true;
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }

            ImGui::EndPopup();
        }
        else {
            popupCalls.clear();
        }

        return openCall;
    }
    
    void DebuggerInspector::inspect(RenderWorker *directWorker, const VI &vi, Workload &workload, FramebufferManager &fbManager, TextureCache &textureCache, DrawCallKey &outDrawCallKey, bool &outCreateDrawCallKey, RenderWindow window) {
        const char ReplaceErrorModalId[] = "Replace error";
        const char ReplaceOutdatedModalId[] = "Replace outdated";
        const char ReplaceDirectoryOnlyModalId[] = "Replace directory only";

        auto textTransformGroup = [](const char *label, const TransformGroup &group) {
            ImGui::Text("%s", label);
            ImGui::Indent();
            ImGui::Text("%u (POS %u ROT %u SCA %u ORDER %u)", group.matrixId, group.positionInterpolation, group.rotationInterpolation, group.scaleInterpolation, group.ordering);
            ImGui::Unindent();
        };

        auto textMatrix = [](const char *label, const interop::float4x4 &m) {
            ImGui::Text("%s", label);
            ImGui::Indent();
            for (int i = 0; i < 4; i++) {
                ImGui::Text("%f %f %f %f", m[i][0], m[i][1], m[i][2], m[i][3]);
            }

            ImGui::Unindent();
        };

        auto textAddressing = [](const char *label, uint8_t cm) {
            ImGui::Text("%s", label);
            ImGui::SameLine();

            switch (cm) {
            case G_TX_WRAP:
                ImGui::Text("G_TX_WRAP");
                break;
            case G_TX_MIRROR:
                ImGui::Text("G_TX_MIRROR");
                break;
            case G_TX_CLAMP:
                ImGui::Text("G_TX_CLAMP");
                break;
            case G_TX_MIRROR | G_TX_CLAMP:
                ImGui::Text("G_TX_MIRROR | G_TX_CLAMP");
                break;
            default:
                ImGui::Text("Unknown (%d)", cm);
                break;
            }
        };

        auto textPixelSize = [](const char *label, uint8_t siz) {
            ImGui::Text("%s", label);
            ImGui::SameLine();

            switch (siz) {
            case G_IM_SIZ_4b:
                ImGui::Text("G_IM_SIZ_4b");
                break;
            case G_IM_SIZ_8b:
                ImGui::Text("G_IM_SIZ_8b");
                break;
            case G_IM_SIZ_16b:
                ImGui::Text("G_IM_SIZ_16b");
                break;
            case G_IM_SIZ_32b:
                ImGui::Text("G_IM_SIZ_32b");
                break;
            default:
                ImGui::Text("Unknown (%d)", siz);
                break;
            }
        };

        auto textPixelFormat = [](const char *label, uint8_t fmt) {
            ImGui::Text("%s", label);
            ImGui::SameLine();
            switch (fmt) {
            case G_IM_FMT_RGBA:
                ImGui::Text("G_IM_FMT_RGBA");
                break;
            case G_IM_FMT_CI:
                ImGui::Text("G_IM_FMT_CI");
                break;
            case G_IM_FMT_IA:
                ImGui::Text("G_IM_FMT_IA");
                break;
            case G_IM_FMT_I:
                ImGui::Text("G_IM_FMT_I");
                break;
            default:
                ImGui::Text("Unknown (%d)", fmt);
                break;
            }
        };

        auto showHashAndReplaceButton = [&](uint64_t replacementHash) {
            char hexStr[64];
            snprintf(hexStr, sizeof(hexStr), "%016" PRIx64, replacementHash);
            ImGui::Text("Hash 0x%s", hexStr);
            ImGui::SameLine();

            if (ImGui::Button("Replace")) {
                if (!textureCache.textureMap.replacementMap.fileSystemIsDirectory) {
                    ImGui::OpenPopup(ReplaceDirectoryOnlyModalId);
                }
                else if (textureCache.textureMap.replacementMap.directoryDatabase.config.hashVersion < TMEMHasher::CurrentHashVersion) {
                    ImGui::OpenPopup(ReplaceOutdatedModalId);
                }
                else {
                    std::filesystem::path textureFilename = FileDialog::getOpenFilename({ FileFilter("Image Files", "dds,png") });
                    if (!textureFilename.empty()) {
                        std::filesystem::path directoryPath = textureCache.textureMap.replacementMap.replacementDirectories.front().dirOrZipPath;
                        std::filesystem::path relativePath = std::filesystem::relative(textureFilename, directoryPath);
                        if (!relativePath.empty()) {
                            textureCache.addReplacement(replacementHash, relativePath.u8string());
                        }
                        else {
                            ImGui::OpenPopup(ReplaceErrorModalId);
                        }
                    }
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Copy hash")) {
                ImGui::SetClipboardText(hexStr);
            }

            if (ImGui::BeginPopupModal(ReplaceErrorModalId)) {
                ImGui::Text("The texture must be relative to the current texture pack's directory.");

                if (ImGui::Button("Close##replaceError")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(ReplaceOutdatedModalId)) {
                ImGui::Text("The texture database must be upgraded before being able to do manual replacements (use the texture hasher tool).");

                if (ImGui::Button("Close##replaceOutdated")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(ReplaceDirectoryOnlyModalId)) {
                ImGui::Text("Textures can only be replaced when loading a texture pack as a single directory.");

                if (ImGui::Button("Close##replaceDirectoryOnly")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        };

        outCreateDrawCallKey = false;

        ImGui::BeginChild("##debugger");

        if (ImGui::Button(paused ? "Resume (F4)" : "Pause (F4)") || ImGui::IsKeyPressed(ImGuiKey_F4)) {
            paused = !paused;
        }

        const int32_t maxFbIndex = (workload.fbPairCount - 1);
        ImGui::SliderInt("View Framebuffer", &renderer.framebufferIndex, -1, maxFbIndex);
        renderer.framebufferIndex = std::min(renderer.framebufferIndex, maxFbIndex);
        ImGui::SameLine();
        ImGui::BeginDisabled(renderer.framebufferIndex < 0);
        ImGui::Checkbox("View Depth Buffer", &renderer.framebufferDepth);
        ImGui::EndDisabled();
        ImGui::SliderInt("View Draw Call", &renderer.globalDrawCallIndex, -1, workload.gameCallCount);
        renderer.globalDrawCallIndex = std::min(renderer.globalDrawCallIndex, int32_t(workload.gameCallCount));
        ImGui::SliderFloat("Interpolation Weight", &renderer.interpolationWeight, 0.0f, 1.0f);
        ImGui::Checkbox("View Transform Groups", &viewTransformGroups);
        ImGui::Checkbox("View Native Samplers", &viewNativeSamplers);
        ImGui::NewLine();

        uint32_t totalCallCount = 0;
        uint32_t totalTriCount = 0;
        uint32_t totalFbPairCount = workload.fbPairCount;
        uint32_t totalSyncCount = 0;
        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            auto &fbPair = workload.fbPairs[f];
            if ((uint32_t)renderer.framebufferIndex == f) {
                renderer.framebufferAddress = renderer.framebufferDepth ? fbPair.depthImage.address : fbPair.colorImage.address;
            }

            const bool lastFbPair = (f == (workload.fbPairCount - 1));
            if (fbPair.syncRequired || lastFbPair) {
                totalSyncCount++;
            }

            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                auto &proj = fbPair.projections[p];
                bool usesTransforms = (proj.type == Projection::Type::Perspective) || (proj.type == Projection::Type::Orthographic);
                totalCallCount += proj.gameCallCount;
                for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                    totalTriCount += proj.gameCalls[d].callDesc.triangleCount;

                    // Do not modify the highlight of this draw call if it was highlighted by the popup menu before.
                    if ((highightCallIndices.fbPairIndex == f) && (highightCallIndices.projectionIndex == p) && (highightCallIndices.drawCallIndex == d)) {
                        continue;
                    }
                    
                    if (viewNativeSamplers) {
                        if (proj.gameCalls[d].callDesc.tileCount > 0) {
                            uint32_t nativeSamplerCount = 0;
                            for (uint32_t t = 0; t < proj.gameCalls[d].callDesc.tileCount; t++) {
                                const interop::RDPTile &rdpTile = workload.drawData.rdpTiles[proj.gameCalls[d].callDesc.tileIndex + t];
                                if (rdpTile.nativeSampler != NATIVE_SAMPLER_NONE) {
                                    nativeSamplerCount++;
                                }
                            }

                            if (nativeSamplerCount == proj.gameCalls[d].callDesc.tileCount) {
                                proj.gameCalls[d].debuggerDesc.highlightColor = NativeSamplerAllColor;
                            }
                            else if (nativeSamplerCount == 0) {
                                proj.gameCalls[d].debuggerDesc.highlightColor = NativeSamplerNoneColor;
                            }
                            else {
                                proj.gameCalls[d].debuggerDesc.highlightColor = NativeSamplerAnyColor;
                            }
                        }
                        else {
                            proj.gameCalls[d].debuggerDesc.highlightColor = NativeSamplerIgnoredColor;
                        }
                    }
                    else if (viewTransformGroups && usesTransforms) {
                        uint32_t matrixIndex = proj.gameCalls[d].callDesc.minWorldMatrix;
                        uint32_t groupIndex = workload.drawData.worldTransformGroups[matrixIndex];
                        uint32_t matrixId = workload.drawData.transformGroups[groupIndex].matrixId;
                        if (matrixId == G_EX_ID_IGNORE) {
                            proj.gameCalls[d].debuggerDesc.highlightColor = MatrixGroupIgnoredColor;
                        }
                        else if (matrixId == G_EX_ID_AUTO) {
                            hlslpp::float4 colorOffset(0, pseudoRandom(matrixIndex) * 0.25f, pseudoRandom(matrixIndex) * 0.75f, 0.0f);
                            proj.gameCalls[d].debuggerDesc.highlightColor = ColorConverter::RGBA32::toRGBA(MatrixGroupAutoColorBase + colorOffset);
                        }
                        else {
                            hlslpp::float4 colorOffset(pseudoRandom(matrixId) * 0.75f, pseudoRandom(matrixId) * 0.25f, 0.0f, 0.0f);
                            proj.gameCalls[d].debuggerDesc.highlightColor = ColorConverter::RGBA32::toRGBA(MatrixGroupAssignedColorBase + colorOffset);
                        }
                    }
                    else {
                        proj.gameCalls[d].debuggerDesc.highlightColor = 0;
                    }
                }
            }
        }
        
        ImGui::Text("Frame stats");
        ImGui::Text("Draw calls: %u", totalCallCount);
        ImGui::Text("Triangles: %u", totalTriCount);
        ImGui::Text("Framebuffer Pairs: %u", totalFbPairCount);
        ImGui::Text("RDRAM Synchronizations: %u", totalSyncCount);
        ImGui::NewLine();

        bool copyFreeCam = false;
        if (ImGui::Checkbox("Free Camera Enabled", &camera.enabled)) {
            copyFreeCam = camera.enabled;
        }

        /*
        ImGui::BeginDisabled(!freeCamEnabled);
        const std::string freeCamName = "Scene #" + std::to_string(freeCamIndex);
        const size_t sceneCount = workload.perspectiveScenes.size();
        if (ImGui::BeginCombo("Free Camera Scene", freeCamName.c_str())) {
            for (size_t s = 0; s < sceneCount; s++) {
                const std::string sceneName = "Scene #" + std::to_string(s);
                bool sceneSelected = ImGui::Selectable(sceneName.c_str(), (freeCamIndex == s));
                if (ImGui::IsItemHovered()) {
                    const auto &scene = workload.perspectiveScenes[s];
                    for (const auto &sceneProj : scene.projections) {
                        auto &gameProj = workload.fbPairs[sceneProj.fbPairIndex].projections[sceneProj.projectionIndex];
                        for (uint32_t d = 0; d < gameProj.gameCallCount; d++) {
                            auto &drawCall = gameProj.gameCalls[d];
                            drawCall.debuggerDesc.highlightEnabled = true;
                        }
                    }
                }

                if (sceneSelected) {
                    freeCamIndex = int32_t(s);
                    copyFreeCam = true;
                }
            }

            ImGui::EndCombo();
        }

        ImGui::EndDisabled();
        */
        if (copyFreeCam) {
            // TODO: Defaults to first perspective projection for now.
            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                for (uint32_t p = 0; p < workload.fbPairs[f].projectionCount; p++) {
                    if (workload.fbPairs[f].projections[p].type == Projection::Type::Perspective) {
                        enableFreeCamera(workload, f, p);
                        break;
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader("VI")) {
            ImGui::Indent();
            ImGui::Text("Status:");
            ImGui::Indent();
            ImGui::Text("Type:");
            ImGui::SameLine();
            switch (vi.status.type) {
            case VI_STATUS_TYPE_BLANK:
                ImGui::Text("BLANK");
                break;
            case VI_STATUS_TYPE_RESERVED:
                ImGui::Text("RESERVED");
                break;
            case VI_STATUS_TYPE_16_BIT:
                ImGui::Text("16_BIT");
                break;
            case VI_STATUS_TYPE_32_BIT:
                ImGui::Text("TYPE_32_BIT");
                break;
            default:
                ImGui::Text("Unknown");
                break;
            }

            ImGui::Text("Gamma Dither Enable: %u", vi.status.gammaDitherEnable);
            ImGui::Text("Gamma Correction Enable: %u", vi.status.gammaEnable);
            ImGui::Text("Divot Enable: %u", vi.status.divotEnable);
            ImGui::Text("V Bus Clock Enable: %u", vi.status.vbusClockEnable);
            ImGui::Text("Serrate: %u", vi.status.serrate);
            ImGui::Text("Test Mode: %u", vi.status.testMode);
            ImGui::Text("Anti-Aliasing Mode:");
            ImGui::SameLine();
            switch (vi.status.aaMode) {
            case VI_STATUS_AA_MODE_RESAMP_ALWAYS_FETCH:
                ImGui::Text("RESAMP_ALWAYS_FETCH");
                break;
            case VI_STATUS_AA_MODE_RESAMP_FETCH_IF_NEEDED:
                ImGui::Text("RESAMP_FETCH_IF_NEEDED");
                break;
            case VI_STATUS_AA_MODE_RESAMP_ONLY:
                ImGui::Text("RESAMP_ONLY");
                break;
            case VI_STATUS_AA_MODE_NONE:
                ImGui::Text("Unknown");
                break;
            }

            ImGui::Text("Reserved: %u", vi.status.reserved);
            ImGui::Text("Diagnostics: %u", vi.status.diagnostics);
            ImGui::Text("Pixel Advance: %u", vi.status.pixelAdvance);
            ImGui::Text("Dither Filter: %u", vi.status.ditherFilter);
            ImGui::Unindent();

            ImGui::Text("Origin: %u / 0x%X", vi.origin, vi.origin);
            ImGui::Text("Width: %u", vi.width);
            ImGui::Text("Intr: %u", vi.intr);
            ImGui::Text("V Current Line: %u", vi.vCurrentLine);
            ImGui::Text("Burst:");
            ImGui::Indent();
            ImGui::Text("H Sync Width: %u", vi.burst.hSyncWidth);
            ImGui::Text("Color Width: %u", vi.burst.colorWidth);
            ImGui::Text("V Sync Width: %u", vi.burst.vSyncWidth);
            ImGui::Text("Color Start: %u", vi.burst.colorStart);
            ImGui::Unindent();

            ImGui::Text("V Sync: %u", vi.vSync);

            ImGui::Text("H Sync:");
            ImGui::Indent();
            ImGui::Text("H Sync: %u", vi.hSync.hSync);
            ImGui::Text("Leap: %u", vi.hSync.leap);
            ImGui::Unindent();

            ImGui::Text("Leap:");
            ImGui::Indent();
            ImGui::Text("Leap A: %u", vi.leap.leapA);
            ImGui::Text("Leap B: %u", vi.leap.leapB);
            ImGui::Unindent();

            ImGui::Text("H Region:");
            ImGui::Indent();
            ImGui::Text("H Start: %u", vi.hRegion.hStart);
            ImGui::Text("H End: %u", vi.hRegion.hEnd);
            ImGui::Unindent();

            ImGui::Text("V Region:");
            ImGui::Indent();
            ImGui::Text("V Start: %u", vi.vRegion.vStart);
            ImGui::Text("V End: %u", vi.vRegion.vEnd);
            ImGui::Unindent();

            ImGui::Text("V Burst:");
            ImGui::Indent();
            ImGui::Text("H Sync Width: %u", vi.vBurst.hSyncWidth);
            ImGui::Text("Color Width: %u", vi.vBurst.colorWidth);
            ImGui::Text("V Sync Width: %u", vi.vBurst.vSyncWidth);
            ImGui::Text("Color Start: %u", vi.vBurst.colorStart);
            ImGui::Unindent();

            ImGui::Text("X Transform:");
            ImGui::Indent();
            ImGui::Text("X Scale: %f", vi.xScaleFloat());
            ImGui::Text("X Offset: %f", vi.xOffsetFloat());
            ImGui::Unindent();

            ImGui::Text("Y Transform:");
            ImGui::Indent();
            ImGui::Text("Y Scale: %f", vi.yScaleFloat());
            ImGui::Text("Y Offset: %f", vi.yOffsetFloat());
            ImGui::Unindent();

            ImGui::Unindent();
        }
        
        const auto &commandWarnings = workload.commandWarnings;
        const size_t warningCount = commandWarnings.size();
        if ((warningCount > 0) && ImGui::CollapsingHeader("Warnings")) {
            ImGui::Indent();
            for (size_t w = 0; w < warningCount; w++) {
                const CommandWarning &warn = commandWarnings[w];
                ImGui::PushID(int(w));
                if (ImGui::Selectable(warn.message.c_str())) {
                    switch (warn.indexType) {
                    case CommandWarning::IndexType::CallIndex:
                        openCallIndices = { warn.call.fbPairIndex, warn.call.projIndex, warn.call.callIndex };
                        openLoadIndex = -1;
                        openTileIndex = -1;
                        openCall = true;
                        break;
                    case CommandWarning::IndexType::LoadIndex:
                        // Search for the call with the load index.
                        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                            const FramebufferPair &fbPair = workload.fbPairs[f];
                            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                                const Projection &proj = fbPair.projections[p];
                                for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                                    const GameCall &call = proj.gameCalls[d];
                                    const auto &desc = call.callDesc;
                                    if (warn.load.index >= desc.loadIndex && warn.load.index < (desc.loadIndex + desc.loadCount)) {
                                        openLoadIndex = warn.load.index;
                                        openCallIndices = { f, p, d };
                                        openCall = true;
                                    }
                                }
                            }
                        }

                        break;
                    case CommandWarning::IndexType::TileIndex:
                        // Search for the call with the tile index.
                        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                            const FramebufferPair &fbPair = workload.fbPairs[f];
                            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                                const Projection &proj = fbPair.projections[p];
                                for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                                    const GameCall &call = proj.gameCalls[d];
                                    const auto &desc = call.callDesc;
                                    if (warn.tile.index >= desc.tileIndex && warn.tile.index < (desc.tileIndex + desc.tileCount)) {
                                        openTileIndex = warn.tile.index;
                                        openCallIndices = { f, p, d };
                                        openCall = true;
                                    }
                                }
                            }
                        }

                        break;
                    default:
                        assert(false && "Unknown index type.");
                        break;
                    }
                }

                ImGui::PopID();
            }

            ImGui::Unindent();
        }

        auto highlightSpriteCommand = [&](const SpriteCommand &spriteCommand) {
            for (uint32_t c = 0; c < spriteCommand.callCount; c++) {
                highlightDrawCall(workload, { spriteCommand.fbPairIndex, spriteCommand.projIndex, spriteCommand.callIndex + c });
            }
        };

        const auto &spriteCommands = workload.spriteCommands;
        const size_t spriteCount = spriteCommands.size();
        if (spriteCount > 0) {
            bool spriteCommandsOpen = ImGui::CollapsingHeader("S2D Commands");
            if (ImGui::IsItemHovered()) {
                for (size_t s = 0; s < spriteCount; s++) {
                    const SpriteCommand &spriteCommand = spriteCommands[s];
                    highlightSpriteCommand(spriteCommand);
                }
            }

            if (spriteCommandsOpen) {
                ImGui::Indent();
                for (size_t s = 0; s < spriteCount; s++) {
                    const SpriteCommand &spriteCommand = spriteCommands[s];
                    ImGui::PushID(int(s));
                    const std::string commandName = "Command #" + std::to_string(s);
                    bool headerOpen = ImGui::CollapsingHeader(commandName.c_str());
                    if (ImGui::IsItemHovered()) {
                        highlightSpriteCommand(spriteCommand);
                    }

                    if (headerOpen) {
                        showHashAndReplaceButton(spriteCommand.replacementHash);
                    }

                    ImGui::PopID();
                }

                ImGui::Unindent();
            }
        }

        const auto &drawData = workload.drawData;
        const auto &posShorts = drawData.posShorts;
        const auto &tcFloats = drawData.tcFloats;
        const auto &normColBytes = drawData.normColBytes;
        const auto &viewProjIndices = drawData.viewProjIndices;
        const auto &worldIndices = drawData.worldIndices;
        const auto &fogIndices = drawData.fogIndices;
        const auto &lightIndices = drawData.lightIndices;
        const auto &lightCounts = drawData.lightCounts;
        const auto &lookAtIndices = drawData.lookAtIndices;
        const auto &posTransformed = drawData.posTransformed;
        const auto &posScreen = drawData.posScreen;
        const auto &faceIndices = drawData.faceIndices;
        const auto &triPosFloats = drawData.triPosFloats;
        const auto &triTcFloats = drawData.triTcFloats;
        const auto &triColorFloats = drawData.triColorFloats;
        const size_t fogCount = drawData.rspFog.size();
        if ((fogCount > 0) && ImGui::CollapsingHeader("Fog")) {
            ImGui::Indent();
            for (size_t f = 0; f < fogCount; f++) {
                const interop::RSPFog &rspFog = drawData.rspFog[f];
                ImGui::Text("Fog #%zu: Mul %f Offset %f", f, rspFog.mul, rspFog.offset);
            }

            ImGui::Unindent();
        }

        const size_t lightCount = drawData.rspLights.size();
        if ((lightCount > 0) && ImGui::CollapsingHeader("Lights")) {
            ImGui::Indent();
            for (size_t l = 0; l < lightCount; l++) {
                const interop::RSPLight &rspLight = drawData.rspLights[l];
                ImGui::Text("Light #%zu: DirPos %f %f %f COL %f %f %f COLC %f %f %f KC %u KL %u KQ %u", l,
                    rspLight.posDir.x, rspLight.posDir.y, rspLight.posDir.z,
                    rspLight.col.x, rspLight.col.y, rspLight.col.z,
                    rspLight.colc.x, rspLight.colc.y, rspLight.colc.z,
                    rspLight.kc, rspLight.kl, rspLight.kq
                );
            }

            ImGui::Unindent();
        }

        const size_t lookAtCount = drawData.rspLookAt.size();
        if ((lookAtCount > 0) && ImGui::CollapsingHeader("LookAt")) {
            ImGui::Indent();
            for (size_t l = 0; l < lookAtCount; l++) {
                const interop::RSPLookAt &rspLookAt = drawData.rspLookAt[l];
                ImGui::Text("LookAt #%zu: X %f %f %f Y %f %f %f", l,
                    rspLookAt.x.x, rspLookAt.x.y, rspLookAt.x.z,
                    rspLookAt.y.x, rspLookAt.y.y, rspLookAt.y.z);
            }

            ImGui::Unindent();
        }

        const size_t modifyCount = drawData.modifyCount();
        if ((modifyCount > 0) && ImGui::CollapsingHeader("Modify")) {
            ImGui::Indent();
            for (size_t m = 0; m < modifyCount; m++) {
                const bool modifyZ = drawData.modifyPosUints[m * 2] & 0x1;
                const uint32_t modifyIndex = drawData.modifyPosUints[m * 2] >> 1;
                const uint32_t modifyValue = drawData.modifyPosUints[m * 2 + 1];
                ImGui::Text(modifyZ ? "Modify Z" : "Modify XY");
                ImGui::SameLine();
                ImGui::Text("#%u: %u (Raw value)", modifyIndex, modifyValue);
            }

            ImGui::Unindent();
        }

        ImGui::NewLine();
        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            ImGui::PushID(f);

            bool openFbPair = false;
            if (openCall) {
                openFbPair = openCallIndices.fbPairIndex == f;
                ImGui::SetNextItemOpen(openFbPair);
            }
            
            const auto &fbPair = workload.fbPairs[f];
            const std::string fbPairName = framebufferPairName(workload, f);
            bool headerOpen = ImGui::CollapsingHeader(fbPairName.c_str());
            bool highlightFbCalls = ImGui::IsItemHovered();
            if (headerOpen) {
                ImGui::Indent();
                ImGui::Text("Display List Address: 0x%08X", fbPair.displayListAddress);
                ImGui::Text("Display List Counter: %" PRIu64, fbPair.displayListCounter);
                ImGui::Text("Flush Reason:");
                ImGui::SameLine();
                
                switch (fbPair.flushReason) {
                case FramebufferPair::FlushReason::SamplingFromColorImage:
                    ImGui::Text("The next framebuffer pair samples from this color image.");
                    break;
                case FramebufferPair::FlushReason::SamplingFromDepthImage:
                    ImGui::Text("The next framebuffer pair samples from this depth image.");
                    break;
                case FramebufferPair::FlushReason::ColorImageChanged:
                    ImGui::Text("The color image was changed.");
                    break;
                case FramebufferPair::FlushReason::DepthImageChanged:
                    ImGui::Text("The depth image was changed.");
                    break;
                case FramebufferPair::FlushReason::ProcessDisplayListsEnd:
                    ImGui::Text("All display lists were processed.");
                    break;
                case FramebufferPair::FlushReason::None:
                default:
                    ImGui::Text("No valid reason provided.");
                    break;
                }

                ImGui::NewLine();
                ImGui::Text("Color:");
                ImGui::Indent();
                ImGui::Text("Address: %u / 0x%X", fbPair.colorImage.address, fbPair.colorImage.address);
                textPixelFormat("Color format:", fbPair.colorImage.fmt);
                textPixelSize("Pixel size:", fbPair.colorImage.siz);
                ImGui::Text("Width: %u", fbPair.colorImage.width);

                ImGui::Unindent();
                ImGui::Text("Depth:");
                ImGui::Indent();
                ImGui::Text("Address: %u / 0x%X", fbPair.depthImage.address, fbPair.depthImage.address);
                ImGui::Text("Read: %d", fbPair.depthRead);
                ImGui::Text("Write: %d", fbPair.depthWrite);
                ImGui::Unindent();

                ImGui::Text("Draw Color Area: %d %d %d %d", fbPair.drawColorRect.ulx, fbPair.drawColorRect.uly, fbPair.drawColorRect.lrx, fbPair.drawColorRect.lry);
                ImGui::Text("Draw Depth Area: %d %d %d %d", fbPair.drawDepthRect.ulx, fbPair.drawDepthRect.uly, fbPair.drawDepthRect.lrx, fbPair.drawDepthRect.lry);
                ImGui::Unindent();
                ImGui::NewLine();

                int fbOpType = 0;
                for (auto &fbOperations : { fbPair.startFbOperations, fbPair.endFbOperations }) {
                    ImGui::PushID(fbOpType);

                    const uint32_t opCount = uint32_t(fbOperations.size());
                    if (opCount > 0) {
                        const std::string suffix = (fbOpType > 0) ? "(End)" : "(Start)";
                        const std::string fbOpsName = "Framebuffer Operations" + suffix;
                        if (ImGui::TreeNode(fbOpsName.c_str(), "%s", fbOpsName.c_str())) {
                            ImGui::Indent();
                            for (uint32_t o = 0; o < opCount; o++) {
                                const auto &fbOp = fbOperations[o];
                                const std::string opName = "Operation #" + std::to_string(o);
                                if (ImGui::TreeNode(opName.c_str(), "%s", opName.c_str())) {
                                    ImGui::Indent();
                                    ImGui::SameLine();
                                    switch (fbOp.type) {
                                    case FramebufferOperation::Type::WriteChanges:
                                        ImGui::Text("WriteChanges");
                                        ImGui::Text("Address: 0x%08X", fbOp.writeChanges.address);
                                        ImGui::Text("Tile ID: %" PRIu64, fbOp.writeChanges.id);
                                        break;
                                    case FramebufferOperation::Type::CreateTileCopy: {
                                        ImGui::Text("CreateTileCopy");
                                        ImGui::Text("Address (Operation): 0x%08X", fbOp.createTileCopy.address);
                                        ImGui::Text("Tile ID: %" PRIu64, fbOp.createTileCopy.id);

                                        const auto &fbTile = fbOp.createTileCopy.fbTile;
                                        ImGui::Text("Address (Tile): 0x%08X", fbTile.address);
                                        ImGui::Text("Left: %u", fbTile.left);
                                        ImGui::Text("Top: %u", fbTile.top);
                                        ImGui::Text("Right: %u", fbTile.right);
                                        ImGui::Text("Bottom: %u", fbTile.bottom);
                                        textPixelSize("Pixel Size:", fbTile.siz);
                                        textPixelFormat("Pixel Format:", fbTile.fmt);
                                        break;
                                    }
                                    case FramebufferOperation::Type::ReinterpretTile:
                                        ImGui::Text("ReinterpretTile");
                                        ImGui::Text("Source Tile ID: %" PRIu64, fbOp.reinterpretTile.srcId);
                                        textPixelSize("Source Pixel Size:", fbOp.reinterpretTile.srcSiz);
                                        textPixelFormat("Source Pixel Format:", fbOp.reinterpretTile.srcFmt);
                                        ImGui::Text("Destination Tile ID: %" PRIu64, fbOp.reinterpretTile.dstId);
                                        textPixelSize("Destination Pixel Size:", fbOp.reinterpretTile.dstSiz);
                                        textPixelFormat("Destination Pixel Format:", fbOp.reinterpretTile.dstFmt);
                                        ImGui::Text("UL Scale S: %d", fbOp.reinterpretTile.ulScaleS);
                                        ImGui::Text("UL Scale T: %d", fbOp.reinterpretTile.ulScaleT);
                                        ImGui::Text("Texel Shift: %u %u", fbOp.reinterpretTile.texelShift[0], fbOp.reinterpretTile.texelShift[1]);
                                        ImGui::Text("Texel Mask: 0x%X 0x%X", fbOp.reinterpretTile.texelMask[0], fbOp.reinterpretTile.texelMask[1]);
                                        ImGui::Text("TLUT Hash: 0x%016" PRIx64, fbOp.reinterpretTile.tlutHash);
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }
                            }

                            ImGui::Unindent();
                            ImGui::TreePop();
                        }
                    }

                    ImGui::PopID();
                    fbOpType++;
                }

                ImGui::Indent();
                for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                    ImGui::PushID(p);

                    bool openProj = false;
                    if (openCall) {
                        openProj = openFbPair && (openCallIndices.projectionIndex == p);
                        ImGui::SetNextItemOpen(openProj);
                    }

                    const auto &proj = fbPair.projections[p];
                    const std::string projName = projectionName(workload, f, p);
                    bool headerOpen = ImGui::CollapsingHeader(projName.c_str());
                    bool highlightProjCalls = highlightFbCalls || ImGui::IsItemHovered();
                    if (headerOpen) {
                        ImGui::Indent();
                        ImGui::Text("Scissor: %d %d %d %d", proj.scissorRect.ulx, proj.scissorRect.uly, proj.scissorRect.lrx, proj.scissorRect.lry);
                        ImGui::NewLine();

                        if (proj.usesViewport()) {
                            const auto &viewport = drawData.rspViewports[proj.transformsIndex];
                            const FixedRect viewportRect = viewport.rect();
                            ImGui::NewLine();
                            ImGui::Text("Viewport Scale: %f %f %f", viewport.scale[0], viewport.scale[1], viewport.scale[2]);
                            ImGui::Text("Viewport Translate: %f %f %f", viewport.translate[0], viewport.translate[1], viewport.translate[2]);
                            ImGui::Text("Viewport Rect: %d %d %d %d", viewportRect.ulx, viewportRect.uly, viewportRect.lrx, viewportRect.lry);
                            ImGui::NewLine();
                            textMatrix("View (estimate):", drawData.viewTransforms[proj.transformsIndex]);
                            ImGui::NewLine();
                            textMatrix("Projection (estimate):", drawData.projTransforms[proj.transformsIndex]);
                            ImGui::NewLine();
                            textMatrix("Merged projection (RSP):", drawData.viewProjTransforms[proj.transformsIndex]);
                            ImGui::NewLine();
                            ImGui::NewLine();
                        }

                        for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                            ImGui::PushID(d);
                            if (openCall) {
                                bool open = openProj && (openCallIndices.drawCallIndex == d);
                                ImGui::SetNextItemOpen(open);
                                if (open) {
                                    ImGui::SetScrollHereY(0.0f);
                                }
                            }

                            const std::string callName = "Call #" + std::to_string(d);
                            bool headerOpen = ImGui::CollapsingHeader(callName.c_str());
                            if (highlightProjCalls || ImGui::IsItemHovered()) {
                                highlightDrawCall(workload, { f, p, d });
                            }

                            if (headerOpen) {
                                ImGui::Indent();

                                const auto &call = proj.gameCalls[d];
                                const auto &callDesc = call.callDesc;

                                if (ImGui::Button("Create draw call mod")) {
                                    memset(outDrawCallKey.tmemHashes, 0, sizeof(outDrawCallKey.tmemHashes));

                                    for (uint32_t t = 0; t < callDesc.tileCount; t++) {
                                        const uint64_t tmemHash = drawData.callTiles[callDesc.tileIndex + t].tmemHashOrID;
                                        if (tmemHash > 0) {
                                            outDrawCallKey.tmemHashes[t] = tmemHash;
                                        }
                                    }

                                    outDrawCallKey.colorCombiner = callDesc.colorCombiner;
                                    outDrawCallKey.otherMode.L = callDesc.otherMode.L;
                                    outDrawCallKey.otherMode.H = callDesc.otherMode.H;
                                    outDrawCallKey.geometryMode = callDesc.geometryMode;
                                    outCreateDrawCallKey = true;
                                }

                                DrawStatus drawStatus(call.callDesc.drawStatusChanges);
                                if (drawStatus.isChanged()) {
                                    ImGui::Text("Draw Attributes Changed");
                                    ImGui::Indent();
                                    for (uint32_t i = 0; i < uint32_t(DrawAttribute::Count); i++) {
                                        if (drawStatus.isChanged(DrawAttribute(i))) {
                                            const std::string attributeName = DrawCall::attributeName(DrawAttribute(i));
                                            ImGui::Text("%s", attributeName.c_str());
                                        }
                                    }

                                    ImGui::Unindent();
                                }

                                if (proj.type == Projection::Type::Rectangle) {
                                    ImGui::NewLine();
                                    const auto &rect = call.callDesc.rect;
                                    ImGui::Text("Rect: %d %d %d %d (%0.2f %0.2f %0.2f %0.2f)", 
                                        rect.ulx, rect.uly, rect.lrx, rect.lry, 
                                        rect.ulx / 4.0f, rect.uly / 4.0f, rect.lrx / 4.0f, rect.lry / 4.0f);

                                    ImGui::Text("Dsdx: %u (%0.6f)", call.callDesc.rectDsdx, call.callDesc.rectDsdx / 1024.0f);
                                    ImGui::Text("Dtdy: %u (%0.6f)", call.callDesc.rectDtdy, call.callDesc.rectDtdy / 1024.0f);
                                }

                                ImGui::NewLine();
                                ImGui::Text("Triangle count: %u", callDesc.triangleCount);

                                const auto &meshDesc = call.meshDesc;
                                if (proj.usesViewport()) {
                                    ImGui::Text("Face Indices Start: %u", call.meshDesc.faceIndicesStart);
                                    if (ImGui::TreeNode("Vertex contents", "Vertex contents")) {
                                        ImGui::Indent();

                                        const uint32_t faceIndicesEnd = meshDesc.faceIndicesStart + callDesc.triangleCount * 3;
                                        for (uint32_t i = meshDesc.faceIndicesStart; i < faceIndicesEnd; i++) {
                                            const uint32_t v = faceIndices[i];
                                            ImGui::Text("#%u: INDEX %u POS %d %d %d TC %0.2f %0.2f NORM/COL %u %u %u %u VIEWPROJ %u WORLD %u FOG %u LIGHT %u/%u LOOKAT %u POS_TF %0.2f %0.2f %0.2f %0.2f POS_SCR %0.2f %0.2f %0.4f",
                                                i, v, posShorts[v * 3 + 0], posShorts[v * 3 + 1], posShorts[v * 3 + 2], tcFloats[v * 2 + 0], tcFloats[v * 2 + 1],
                                                normColBytes[v * 4 + 0], normColBytes[v * 4 + 1], normColBytes[v * 4 + 2], normColBytes[v * 4 + 3],
                                                viewProjIndices[v], worldIndices[v], fogIndices[v], lightIndices[v], lightCounts[v], lookAtIndices[v],
                                                posTransformed[v][0], posTransformed[v][1], posTransformed[v][2], posTransformed[v][3],
                                                posScreen[v][0], posScreen[v][1], posScreen[v][2]);
                                        }

                                        ImGui::Unindent();
                                        ImGui::TreePop();
                                    }

                                    if (ImGui::TreeNode("Min/Max model matrix", "Min/Max model matrix: %u/%u", callDesc.minWorldMatrix, callDesc.maxWorldMatrix)) {
                                        ImGui::Indent();
                                        ImGui::Text("Min matrix segmented address: 0x%08X", drawData.worldTransformSegmentedAddresses[callDesc.minWorldMatrix]);
                                        ImGui::Text("Min matrix physical address: 0x%08X", drawData.worldTransformPhysicalAddresses[callDesc.minWorldMatrix]);
                                        textTransformGroup("Min matrix group:", drawData.transformGroups[drawData.worldTransformGroups[callDesc.minWorldMatrix]]);
                                        textMatrix("Min matrix:", drawData.worldTransforms[callDesc.minWorldMatrix]);
                                        ImGui::Text("Max matrix segmented address: 0x%08X", drawData.worldTransformSegmentedAddresses[callDesc.maxWorldMatrix]);
                                        ImGui::Text("Max matrix physical address: 0x%08X", drawData.worldTransformPhysicalAddresses[callDesc.maxWorldMatrix]);
                                        textTransformGroup("Min matrix group:", drawData.transformGroups[drawData.worldTransformGroups[callDesc.maxWorldMatrix]]);
                                        textMatrix("Max matrix:", drawData.worldTransforms[callDesc.maxWorldMatrix]);
                                        ImGui::Unindent();
                                        ImGui::TreePop();
                                    }

                                    ImGui::NewLine();
                                }
                                else {
                                    if (ImGui::TreeNode("Triangle contents", "Triangle contents")) {
                                        const uint32_t rawVertexEnd = meshDesc.rawVertexStart + callDesc.triangleCount * 3;
                                        for (uint32_t v = meshDesc.rawVertexStart; v < rawVertexEnd; v++) {
                                            ImGui::Text("#%u: POS %0.3f %0.3f %0.3f %0.3f TC %0.3f %0.3f COL %0.3f %0.3f %0.3f %0.3f",
                                                v, triPosFloats[v * 4 + 0], triPosFloats[v * 4 + 1], triPosFloats[v * 4 + 2], triPosFloats[v * 4 + 3],
                                                triTcFloats[v * 2 + 0], triTcFloats[v * 2 + 1],
                                                triColorFloats[v * 4 + 0], triColorFloats[v * 4 + 1], triColorFloats[v * 4 + 2], triColorFloats[v * 4 + 3]);
                                        }

                                        ImGui::TreePop();
                                    }
                                }
                                
                                for (uint32_t l = 0; l < callDesc.loadCount; l++) {
                                    const LoadOperation &loadOp = drawData.loadOperations[callDesc.loadIndex + l];
                                    ImGui::PushID(l);
                                    
                                    if (openCall) {
                                        bool open = ((uint32_t)openLoadIndex == (callDesc.loadIndex + l));
                                        ImGui::SetNextItemOpen(open);
                                        if (open) {
                                            ImGui::SetScrollHereY(0.0f);
                                        }
                                    }

                                    if (ImGui::TreeNode("Load operation", "Load operation #%d:", l)) {
                                        ImGui::Indent();

                                        const auto &texture = loadOp.texture;
                                        ImGui::Text("Texture");
                                        ImGui::Indent();
                                        ImGui::Text("Address: 0x%08X", texture.address);
                                        textPixelFormat("Color format:", texture.fmt);
                                        textPixelSize("Pixel size:", texture.siz);
                                        ImGui::Text("Width: %u", texture.width);
                                        ImGui::Unindent();

                                        const auto &tile = loadOp.tile;
                                        ImGui::Text("Tile");
                                        ImGui::Indent();
                                        textAddressing("Addressing S", tile.cms);
                                        textAddressing("Addressing T", tile.cmt);
                                        ImGui::Text("Mask S %u", tile.masks);
                                        ImGui::Text("Mask T %u", tile.maskt);
                                        ImGui::Text("Shift S %u", tile.shifts);
                                        ImGui::Text("Shift T %u", tile.shiftt);
                                        ImGui::Text("Upper left S %u (%0.2f)", tile.uls, tile.uls / 4.0f);
                                        ImGui::Text("Upper left T %u (%0.2f)", tile.ult, tile.ult / 4.0f);
                                        ImGui::Text("Lower right S %u (%0.2f)", tile.lrs, tile.lrs / 4.0f);
                                        ImGui::Text("Lower right T %u (%0.2f)", tile.lrt, tile.lrt / 4.0f);
                                        textPixelFormat("Color format:", tile.fmt);
                                        textPixelSize("Pixel size:", tile.siz);
                                        ImGui::Text("Line %u", tile.line);
                                        ImGui::Text("TMEM %u", tile.tmem);
                                        ImGui::Text("Palette %u", tile.palette);
                                        ImGui::Unindent();

                                        ImGui::Text("Operation");
                                        ImGui::Indent();
                                        ImGui::Text("Type:");
                                        ImGui::SameLine();
                                        switch (loadOp.type) {
                                        case LoadOperation::Type::Tile: {
                                            const auto &opTile = loadOp.operationTile;
                                            ImGui::Text("Tile");
                                            ImGui::Text("Tile #%u", opTile.tile);
                                            ImGui::Text("Upper left S %u (%0.2f)", opTile.uls, opTile.uls / 4.0f);
                                            ImGui::Text("Upper left T %u (%0.2f)", opTile.ult, opTile.ult / 4.0f);
                                            ImGui::Text("Lower right S %u (%0.2f)", opTile.lrs, opTile.lrs / 4.0f);
                                            ImGui::Text("Lower right T %u (%0.2f)", opTile.lrt, opTile.lrt / 4.0f);
                                            break;
                                        }
                                        case LoadOperation::Type::Block: {
                                            const auto &opBlock = loadOp.operationBlock;
                                            ImGui::Text("Block");
                                            ImGui::Text("Tile #%u", opBlock.tile);
                                            ImGui::Text("Upper left S %u (%0.2f)", opBlock.uls, opBlock.uls / 4.0f);
                                            ImGui::Text("Upper left T %u (%0.2f)", opBlock.ult, opBlock.ult / 4.0f);
                                            ImGui::Text("Lower right S %u (%0.2f)", opBlock.lrs, opBlock.lrs / 4.0f);
                                            ImGui::Text("DXT %u", opBlock.dxt);
                                            break;
                                        }
                                        case LoadOperation::Type::TLUT: {
                                            const auto &opTLUT = loadOp.operationTLUT;
                                            ImGui::Text("TLUT");
                                            ImGui::Text("Tile #%u", opTLUT.tile);
                                            ImGui::Text("Upper left S %u (%0.2f)", opTLUT.uls, opTLUT.uls / 4.0f);
                                            ImGui::Text("Upper left T %u (%0.2f)", opTLUT.ult, opTLUT.ult / 4.0f);
                                            ImGui::Text("Lower right S %u (%0.2f)", opTLUT.lrs, opTLUT.lrs / 4.0f);
                                            ImGui::Text("Lower right T %u (%0.2f)", opTLUT.lrt, opTLUT.lrt / 4.0f);
                                            break;
                                        }
                                        }

                                        ImGui::Unindent();
                                        ImGui::Unindent();
                                        ImGui::TreePop();
                                    }

                                    ImGui::PopID();
                                }
                                
                                for (uint32_t t = 0; t < callDesc.tileCount; t++) {
                                    const DrawCallTile &callTile = drawData.callTiles[callDesc.tileIndex + t];
                                    const LoadTile &loadTile = callTile.loadTile;
                                    if (openCall) {
                                        bool open = ((uint32_t)openTileIndex == (callDesc.tileIndex + t));
                                        ImGui::SetNextItemOpen(open);
                                        if (open) {
                                            ImGui::SetScrollHereY(0.0f);
                                        }
                                    }

                                    ImGui::PushID(t);
                                    if (ImGui::TreeNode("Texture tile", "Texture tile #%d:", t)) {
                                        ImGui::Indent();
                                        textAddressing("Addressing S", loadTile.cms);
                                        textAddressing("Addressing T", loadTile.cmt);
                                        ImGui::Text("Mask S %u", loadTile.masks);
                                        ImGui::Text("Mask T %u", loadTile.maskt);
                                        ImGui::Text("Shift S %u", loadTile.shifts);
                                        ImGui::Text("Shift T %u", loadTile.shiftt);
                                        ImGui::Text("Upper left S %u (%0.2f)", loadTile.uls, loadTile.uls / 4.0f);
                                        ImGui::Text("Upper left T %u (%0.2f)", loadTile.ult, loadTile.ult / 4.0f);
                                        ImGui::Text("Lower right S %u (%0.2f)", loadTile.lrs, loadTile.lrs / 4.0f);
                                        ImGui::Text("Lower right T %u (%0.2f)", loadTile.lrt, loadTile.lrt / 4.0f);
                                        textPixelFormat("Color format:", loadTile.fmt);
                                        textPixelSize("Pixel size:", loadTile.siz);
                                        ImGui::Text("Line %u", loadTile.line);
                                        ImGui::Text("TMEM %u", loadTile.tmem);
                                        ImGui::Text("Palette %u", loadTile.palette);
                                        ImGui::Text("Sample Width %u", callTile.sampleWidth);
                                        ImGui::Text("Sample Height %u", callTile.sampleHeight);

                                        if (callTile.validTexcoords()) {
                                            ImGui::Text("Min Texcoord: %d %d", callTile.minTexcoord.x, callTile.minTexcoord.y);
                                            ImGui::Text("Max Texcoord: %d %d", callTile.maxTexcoord.x, callTile.maxTexcoord.y);
                                        }

                                        if (callTile.tileCopyUsed) {
                                            auto tileIt = fbManager.tileCopies.find(callTile.tmemHashOrID);
                                            if (tileIt != fbManager.tileCopies.end()) {
                                                const auto &tileCopy = tileIt->second;
                                                if (ImGui::TreeNode("Tile Copy", "Tile Copy ID %" PRIu64, tileCopy.id)) {
                                                    ImGui::Indent();
                                                    ImGui::Text("Address %u / 0x%X", tileCopy.address, tileCopy.address);
                                                    ImGui::Text("Left %u", tileCopy.left);
                                                    ImGui::Text("Top %u", tileCopy.top);
                                                    ImGui::Text("Width %u", tileCopy.usedWidth);
                                                    ImGui::Text("Height %u", tileCopy.usedHeight);
                                                    ImGui::Unindent();
                                                    ImGui::TreePop();
                                                }
                                            }
                                        }
                                        else {
                                            showHashAndReplaceButton(callTile.tmemHashOrID);
                                        }

                                        uint32_t textureIndex = 0;
                                        const Texture *texture = nullptr;
                                        if (ImGui::Button("Dump TMEM")) {
                                            textureCache.useTexture(callTile.tmemHashOrID, workload.submissionFrame, textureIndex);
                                            texture = textureCache.getTexture(textureIndex);
                                            if (texture != nullptr) {
                                                std::filesystem::path binFilename = FileDialog::getSaveFilename({ FileFilter("BIN Files", "bin") });
                                                if (!binFilename.empty()) {
                                                    std::ofstream o(binFilename, std::ios_base::out | std::ios_base::binary);
                                                    if (o.is_open()) {
                                                        o.write(reinterpret_cast<const char *>(texture->bytesTMEM.data()), texture->bytesTMEM.size());
                                                    }
                                                }
                                            }
                                        }

                                        ImGui::Unindent();
                                        ImGui::TreePop();
                                    }

                                    ImGui::PopID();
                                }

                                const auto &combiner = callDesc.colorCombiner;
                                const bool twoCycle = (callDesc.otherMode.cycleType() == G_CYC_2CYCLE);
                                if (ImGui::TreeNode("Combine", "Combine: %u %u", combiner.L, combiner.H)) {
                                    ImGui::Indent();
                                    for (int c = 0; c < 2; c++) {
                                        const std::string color = combiner.cycleColorText(c);
                                        const std::string alpha = combiner.cycleAlphaText(c);
                                        ImGui::Text("Cycle #%u:", c);
                                        ImGui::Indent();
                                        ImGui::Text("Color: %s", color.c_str());
                                        ImGui::Text("Alpha: %s", alpha.c_str());
                                        ImGui::Unindent();
                                    }

                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }

                                if (ImGui::TreeNode("Other mode low", "Other mode low: %u", callDesc.otherMode.L)) {
                                    ImGui::Indent();

                                    uint32_t alphaCompare = callDesc.otherMode.alphaCompare();
                                    switch (alphaCompare) {
                                    case G_AC_NONE:
                                        ImGui::Text("Alpha compare: G_AC_NONE");
                                        break;
                                    case G_AC_THRESHOLD:
                                        ImGui::Text("Alpha compare: G_AC_THRESHOLD");
                                        break;
                                    case G_AC_DITHER:
                                        ImGui::Text("Alpha compare: G_AC_DITHER");
                                        break;
                                    default:
                                        ImGui::Text("Alpha compare: UNKNOWN");
                                        break;
                                    }

                                    uint32_t cvgDst = callDesc.otherMode.cvgDst();
                                    switch (cvgDst) {
                                    case CVG_DST_CLAMP:
                                        ImGui::Text("Coverage destination: CVG_DST_CLAMP");
                                        break;
                                    case CVG_DST_WRAP:
                                        ImGui::Text("Coverage destination: CVG_DST_WRAP");
                                        break;
                                    case CVG_DST_FULL:
                                        ImGui::Text("Coverage destination: CVG_DST_FULL");
                                        break;
                                    case CVG_DST_SAVE:
                                        ImGui::Text("Coverage destination: CVG_DST_SAVE");
                                        break;
                                    default:
                                        ImGui::Text("Coverage destination: UNKNOWN");
                                        break;
                                    }
                                    
                                    ImGui::Text("Coverage multiply alpha: %d", callDesc.otherMode.cvgXAlpha());
                                    ImGui::Text("Alpha coverage select: %d", callDesc.otherMode.alphaCvgSel());
                                    ImGui::Text("Color on coverage: %d", callDesc.otherMode.clrOnCvg());
                                    ImGui::Text("Anti-aliasing enabled: %d", callDesc.otherMode.aaEn());

                                    uint32_t zSource = callDesc.otherMode.zSource();
                                    if (zSource == G_ZS_PRIM) {
                                        ImGui::Text("Z source: G_ZS_PRIM");
                                    }
                                    else if (zSource == G_ZS_PIXEL) {
                                        ImGui::Text("Z source: G_ZS_PIXEL");
                                    }

                                    ImGui::Text("Z compare: %d", callDesc.otherMode.zCmp());
                                    ImGui::Text("Z update: %d", callDesc.otherMode.zUpd());

                                    uint32_t zMode = callDesc.otherMode.zMode();
                                    switch (zMode) {
                                    case ZMODE_OPA:
                                        ImGui::Text("Z mode: ZMODE_OPA");
                                        break;
                                    case ZMODE_INTER:
                                        ImGui::Text("Z mode: ZMODE_INTER");
                                        break;
                                    case ZMODE_XLU:
                                        ImGui::Text("Z mode: ZMODE_XLU");
                                        break;
                                    case ZMODE_DEC:
                                        ImGui::Text("Z mode: ZMODE_DEC");
                                        break;
                                    default:
                                        ImGui::Text("Z mode: UNKNOWN");
                                        break;
                                    }

                                    const bool forceBlend = callDesc.otherMode.forceBlend();
                                    ImGui::Text("Force blender: %d", forceBlend);

                                    interop::Blender::EmulationRequirements blenderEmuReqs = interop::Blender::checkEmulationRequirements(callDesc.otherMode);
                                    const int combineCycles = interop::Blender::combineCycleCount(callDesc.otherMode);
                                    const int blendCycles = interop::Blender::blendCycleCount(callDesc.otherMode);
                                    ImGui::Text("Blender cycles used: %d", combineCycles);
                                    ImGui::Text("Blender cycles computed: %d", blendCycles);
                                    ImGui::Text("Blender simple emulation: %d", blenderEmuReqs.simpleEmulation);
                                    if (!blenderEmuReqs.simpleEmulation) {
                                        ImGui::Text("Blender approximation: %d", int32_t(blenderEmuReqs.approximateEmulation));
                                    }

                                    const uint32_t blenderInputs = callDesc.otherMode.blenderInputs();
                                    for (int c = 0; c < 2; c++) {
                                        const bool secondCycle = (c > 0);
                                        auto inputP = interop::Blender::decodeInputP(blenderInputs, secondCycle);
                                        auto inputM = interop::Blender::decodeInputM(blenderInputs, secondCycle);
                                        auto inputA = interop::Blender::decodeInputA(blenderInputs, secondCycle);
                                        auto inputB = interop::Blender::decodeInputB(blenderInputs, secondCycle);
                                        const std::string P = interop::Blender::inputToString(inputP);
                                        const std::string M = interop::Blender::inputToString(inputM);
                                        const std::string A = interop::Blender::inputToString(inputA);
                                        const std::string B = interop::Blender::inputToString(inputB);
                                        ImGui::Text("Cycle #%d:", c);
                                        ImGui::Indent();
                                        ImGui::Text("P: %s", P.c_str());
                                        ImGui::Text("M: %s", M.c_str());
                                        ImGui::Text("A: %s", A.c_str());
                                        ImGui::Text("B: %s", B.c_str());
                                        ImGui::Text("Color = (%s * %s + %s * %s)", P.c_str(), A.c_str(), M.c_str(), B.c_str());
                                        
                                        const bool normalizeCycle = (c == (blendCycles - 1));
                                        if (normalizeCycle) {
                                            ImGui::SameLine();
                                            ImGui::Text(" / (%s + %s)", A.c_str(), B.c_str());
                                        }
                                        
                                        if (!blenderEmuReqs.simpleEmulation) {
                                            ImGui::Text("Passthrough: %d", blenderEmuReqs.cycles[c].passthrough);
                                            ImGui::Text("Numerator can overflow: %d", blenderEmuReqs.cycles[c].numeratorOverflow);
                                            ImGui::Text("Uses framebuffer color: %d", blenderEmuReqs.cycles[c].framebufferColor);
                                        }

                                        ImGui::Unindent();
                                    }

                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }

                                if (ImGui::TreeNode("Other mode high", "Other mode high: %u", callDesc.otherMode.H)) {
                                    const uint32_t cycleType = callDesc.otherMode.cycleType();
                                    const uint32_t textDetail = callDesc.otherMode.textDetail();
                                    const uint32_t textFilt = callDesc.otherMode.textFilt();
                                    const uint32_t textLut = callDesc.otherMode.textLUT();
                                    const uint32_t alphaDither = callDesc.otherMode.alphaDither();
                                    const uint32_t rgbDither = callDesc.otherMode.rgbDither();

                                    ImGui::Indent();

                                    ImGui::Text("Cycle type:");
                                    ImGui::SameLine();
                                    switch (cycleType) {
                                    case G_CYC_1CYCLE:
                                        ImGui::Text("G_CYC_1CYCLE");
                                        break;
                                    case G_CYC_2CYCLE:
                                        ImGui::Text("G_CYC_2CYCLE");
                                        break;
                                    case G_CYC_COPY:
                                        ImGui::Text("G_CYC_COPY");
                                        break;
                                    case G_CYC_FILL:
                                        ImGui::Text("G_CYC_FILL");
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Text("Texture filtering:");
                                    ImGui::SameLine();
                                    switch (textFilt) {
                                    case G_TF_AVERAGE:
                                        ImGui::Text("G_TF_AVERAGE");
                                        break;
                                    case G_TF_BILERP:
                                        ImGui::Text("G_TF_BILERP");
                                        break;
                                    case G_TF_POINT:
                                        ImGui::Text("G_TF_POINT");
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Text("Texture detail:");
                                    ImGui::SameLine();
                                    switch (textDetail) {
                                    case G_TD_CLAMP:
                                        ImGui::Text("G_TD_CLAMP");
                                        break;
                                    case G_TD_SHARPEN:
                                        ImGui::Text("G_TD_SHARPEN");
                                        break;
                                    case G_TD_DETAIL:
                                        ImGui::Text("G_TD_DETAIL");
                                        break;
                                    case G_TD_SHARPEN | G_TD_DETAIL:
                                        ImGui::Text("G_TD_SHARPEN | G_TD_DETAIL");
                                        break;
                                    }
                                    
                                    ImGui::Text("Texture LUT:");
                                    ImGui::SameLine();
                                    switch (textLut) {
                                    case G_TT_NONE:
                                        ImGui::Text("G_TT_NONE");
                                        break;
                                    case G_TT_RGBA16:
                                        ImGui::Text("G_TT_RGBA16");
                                        break;
                                    case G_TT_IA16:
                                        ImGui::Text("G_TT_IA16");
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Text("Alpha Dither:");
                                    ImGui::SameLine();
                                    switch (alphaDither) {
                                    case G_AD_PATTERN:
                                        ImGui::Text("G_AD_PATTERN");
                                        break;
                                    case G_AD_NOTPATTERN:
                                        ImGui::Text("G_AD_NOTPATTERN");
                                        break;
                                    case G_AD_NOISE:
                                        ImGui::Text("G_AD_NOISE");
                                        break;
                                    case G_AD_DISABLE:
                                        ImGui::Text("G_AD_DISABLE");
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Text("RGB Dither:");
                                    ImGui::SameLine();
                                    switch (rgbDither) {
                                    case G_CD_MAGICSQ:
                                        ImGui::Text("G_CD_MAGICSQ");
                                        break;
                                    case G_CD_BAYER:
                                        ImGui::Text("G_CD_BAYER");
                                        break;
                                    case G_CD_NOISE:
                                        ImGui::Text("G_CD_NOISE");
                                        break;
                                    case G_CD_DISABLE:
                                        ImGui::Text("G_CD_DISABLE");
                                        break;
                                    default:
                                        ImGui::Text("Unknown");
                                        break;
                                    }

                                    ImGui::Text("Combiner key: %s", (callDesc.otherMode.combKey() == G_CK_KEY) ? "G_CK_KEY" : "G_CK_NONE");
                                    ImGui::Text("Texture LOD: %s", (callDesc.otherMode.textLOD() == G_TL_LOD) ? "G_TL_LOD" : "G_TL_TILE");
                                    ImGui::Text("Texture perspective: %s", (callDesc.otherMode.textPersp() == G_TP_PERSP) ? "G_TP_PERSP" : "G_TP_NONE");
                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }
                                
                                if (ImGui::TreeNode("Geometry mode", "Geometry mode: %u", callDesc.geometryMode)) {
                                    ImGui::Indent();
                                    ImGui::Text("Lighting: %d", (callDesc.geometryMode & G_LIGHTING) != 0);
                                    ImGui::Text("Point lighting: %d", (callDesc.geometryMode & G_POINT_LIGHTING) != 0);
                                    ImGui::Text("Texture gen: %d", (callDesc.geometryMode & G_TEXTURE_GEN) != 0);
                                    ImGui::Text("Texture gen linear: %d", (callDesc.geometryMode & G_TEXTURE_GEN_LINEAR) != 0);
                                    ImGui::Text("Fog: %d", (callDesc.geometryMode & G_FOG) != 0);
                                    ImGui::Text("Cull flags: %u", callDesc.geometryMode & callDesc.cullBothMask);
                                    ImGui::Text("Shading: %u", (callDesc.geometryMode & G_SHADE) != 0);
                                    ImGui::Text("Shading smooth: %u", (callDesc.geometryMode & callDesc.shadingSmoothMask) != 0);
                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }

                                hlslpp::float4 fillColor;
                                if (fbPair.colorImage.siz == G_IM_SIZ_32b) {
                                    fillColor = ColorConverter::RGBA32::toRGBAF(callDesc.fillColor);
                                }
                                else {
                                    fillColor = ColorConverter::RGBA16::toRGBAF(callDesc.fillColor & 0xFFFF);
                                }

                                const auto &rdp = callDesc.rdpParams;
                                const int32_t *K = rdp.convertK;
                                ImGui::Text("Prim color: %f %f %f %f", rdp.primColor[0], rdp.primColor[1], rdp.primColor[2], rdp.primColor[3]);
                                ImGui::Text("Prim LOD (Frac/Min): %f %f", rdp.primLOD[0], rdp.primLOD[1]);
                                ImGui::Text("Prim depth (Z/DZ): %f %f", rdp.primDepth[0], rdp.primDepth[1]);
                                ImGui::Text("Env color: %f %f %f %f", rdp.envColor[0], rdp.envColor[1], rdp.envColor[2], rdp.envColor[3]);
                                ImGui::Text("Fog color: %f %f %f %f", rdp.fogColor[0], rdp.fogColor[1], rdp.fogColor[2], rdp.fogColor[3]);
                                ImGui::Text("Fill color: %f %f %f %f", fillColor[0], fillColor[1], fillColor[2], fillColor[3]);
                                ImGui::Text("Blend color: %f %f %f %f", rdp.blendColor[0], rdp.blendColor[1], rdp.blendColor[2], rdp.blendColor[3]);
                                ImGui::Text("Convert K: %d %d %d %d %d %d", K[0], K[1], K[2], K[3], K[4], K[5]);
                                ImGui::Text("Key center: %f %f %f", rdp.keyCenter[0], rdp.keyCenter[1], rdp.keyCenter[2]);
                                ImGui::Text("Key scale: %f %f %f", rdp.keyScale[0], rdp.keyScale[1], rdp.keyScale[2]);
                                ImGui::Text("Scissor: %d %d %d %d", callDesc.scissorRect.ulx, callDesc.scissorRect.uly, callDesc.scissorRect.lrx, callDesc.scissorRect.lry);
                                ImGui::Text("Scissor Mode: %u", callDesc.scissorMode);

                                bool pixelShaderButton = ImGui::Button("Dump Pixel Shader");
                                bool vertexShaderButton = ImGui::Button("Dump Vertex Shader");
                                if (pixelShaderButton || vertexShaderButton) {
                                    RasterShaderText shaderText = RasterShader::generateShaderText(call.shaderDesc, true);
                                    std::filesystem::path shaderFilename = FileDialog::getSaveFilename({ FileFilter("HLSL", "hlsl") });
                                    if (!shaderFilename.empty()) {
                                        std::ofstream o(shaderFilename);
                                        if (o.is_open()) {
                                            if (pixelShaderButton) {
                                                o << shaderText.pixelShader;
                                            }
                                            else if (vertexShaderButton) {
                                                o << shaderText.vertexShader;
                                            }

                                            o.close();
                                        }
                                    }
                                }

                                ImGui::Unindent();
                            }

                            ImGui::PopID();
                        }

                        ImGui::Unindent();
                    }
                    else if (highlightProjCalls) {
                        for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                            highlightDrawCall(workload, { f, p, d });
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::Unindent();
            }
            else if (highlightFbCalls) {
                for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                    for (uint32_t d = 0; d < fbPair.projections[p].gameCallCount; d++) {
                        highlightDrawCall(workload, { f, p, d });
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::EndChild();

        openCall = false;
    }

    void DebuggerInspector::rightClick(const Workload &workload, hlslpp::float2 cursorPos) {
        typedef std::pair<CallIndices, float> Hit;
        std::vector<Hit> hits;
        hlslpp::float3 triCoords[3];
        bool hitTri = false;
        float hitDepth = 0.0f;
        const uint32_t *faceIndices = workload.drawData.faceIndices.data();
        const hlslpp::float3 *posScreen = workload.drawData.posScreen.data();
        const float *triPosFloats = workload.drawData.triPosFloats.data();
        uint32_t gameCallIndex = 0;
        int32_t cursorRectX = std::lround(cursorPos[0] * 4.0f);
        int32_t cursorRectY = std::lround(cursorPos[1] * 4.0f);
        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            const auto &fbPair = workload.fbPairs[f];
            for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                const auto &proj = fbPair.projections[p];
                if (proj.type == Projection::Type::Rectangle) {
                    for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                        const auto &call = proj.gameCalls[d];
                        const auto &callDesc = call.callDesc;
                        if (callDesc.rect.contains(cursorRectX, cursorRectY)) {
                            // Use a fake depth that corresponds roughly to where the rect is in the draw order.
                            float fakeDepth = 1.0f - (float(gameCallIndex) / float(workload.gameCallCount));
                            hits.push_back({ { f, p, d }, fakeDepth });
                        }

                        gameCallIndex++;
                    }
                }
                else {
                    for (uint32_t d = 0; d < proj.gameCallCount; d++) {
                        const auto &call = proj.gameCalls[d];
                        const auto &callDesc = call.callDesc;
                        for (uint32_t t = 0; t < callDesc.triangleCount; t++) {
                            if (proj.usesViewport()) {
                                for (uint32_t i = 0; i < 3; i++) {
                                    const uint32_t vertexIndex = faceIndices[call.meshDesc.faceIndicesStart + t * 3 + i];
                                    triCoords[i] = posScreen[vertexIndex];
                                }
                            }
                            else {
                                for (uint32_t i = 0; i < 3; i++) {
                                    const uint32_t vertexIndex = call.meshDesc.rawVertexStart + t * 3 + i;
                                    triCoords[i][0] = triPosFloats[vertexIndex * 4 + 0];
                                    triCoords[i][1] = triPosFloats[vertexIndex * 4 + 1];
                                    triCoords[i][2] = triPosFloats[vertexIndex * 4 + 2];
                                }
                            }

                            hlslpp::float2 baryCoords = barycentricCoordinates(cursorPos, { triCoords[0].x, triCoords[0].y }, { triCoords[1].x, triCoords[1].y }, { triCoords[2].x, triCoords[2].y });
                            if ((baryCoords.x >= 0.0f) && (baryCoords.y >= 0.0f) && (float(baryCoords.x + baryCoords.y) <= 1.0f)) {
                                hitDepth = triCoords[0].z + (triCoords[1].z - triCoords[0].z) * baryCoords.x + (triCoords[2].z - triCoords[0].z) * baryCoords.y;
                                if ((hitDepth >= 0.0f) && (hitDepth <= 1.0f)) {
                                    hits.push_back({ { f, p, d }, hitDepth });
                                    break;
                                }
                            }
                        }

                        gameCallIndex++;
                    }
                }
            }
        }

        if (!hits.empty()) {
            // Sort hits by depth and only insert one entry per draw call.
            std::set<CallIndices> insertedIndices;
            popupCalls.clear();
            std::sort(hits.begin(), hits.end(), [](Hit a, Hit b) {
                return a.second < b.second;
            });

            for (auto hit : hits) {
                if (insertedIndices.find(hit.first) != insertedIndices.end()) {
                    continue;
                }

                popupCalls.push_back(hit.first);
                insertedIndices.insert(hit.first);
            }

            ImGui::OpenPopup("##popupCallindices");
        }
    }

    void DebuggerInspector::enableFreeCamera(const Workload &workload, uint32_t fbPairIndex, uint32_t projIndex) {
        const Projection &gameProj = workload.fbPairs[fbPairIndex].projections[projIndex];
        const interop::float4x4 &projMatrix = workload.drawData.projTransforms[gameProj.transformsIndex];
        camera.enabled = true;
        camera.sceneIndex = 0;
        camera.viewMatrix = workload.drawData.viewTransforms[gameProj.transformsIndex];
        camera.invViewMatrix = hlslpp::inverse(camera.viewMatrix);
        camera.projMatrix = projMatrix;
        camera.nearPlane = nearPlaneFromProj(projMatrix);
        camera.farPlane = farPlaneFromProj(projMatrix);
        camera.fov = fovFromProj(projMatrix);
    }
};