//
// RT64
//

#include "common/rt64_math.h"

#include "rt64_game_frame.h"
#include "rt64_workload_queue.h"

#include "xxHash/xxh3.h"

namespace RT64 {
    // GameFrame
    
    bool GameFrame::areFramebufferPairsCompatible(const WorkloadQueue &workloadQueue, const GameIndices::FramebufferPair &first, const GameIndices::FramebufferPair &second) {
        if (first == second) {
            return true;
        }

        const Workload &firstWorkload = workloadQueue.workloads[first.workloadIndex];
        const Workload &secondWorkload = workloadQueue.workloads[second.workloadIndex];
        const auto &firstFbPair = firstWorkload.fbPairs[first.fbPairIndex];
        const auto &secondFbPair = secondWorkload.fbPairs[second.fbPairIndex];
        if ((firstFbPair.depthRead || firstFbPair.depthWrite) && (secondFbPair.depthRead || secondFbPair.depthWrite)) {
            if (firstFbPair.depthImage.address != secondFbPair.depthImage.address) {
                return false;
            }
        }

        const auto &firstColorImage = firstFbPair.colorImage;
        const auto &secondColorImage = secondFbPair.colorImage;
        if ((firstColorImage.address != secondColorImage.address) ||
            (firstColorImage.fmt != secondColorImage.fmt) ||
            (firstColorImage.siz != secondColorImage.siz) ||
            (firstColorImage.width != secondColorImage.width))
        {
            return false;
        }

        return true;
    }

    bool GameFrame::isSceneCompatible(const WorkloadQueue &workloadQueue, const GameScene &scene, const GameIndices::Projection &proj) {
        assert(!scene.projections.empty());

        const float MatrixDiffTolerance = 1e-6f;
        const Workload &workload = workloadQueue.workloads[proj.workloadIndex];
        const FramebufferPair &fbPair = workload.fbPairs[proj.fbPairIndex];
        const Projection &fbProj = fbPair.projections[proj.projectionIndex];
        const GameIndices::Projection &firstProj = scene.projections.front();
        if (!areFramebufferPairsCompatible(workloadQueue, { firstProj.workloadIndex, firstProj.fbPairIndex }, { proj.workloadIndex, proj.fbPairIndex })) {
            return false;
        }

        const Workload &cmpWorkload = workloadQueue.workloads[firstProj.workloadIndex];
        const FramebufferPair &cmpFbPair = cmpWorkload.fbPairs[firstProj.fbPairIndex];
        const Projection &cmpProj = cmpFbPair.projections[firstProj.projectionIndex];
        const interop::float4x4 &cmpViewMatrix = cmpWorkload.drawData.viewTransforms[cmpProj.transformsIndex];
        const interop::float4x4 &fbViewMatrix = workload.drawData.viewTransforms[fbProj.transformsIndex];
        const float viewMatrixDiff = matrixDifference(cmpViewMatrix, fbViewMatrix);
        if (viewMatrixDiff > MatrixDiffTolerance) {
            return false;
        }

        const interop::float4x4 &cmpProjMatrix = cmpWorkload.drawData.projTransforms[cmpProj.transformsIndex];
        const interop::float4x4 &fbProjMatrix = workload.drawData.projTransforms[fbProj.transformsIndex];
        const float projMatrixDiff = matrixDifference(cmpProjMatrix, fbProjMatrix);
        if (projMatrixDiff > MatrixDiffTolerance) {
            return false;
        }

        return true;
    }

    void GameFrame::set(WorkloadQueue &workloadQueue, const uint32_t *workloadIndices, uint32_t indicesCount) {
        assert(workloadIndices != nullptr);
        assert(indicesCount > 0);

        matched = false;
        perspectiveScenes.clear();
        orthographicScenes.clear();
        workloads.clear();
        workloads.insert(workloads.end(), workloadIndices, workloadIndices + indicesCount);

        auto addProjection = [&](const WorkloadQueue &workloadQueue, const GameIndices::Projection &newProj, std::vector<GameScene> &gameScenes) {
            bool added = false;
            for (auto &gameScene : gameScenes) {
                if (isSceneCompatible(workloadQueue, gameScene, newProj)) {
                    gameScene.projections.emplace_back(newProj);
                    added = true;
                    break;
                }
            }

            if (!added) {
                gameScenes.emplace_back(GameScene());
                gameScenes.back().projections.emplace_back(newProj);
            }
        };

        for (uint32_t i = 0; i < indicesCount; i++) {
            uint32_t w = workloadIndices[i];
            const Workload &workload = workloadQueue.workloads[w];
            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                const FramebufferPair &fbPair = workload.fbPairs[f];
                for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                    const GameIndices::Projection newProj = { w, f, p };
                    const auto &fbPairProj = fbPair.projections[p];
                    switch (fbPairProj.type) {
                    case Projection::Type::Perspective:
                        addProjection(workloadQueue, newProj, perspectiveScenes);
                        break;
                    case Projection::Type::Orthographic:
                        addProjection(workloadQueue, newProj, orthographicScenes);
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // Use the default values for the preset scene.
        presetScene = PresetScene();

        /*
        // Use the settings from all enabled presets. Use the last one sorted by name enabled.
        // TODO: Figure out a way to skip the linear lookup on the map.
        for (const auto &it : sceneLibrary.presetMap) {
            if (!it.second.enabled) {
                continue;
            }

            gameFrame.presetScene = it.second;
        }
        */

        /*
        TODO: Must be per projection.
        if (presetScene.estimateAmbientLight && (lightManager.ambientSum > 0)) {
            hlslpp::float3 ambientLight = { 0.01f, 0.01f, 0.01f };
            ambientLight = lightManager.estimatedAmbientLight(gameFrame.presetScene.ambientLightIntensity);
            presetScene.ambientBaseColor = ambientLight;
            presetScene.ambientNoGIColor = ambientLight;
        }
        */

        for (const GameScene &scene : perspectiveScenes) {
            for (const GameIndices::Projection &projection : scene.projections) {
                Workload &workload = workloadQueue.workloads[projection.workloadIndex];
                FramebufferPair &fbPair = workload.fbPairs[projection.fbPairIndex];
                Projection &proj = fbPair.projections[projection.projectionIndex];

                // Add all the lights stored in the workload.
                for (const interop::PointLight &light : workload.pointLights) {
                    proj.addPointLight(light);
                }
            }
        }

        frameMap.clear();
        frameMap.workloads.resize(workloadQueue.workloads.size());
    }
    
    typedef std::pair<uint32_t, uint32_t> IndexPair;

    bool operator<(const IndexPair &lhs, const IndexPair &rhs) {
        return (lhs.first < rhs.first) || ((lhs.first == rhs.first) && lhs.second < rhs.second);
    }

    struct MatchCandidate {
        uint32_t curIndex = 0;
        uint32_t prevIndex = 0;
        float difference = FLT_MAX;

        MatchCandidate(uint32_t curIndex, uint32_t prevIndex, float difference) {
            this->curIndex = curIndex;
            this->prevIndex = prevIndex;
            this->difference = difference;
        }
    };

    bool operator<(const MatchCandidate &lhs, const MatchCandidate &rhs) {
        return lhs.difference < rhs.difference;
    }

    struct TransformMatchResult {
        float positionDifference = FLT_MAX;
        float orientationDifference = FLT_MAX;
        float screenSpaceDifference = FLT_MAX;
        bool valid = false;

        float computeDifference() const {
            if (!valid) {
                return FLT_MAX;
            }

            float totalDiff = 0.0f;

            const float PositionDiffScale = 1.0f;
            if (positionDifference < FLT_MAX) {
                totalDiff += positionDifference * PositionDiffScale;
            }

            const float OrientationDiffScale = 1.0f;
            if (orientationDifference < FLT_MAX) {
                totalDiff += orientationDifference * OrientationDiffScale;
            }

            const float ScreenSpaceDiffScale = 1.0f;
            if (screenSpaceDifference < FLT_MAX) {
                totalDiff += screenSpaceDifference * ScreenSpaceDiffScale;
            }

            return totalDiff;
        }
    };

    TransformMatchResult computeTransformMatch(const hlslpp::float4x4 &curTransform, const hlslpp::float4x4 &curViewProj, const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &prevViewProj, const RigidBody *prevRigidBody) {
        TransformMatchResult matchResult;

        // Do not accept a match between these transforms if the determinant is different, which indicates they're mirrored from each other.
        const float m0det = hlslpp::determinant(extract3x3(prevTransform));
        const float m1det = hlslpp::determinant(extract3x3(curTransform));
        if ((m0det * m1det) < 0.0f) {
            return matchResult;
        }

        // Compute the difference between the translation components of the 4x4 matrices.
        const hlslpp::float3 curPos = curTransform[3].xyz;
        hlslpp::float3 prevPos = prevTransform[3].xyz;
        if (prevRigidBody != nullptr) {
            prevPos += prevRigidBody->linearVelocity;
        }

        matchResult.positionDifference = hlslpp::length(curPos - prevPos);

        // Compute the dot product difference between the normalized XYZ vectors of the 3x3 matrices.
        matchResult.orientationDifference =
            (1.0f - hlslpp::dot(hlslpp::normalize(curTransform[0].xyz), hlslpp::normalize(prevTransform[0].xyz))) +
            (1.0f - hlslpp::dot(hlslpp::normalize(curTransform[1].xyz), hlslpp::normalize(prevTransform[1].xyz))) +
            (1.0f - hlslpp::dot(hlslpp::normalize(curTransform[2].xyz), hlslpp::normalize(prevTransform[2].xyz)));

        // Compute the difference between the screen-space position of both transforms on their respective projections.
        hlslpp::float4 prevScreenPos = hlslpp::mul(prevTransform[3], prevViewProj);
        hlslpp::float4 curScreenPos = hlslpp::mul(curTransform[3], curViewProj);
        prevScreenPos = (fabs(prevScreenPos.w) < 1e-6f) ? prevScreenPos : prevScreenPos / prevScreenPos.w;
        curScreenPos = (fabs(curScreenPos.w) < 1e-6f) ? curScreenPos : curScreenPos / curScreenPos.w;
        matchResult.screenSpaceDifference = hlslpp::length(curScreenPos.xyz - prevScreenPos.xyz);
        
        matchResult.valid = true;
        return matchResult;
    }

    void GameFrame::match(RenderWorker *worker, WorkloadQueue &workloadQueue, const GameFrame &prevFrame, BufferUploader *velocityUploader, bool &velocityUploaderUsed, bool &tileInterpolationUsed) {
        tileInterpolationUsed = false;
        matched = true;

        thread_local std::set<uint32_t> workloadsModified;
        workloadsModified.clear();

        for (uint32_t w = 0; w < workloads.size(); w++) {
            if (w >= prevFrame.workloads.size()) {
                continue;
            }

            // We assume the workloads will be detected in the same order between frames.
            GameFrameMap::WorkloadMap &workloadMap = frameMap.workloads[workloads[w]];
            workloadMap.prevWorkloadIndex = prevFrame.workloads[w];
            workloadMap.mapped = true;

            Workload &curWorkload = workloadQueue.workloads[workloads[w]];
            const Workload &prevWorkload = workloadQueue.workloads[workloadMap.prevWorkloadIndex];
            workloadMap.viewProjections.clear();
            workloadMap.viewProjections.resize(curWorkload.drawData.viewProjTransforms.size());
            workloadMap.transforms.clear();
            workloadMap.transforms.resize(curWorkload.drawData.worldTransforms.size());
            workloadMap.tiles.clear();
            workloadMap.tiles.resize(curWorkload.drawData.rdpTiles.size());
            workloadMap.prevTransformsMapped.clear();
            workloadMap.prevTransformsMapped.resize(prevWorkload.drawData.worldTransforms.size());
            workloadMap.prevTilesMapped.clear();
            workloadMap.prevTilesMapped.resize(prevWorkload.drawData.rdpTiles.size());

            buildTransformIdMap(curWorkload, curWorkload.transformIdMap, curWorkload.transformIgnoredIds);

            // Retrieve the matching maps for the current and previous workload.
            GameFrameMap::WorkloadMap &curWorkloadMap = frameMap.workloads[workloads[w]];
            const GameFrameMap::WorkloadMap *prevWorkloadMap = nullptr;
            if (prevFrame.matched && prevFrame.frameMap.workloads[workloadMap.prevWorkloadIndex].mapped) {
                prevWorkloadMap = &prevFrame.frameMap.workloads[workloadMap.prevWorkloadIndex];
            }

            // Match the transforms linearly in the order they were submitted.
            auto curIt = curWorkload.transformIdMap.begin();
            auto prevIt = prevWorkload.transformIdMap.begin();
            bool modifiedVelocityBuffer = false;
            while ((curIt != curWorkload.transformIdMap.end()) && (prevIt != prevWorkload.transformIdMap.end())) {
                if (curIt->first < prevIt->first) {
                    curIt++;
                }
                else if (curIt->first > prevIt->first) {
                    prevIt++;
                }
                else {
                    matchTransform(curWorkload, prevWorkload, curWorkloadMap, prevWorkloadMap, curIt->second, prevIt->second, modifiedVelocityBuffer);
                    curIt++;
                    prevIt++;
                }
            }

            if (modifiedVelocityBuffer) {
                workloadsModified.insert(workloads[w]);
            }

            // Any transforms tagged with the empty ID will be instantly marked as used and skipped.
            for (uint32_t curIt : curWorkload.transformIgnoredIds) {
                GameFrameMap::TransformMap &curTransformMap = curWorkloadMap.transforms[curIt];
                curTransformMap.rigidBody = RigidBody();
                curTransformMap.prevTransformIndex = 0;
                curTransformMap.mapped = false;
            }
        }

        thread_local std::vector<MatchCandidate> matchCandidates;
        thread_local std::vector<bool> curScenesMatched;
        thread_local std::vector<bool> prevScenesMatched;
        auto matchScenes = [&](const std::vector<GameScene> &curScenes, const std::vector<GameScene> &prevScenes) {
            // Find the scenes that are the closest match possible.
            matchCandidates.clear();
            for (uint32_t i = 0; i < curScenes.size(); i++) {
                const GameIndices::Projection curFirstProj = curScenes[i].projections[0];
                const Workload &curWorkload = workloadQueue.workloads[curFirstProj.workloadIndex];
                const FramebufferPair &curFbPair = curWorkload.fbPairs[curFirstProj.fbPairIndex];
                const Projection &curProj = curFbPair.projections[curFirstProj.projectionIndex];
                const hlslpp::float4x4 &curViewTransform = curWorkload.drawData.viewTransforms[curProj.transformsIndex];
                const hlslpp::float4x4 &curProjTransform = curWorkload.drawData.projTransforms[curProj.transformsIndex];
                for (uint32_t j = 0; j < prevScenes.size(); j++) {
                    const GameIndices::Projection prevFirstProj = prevScenes[j].projections[0];
                    const Workload &prevWorkload = workloadQueue.workloads[prevFirstProj.workloadIndex];
                    const FramebufferPair &prevFbPair = prevWorkload.fbPairs[prevFirstProj.fbPairIndex];
                    const Projection &prevProj = prevFbPair.projections[prevFirstProj.projectionIndex];
                    const hlslpp::float4x4 &prevViewTransform = prevWorkload.drawData.viewTransforms[prevProj.transformsIndex];
                    const hlslpp::float4x4 &prevProjTransform = prevWorkload.drawData.projTransforms[prevProj.transformsIndex];
                    matchCandidates.emplace_back(i, j, matrixDifference(curViewTransform, prevViewTransform) + matrixDifference(curProjTransform, prevProjTransform));
                }
            }

            curScenesMatched.clear();
            prevScenesMatched.clear();
            curScenesMatched.resize(curScenes.size());
            prevScenesMatched.resize(prevScenes.size());
            std::stable_sort(matchCandidates.begin(), matchCandidates.end());
            for (const MatchCandidate &candidate : matchCandidates) {
                if (curScenesMatched[candidate.curIndex]) {
                    continue;
                }

                if (prevScenesMatched[candidate.prevIndex]) {
                    continue;
                }

                matchScene(workloadQueue, prevFrame, curScenes[candidate.curIndex], prevScenes[candidate.prevIndex], workloadsModified, tileInterpolationUsed);
                curScenesMatched[candidate.curIndex] = true;
                prevScenesMatched[candidate.prevIndex] = true;
            }
        };

        matchScenes(perspectiveScenes, prevFrame.perspectiveScenes);
        matchScenes(orthographicScenes, prevFrame.orthographicScenes);

        if (!workloadsModified.empty()) {
            thread_local std::vector<BufferUploader::Upload> uploads;
            uploads.clear();

            for (uint32_t i : workloadsModified) {
                Workload &workload = workloadQueue.workloads[i];
                uploads.emplace_back(BufferUploader::Upload{ workload.drawData.velShorts.data(), { 0, workload.drawData.velShorts.size() }, sizeof(int16_t), RenderBufferFlag::FORMATTED, {RenderFormat::R16_SINT}, &workload.drawBuffers.velocityBuffer });
            }

            velocityUploader->submit(worker, uploads);
            velocityUploaderUsed = true;
        }
        else {
            velocityUploaderUsed = false;
        }
    }

    void GameFrame::matchScene(WorkloadQueue &workloadQueue, const GameFrame &prevFrame, const GameScene &curScene, const GameScene &prevScene, std::set<uint32_t> &workloadsModified, bool &tileInterpolationUsed) {
        if (curScene.projections.empty() || prevScene.projections.empty()) {
            return;
        }

        thread_local std::multimap<uint64_t, GameCallMap> curCallHashMap;
        thread_local std::multimap<uint64_t, GameCallMap> prevCallHashMap;
        curCallHashMap.clear();
        prevCallHashMap.clear();

        // Pick the first projection for reference of the scene.
        const GameIndices::Projection &firstCurProjIndices = curScene.projections[0];
        const GameIndices::Projection &firstPrevProjIndices = prevScene.projections[0];
        GameFrameMap::WorkloadMap &firstCurWorkloadMap = frameMap.workloads[firstCurProjIndices.workloadIndex];
        Workload &firstCurWorkload = workloadQueue.workloads[firstCurProjIndices.workloadIndex];
        const FramebufferPair &firstCurFbPair = firstCurWorkload.fbPairs[firstCurProjIndices.fbPairIndex];
        const Projection &firstCurProj = firstCurFbPair.projections[firstCurProjIndices.projectionIndex];
        const Workload &firstPrevWorkload = workloadQueue.workloads[firstPrevProjIndices.workloadIndex];
        const FramebufferPair &firstPrevFbPair = firstPrevWorkload.fbPairs[firstPrevProjIndices.fbPairIndex];
        const Projection &firstPrevProj = firstPrevFbPair.projections[firstPrevProjIndices.projectionIndex];
        const GameFrameMap::WorkloadMap *firstPrevWorkloadMap = nullptr;
        if (prevFrame.matched && prevFrame.frameMap.workloads[firstPrevProjIndices.workloadIndex].mapped) {
            firstPrevWorkloadMap = &prevFrame.frameMap.workloads[firstPrevProjIndices.workloadIndex];
        }

        // Build a multimap with all potential compatibilities between draw calls in the projections.
        uint32_t mappedViewProjIndex = UINT32_MAX;
        for (uint32_t p = 0; p < curScene.projections.size(); p++) {
            const GameIndices::Projection &curProjIndices = curScene.projections[p];
            Workload &curWorkload = workloadQueue.workloads[curProjIndices.workloadIndex];
            const FramebufferPair &curFbPair = curWorkload.fbPairs[curProjIndices.fbPairIndex];
            const Projection &curProj = curFbPair.projections[curProjIndices.projectionIndex];
            buildCallHashMap(p, curWorkload, curProj, curCallHashMap);

            // Projection for the entire scene has been matched already, copy the mapping.
            if (mappedViewProjIndex < UINT32_MAX) {
                firstCurWorkloadMap.viewProjections[curProj.transformsIndex] = firstCurWorkloadMap.viewProjections[mappedViewProjIndex];
            }

            // Can't map to a previous projection.
            if (p >= prevScene.projections.size()) {
                continue;
            }

            if (mappedViewProjIndex == UINT32_MAX) {
                GameFrameMap::ViewProjectionMap &viewProjMap = firstCurWorkloadMap.viewProjections[curProj.transformsIndex];
                if (viewProjMap.mapped) {
                    mappedViewProjIndex = curProj.transformsIndex;
                    continue;
                }

                const GameIndices::Projection &prevProjIndices = prevScene.projections[p];
                const Workload &prevWorkload = workloadQueue.workloads[prevProjIndices.workloadIndex];
                const FramebufferPair &prevFbPair = prevWorkload.fbPairs[prevProjIndices.fbPairIndex];
                const Projection &prevProj = prevFbPair.projections[prevProjIndices.projectionIndex];
                const hlslpp::float4x4 &curView = curWorkload.drawData.viewTransforms[curProj.transformsIndex];
                const hlslpp::float4x4 &prevView = prevWorkload.drawData.viewTransforms[prevProj.transformsIndex];
                const uint32_t curProjGroupIndex = curWorkload.drawData.viewProjTransformGroups[curProj.transformsIndex];
                const uint32_t prevProjGroupIndex = prevWorkload.drawData.viewProjTransformGroups[prevProj.transformsIndex];
                const TransformGroup &curProjGroup = curWorkload.drawData.transformGroups[curProjGroupIndex];
                const TransformGroup &prevProjGroup = prevWorkload.drawData.transformGroups[prevProjGroupIndex];

                // Cameras usually look better with simple interpolation, so default decomposition to off.
                bool projectionDecompose = false;
                uint8_t projectionLinearComponent = G_EX_COMPONENT_INTERPOLATE;
                uint8_t projectionAngularComponent = G_EX_COMPONENT_INTERPOLATE;
                uint8_t projectionScaleComponent = G_EX_COMPONENT_INTERPOLATE;
                uint8_t projectionSkewComponent = G_EX_COMPONENT_INTERPOLATE;
                uint8_t projectionPerspectiveComponent = G_EX_COMPONENT_INTERPOLATE;
                if ((curProjGroup.matrixId != G_EX_ID_IGNORE) && (curProjGroup.matrixId != G_EX_ID_AUTO)) {
                    projectionDecompose = curProjGroup.decompose;
                    projectionLinearComponent = curProjGroup.positionInterpolation;
                    projectionAngularComponent = curProjGroup.rotationInterpolation;
                    projectionScaleComponent = curProjGroup.scaleInterpolation;
                    projectionSkewComponent = curProjGroup.skewInterpolation;
                    projectionPerspectiveComponent = curProjGroup.perspectiveInterpolation;
                    viewProjMap.mapped = (curProjGroup.matrixId == prevProjGroup.matrixId);
                }
                else {
                    viewProjMap.mapped = (curProjGroup.matrixId == G_EX_ID_AUTO);
                }

                if (viewProjMap.mapped) {
                    if (firstPrevWorkloadMap != nullptr) {
                        const GameFrameMap::ViewProjectionMap &prevViewProjMap = firstPrevWorkloadMap->viewProjections[prevProj.transformsIndex];
                        viewProjMap.rigidBody = prevViewProjMap.rigidBody;
                    }

                    viewProjMap.rigidBody.updateLinear(prevView, curView, projectionLinearComponent);
                    viewProjMap.rigidBody.updateAngular(prevView, curView, projectionAngularComponent, projectionScaleComponent, projectionSkewComponent);
                    viewProjMap.rigidBody.updateDecomposition(curView, projectionDecompose);
                    viewProjMap.prevTransformIndex = prevProj.transformsIndex;
                }
                else {
                    viewProjMap.rigidBody = RigidBody();
                }

                if (viewProjMap.mapped) {
                    mappedViewProjIndex = curProj.transformsIndex;
                }
            }
        }

        for (uint32_t p = 0; p < prevScene.projections.size(); p++) {
            const GameIndices::Projection &prevProjIndices = prevScene.projections[p];
            const Workload &prevWorkload = workloadQueue.workloads[prevProjIndices.workloadIndex];
            const FramebufferPair &prevFbPair = prevWorkload.fbPairs[prevProjIndices.fbPairIndex];
            const Projection &prevProj = prevFbPair.projections[prevProjIndices.projectionIndex];
            buildCallHashMap(p, prevWorkload, prevProj, prevCallHashMap);
        }

        // FIXME: Transform set needs to be done per unique workload detected.
        thread_local std::set<IndexPair> transformCheckSet;
        thread_local std::set<IndexPair> tileCheckSet;
        transformCheckSet.clear();
        tileCheckSet.clear();

        // Traverse the map and fill the set with all the combinations of transforms to check.
        for (std::pair<uint64_t, GameCallMap> curIt : curCallHashMap) {
            auto prevRange = prevCallHashMap.equal_range(curIt.first);
            for (auto prevIt = prevRange.first; prevIt != prevRange.second; prevIt++) {
                const GameIndices::Projection &curProjIndices = curScene.projections[curIt.second.sceneProjIndex];
                const Workload &curWorkload = workloadQueue.workloads[curProjIndices.workloadIndex];
                const FramebufferPair &curFbPair = curWorkload.fbPairs[curProjIndices.fbPairIndex];
                const Projection &curProj = curFbPair.projections[curProjIndices.projectionIndex];
                const GameCall &curCall = curProj.gameCalls[curIt.second.callIndex];
                const GameIndices::Projection &prevProjIndices = prevScene.projections[prevIt->second.sceneProjIndex];
                const Workload &prevWorkload = workloadQueue.workloads[prevProjIndices.workloadIndex];
                const FramebufferPair &prevFbPair = prevWorkload.fbPairs[prevProjIndices.fbPairIndex];
                const Projection &prevProj = prevFbPair.projections[prevProjIndices.projectionIndex];
                const GameCall &prevCall = prevProj.gameCalls[prevIt->second.callIndex];
                const uint32_t curWorldMatrixCount = (curCall.callDesc.maxWorldMatrix - curCall.callDesc.minWorldMatrix) + 1;
                const uint32_t prevWorldMatrixCount = (prevCall.callDesc.maxWorldMatrix - prevCall.callDesc.minWorldMatrix) + 1;
                if ((curWorldMatrixCount == prevWorldMatrixCount) && curIt.second.doTransformMatching && prevIt->second.doTransformMatching) {
                    for (uint32_t w = 0; w < curWorldMatrixCount; w++) {
                        const uint32_t curWorldMatrix = curCall.callDesc.minWorldMatrix + w;
                        const uint32_t curGroupIndex = curWorkload.drawData.worldTransformGroups[curWorldMatrix];
                        const TransformGroup &curGroup = curWorkload.drawData.transformGroups[curGroupIndex];
                        const uint32_t prevWorldMatrix = prevCall.callDesc.minWorldMatrix + w;
                        const uint32_t prevGroupIndex = prevWorkload.drawData.worldTransformGroups[prevWorldMatrix];
                        const TransformGroup &prevGroup = prevWorkload.drawData.transformGroups[prevGroupIndex];
                        if ((curGroup.matrixId == prevGroup.matrixId) && ((curGroup.matrixId == G_EX_ID_AUTO) || ((curGroup.matrixId != G_EX_ID_IGNORE) && (curGroup.ordering == G_EX_ORDER_AUTO)))) {
                            transformCheckSet.emplace(curWorldMatrix, prevWorldMatrix);
                        }
                    }
                }

                if ((curCall.callDesc.tileCount == prevCall.callDesc.tileCount) && (curIt.second.doTileMatching && prevIt->second.doTileMatching)) {
                    for (uint32_t t = 0; t < curCall.callDesc.tileCount; t++) {
                        const DrawCallTile &curCallTile = curWorkload.drawData.callTiles[curCall.callDesc.tileIndex + t];
                        const DrawCallTile &prevCallTile = prevWorkload.drawData.callTiles[prevCall.callDesc.tileIndex + t];
                        if (curCallTile.tmemHashOrID != prevCallTile.tmemHashOrID) {
                            continue;
                        }

                        tileCheckSet.emplace(curCall.callDesc.tileIndex + t, prevCall.callDesc.tileIndex + t);
                    }
                }
            }
        }

        // Compute all the differences between transforms and insert them into a vector that will be sorted according to the differences.
        thread_local std::vector<MatchCandidate> matchCandidates;
        matchCandidates.clear();

        const RigidBody *prevRigidBody;
        const hlslpp::float4x4 &firstCurViewProj = firstCurWorkload.drawData.viewProjTransforms[firstCurProj.transformsIndex];
        const hlslpp::float4x4 &firstPrevViewProj = firstPrevWorkload.drawData.viewProjTransforms[firstPrevProj.transformsIndex];
        for (const IndexPair &indices : transformCheckSet) {
            const hlslpp::float4x4 &curTransform = firstCurWorkload.drawData.worldTransforms[indices.first];
            const hlslpp::float4x4 &prevTransform = firstPrevWorkload.drawData.worldTransforms[indices.second];
            prevRigidBody = (firstPrevWorkloadMap != nullptr) ? &firstPrevWorkloadMap->transforms[indices.second].rigidBody : nullptr;

            TransformMatchResult matchResult = computeTransformMatch(curTransform, firstCurViewProj, prevTransform, firstPrevViewProj, prevRigidBody);
            if (matchResult.valid) {
                matchCandidates.emplace_back(indices.first, indices.second, matchResult.computeDifference());
            }
        }

        bool modifiedVelocityBuffer = false;
        std::stable_sort(matchCandidates.begin(), matchCandidates.end());
        for (const MatchCandidate candidate : matchCandidates) {
            if (firstCurWorkloadMap.transforms[candidate.curIndex].mapped) {
                continue;
            }

            if (firstCurWorkloadMap.prevTransformsMapped[candidate.prevIndex]) {
                continue;
            }

            matchTransform(firstCurWorkload, firstPrevWorkload, firstCurWorkloadMap, firstPrevWorkloadMap, candidate.curIndex, candidate.prevIndex, modifiedVelocityBuffer);
        }

        if (modifiedVelocityBuffer) {
            workloadsModified.insert(firstCurProjIndices.workloadIndex);
        }

        // Check for tile matches.
        for (const IndexPair &indices : tileCheckSet) {
            if (firstCurWorkloadMap.tiles[indices.first].mapped) {
                continue;
            }

            if (firstCurWorkloadMap.prevTilesMapped[indices.second]) {
                continue;
            }

            // Check for tile compatibility.
            const interop::RDPTile &curTile = firstCurWorkload.drawData.rdpTiles[indices.first];
            const interop::RDPTile &prevTile = firstPrevWorkload.drawData.rdpTiles[indices.second];
            if ((curTile.fmt != prevTile.fmt) ||
                (curTile.siz != prevTile.siz) ||
                (curTile.stride != prevTile.stride) ||
                (curTile.masks != prevTile.masks) ||
                (curTile.maskt != prevTile.maskt) ||
                (curTile.shifts != prevTile.shifts) ||
                (curTile.shiftt != prevTile.shiftt) ||
                (curTile.cms != prevTile.cms) ||
                (curTile.cmt != prevTile.cmt))
            {
                continue;
            }

            GameFrameMap::TileMap &curTileMap = firstCurWorkloadMap.tiles[indices.first];
            if (firstPrevWorkloadMap != nullptr) {
                const GameFrameMap::TileMap &prevTileMap = firstPrevWorkloadMap->tiles[indices.second];
                curTileMap = prevTileMap;
            }
            
            auto modulo = [](int a, int b) {
                if (b != 0) {
                    int r = a % b;
                    return r < 0 ? r + b : r;
                }
                else {
                    return a;
                }
            };

            const float deltaUls = curTile.uls - prevTile.uls;
            const float deltaUlt = curTile.ult - prevTile.ult;
            const float deltaLrs = curTile.lrs - prevTile.lrs;
            const float deltaLrt = curTile.lrt - prevTile.lrt;
            const bool tileScrolled = (deltaUls != 0.0f) || (deltaUlt != 0.0f) || (deltaLrs != 0.0f) || (deltaLrt != 0.0f);
            const int integerUls = std::lround(curTile.uls);
            const int integerUlt = std::lround(curTile.ult);
            const int integerLrs = std::lround(curTile.lrs);
            const int integerLrt = std::lround(curTile.lrt);
            const int expectedUls = std::lround(prevTile.uls + curTileMap.deltaUls);
            const int expectedUlt = std::lround(prevTile.ult + curTileMap.deltaUlt);
            const int expectedLrs = std::lround(prevTile.lrs + curTileMap.deltaLrs);
            const int expectedLrt = std::lround(prevTile.lrt + curTileMap.deltaLrt);
            const bool wrappedUls = (curTile.cms == G_TX_WRAP) && (modulo(integerUls, curTile.masks * 4) == modulo(expectedUls, curTile.masks * 4));
            const bool wrappedUlt = (curTile.cmt == G_TX_WRAP) && (modulo(integerUlt, curTile.maskt * 4) == modulo(expectedUlt, curTile.maskt * 4));
            const bool wrappedLrs = (curTile.cms == G_TX_WRAP) && (modulo(integerLrs, curTile.masks * 4) == modulo(expectedLrs, curTile.masks * 4));
            const bool wrappedLrt = (curTile.cmt == G_TX_WRAP) && (modulo(integerLrt, curTile.maskt * 4) == modulo(expectedLrt, curTile.maskt * 4));
            curTileMap.prevUls = curTile.uls - curTileMap.deltaUls;
            curTileMap.prevUlt = curTile.ult - curTileMap.deltaUlt;
            curTileMap.prevLrs = curTile.lrs - curTileMap.deltaLrs;
            curTileMap.prevLrt = curTile.lrt - curTileMap.deltaLrt;
            curTileMap.deltaUls = wrappedUls || (abs(deltaUls) >= curTile.masks * 2) ? curTileMap.deltaUls : deltaUls;
            curTileMap.deltaUlt = wrappedUlt || (abs(deltaUlt) >= curTile.maskt * 2) ? curTileMap.deltaUlt : deltaUlt;
            curTileMap.deltaLrs = wrappedLrs || (abs(deltaLrs) >= curTile.masks * 2) ? curTileMap.deltaLrs : deltaLrs;
            curTileMap.deltaLrt = wrappedLrt || (abs(deltaLrt) >= curTile.maskt * 2) ? curTileMap.deltaLrt : deltaLrt;
            curTileMap.mapped = true;
            firstCurWorkloadMap.prevTilesMapped[indices.second] = true;
            tileInterpolationUsed = tileInterpolationUsed || tileScrolled;
        }
    }

    void GameFrame::matchTransform(Workload &curWorkload, const Workload &prevWorkload, GameFrameMap::WorkloadMap &curWorkloadMap, const GameFrameMap::WorkloadMap *prevWorkloadMap, uint32_t curTransformIndex, uint32_t prevTransformIndex, bool &modifiedVelocityBuffer) {
        GameFrameMap::TransformMap &curTransformMap = curWorkloadMap.transforms[curTransformIndex];
        if (prevWorkloadMap != nullptr) {
            curTransformMap.rigidBody = prevWorkloadMap->transforms[prevTransformIndex].rigidBody;
        }

        const hlslpp::float4x4 &curTransform = curWorkload.drawData.worldTransforms[curTransformIndex];
        const hlslpp::float4x4 &prevTransform = prevWorkload.drawData.worldTransforms[prevTransformIndex];
        const uint32_t curGroupIndex = curWorkload.drawData.worldTransformGroups[curTransformIndex];
        const TransformGroup &curGroup = curWorkload.drawData.transformGroups[curGroupIndex];
        curTransformMap.rigidBody.updateLinear(prevTransform, curTransform, curGroup.positionInterpolation);
        curTransformMap.rigidBody.updateAngular(prevTransform, curTransform, curGroup.rotationInterpolation, curGroup.scaleInterpolation, curGroup.skewInterpolation);
        curTransformMap.rigidBody.updatePerspective(prevTransform, curTransform, curGroup.perspectiveInterpolation);
        curTransformMap.rigidBody.updateDecomposition(curTransform, curGroup.decompose);
        curTransformMap.prevTransformIndex = prevTransformIndex;
        curTransformMap.mapped = true;
        curWorkloadMap.prevTransformsMapped[prevTransformIndex] = true;

        uint32_t curVertexIndex = curWorkload.drawData.worldTransformVertexIndices[curTransformIndex];
        uint32_t curVertexCount = curWorkload.drawData.worldTransformVertexCount(curTransformIndex);
        uint32_t prevVertexIndex = prevWorkload.drawData.worldTransformVertexIndices[prevTransformIndex];
        uint32_t prevVertexCount = prevWorkload.drawData.worldTransformVertexCount(prevTransformIndex);
        if ((curGroup.vertexInterpolation != G_EX_COMPONENT_SKIP) && (curVertexCount == prevVertexCount)) {
            const std::vector<int16_t> &curPosShorts = curWorkload.drawData.posShorts;
            const std::vector<int16_t> &prevPosShorts = prevWorkload.drawData.posShorts;
            std::vector<int16_t> &curVelShorts = curWorkload.drawData.velShorts;
            uint64_t curVertexHash = XXH3_64bits(&curPosShorts[curVertexIndex * 3], curVertexCount * 3 * sizeof(int16_t));
            uint64_t prevVertexHash = XXH3_64bits(&prevPosShorts[prevVertexIndex * 3], prevVertexCount * 3 * sizeof(int16_t));
            if (curVertexHash != prevVertexHash) {
                const int16_t *curPosShortsRef = &curPosShorts[curVertexIndex * 3];
                const int16_t *prevPosShortsRef = &prevPosShorts[prevVertexIndex * 3];
                int16_t *curVelShortsRef = &curVelShorts[curVertexIndex * 3];
                for (uint32_t i = 0; i < curVertexCount; i++) {
                    for (uint32_t j = 0; j < 3; j++) {
                        curVelShortsRef[i * 3 + j] = (curPosShortsRef[i * 3 + j] - prevPosShortsRef[i * 3 + j]);
                    }
                }

                modifiedVelocityBuffer = true;
            }
        }
    }
    
    void GameFrame::buildCallHashMap(uint32_t sceneProjIndex, const Workload &workload, const Projection &proj, std::multimap<uint64_t, GameCallMap> &hashMap) const {
        for (uint32_t c = 0; c < proj.gameCallCount; c++) {
            const GameCall &call = proj.gameCalls[c];
            uint32_t matrixIdHash = 0;
            bool doTransformMatching = false;
            bool doTileMatching = false;
            for (uint32_t m = call.callDesc.minWorldMatrix; m <= call.callDesc.maxWorldMatrix; m++) {
                const uint32_t groupIndex = workload.drawData.worldTransformGroups[m];
                const TransformGroup &group = workload.drawData.transformGroups[groupIndex];
                matrixIdHash = matrixIdHash * 33 ^ group.matrixId;

                const bool usesIdWithAutoOrdering = (group.matrixId != G_EX_ID_AUTO) && (group.matrixId != G_EX_ID_IGNORE) && (group.ordering == G_EX_ORDER_AUTO);
                doTransformMatching = doTransformMatching || (group.matrixId == G_EX_ID_AUTO) || usesIdWithAutoOrdering;
                doTileMatching = doTileMatching || (group.tileInterpolation != G_EX_COMPONENT_SKIP);
            }

            hashMap.emplace(hashFromCall(call, matrixIdHash), GameCallMap{ sceneProjIndex, c, doTransformMatching, doTileMatching });
        }
    }

    void GameFrame::buildTransformIdMap(const Workload &workload, std::multimap<uint32_t, uint32_t> &idMap, std::vector<uint32_t> &ignoredIdVector) const {
        idMap.clear();
        ignoredIdVector.clear();

        uint32_t transformCount = uint32_t(workload.drawData.worldTransformGroups.size());
        for (uint32_t i = 0; i < transformCount; i++) {
            const uint32_t groupIndex = workload.drawData.worldTransformGroups[i];
            const TransformGroup &group = workload.drawData.transformGroups[groupIndex];
            if (group.matrixId == G_EX_ID_AUTO) {
                continue;
            }
            else if (group.matrixId == G_EX_ID_IGNORE) {
                ignoredIdVector.emplace_back(i);
            }
            else if (group.ordering == G_EX_ORDER_LINEAR) {
                idMap.emplace(group.matrixId, i);
            }
        }
    }

    uint64_t GameFrame::hashFromCall(const GameCall &call, uint32_t matrixIdHash) const {
        struct CallMatchKey {
            interop::ColorCombiner colorCombiner;
            interop::OtherMode otherMode;
            uint32_t geometryMode;
            uint32_t triangleCount;
            uint32_t matrixIdHash;
        };

        CallMatchKey key;
        key.colorCombiner = call.callDesc.colorCombiner;
        key.otherMode = call.callDesc.otherMode;
        key.geometryMode = call.callDesc.geometryMode;
        key.triangleCount = call.callDesc.triangleCount;
        key.matrixIdHash = matrixIdHash;
        return XXH3_64bits(&key, sizeof(CallMatchKey));
    }

    bool GameFrame::isDebuggerCameraEnabled(const WorkloadQueue &workloadQueue) {
        for (uint32_t w : workloads) {
            if (workloadQueue.workloads[w].debuggerCamera.enabled) {
                return true;
            }
        }

        return false;
    }

    /*
    void resetTransformMap(GameTransformMap &transformMap, const GameFrame &gameFrame) {
        transformMap.fbPairs.resize(gameFrame.fbPairCount);
        for (uint32_t f = 0; f < gameFrame.fbPairCount; f++) {
            const auto &gameFbPair = gameFrame.fbPairs[f];
            auto &mapFbPair = transformMap.fbPairs[f];
            mapFbPair.projections.resize(gameFbPair.projectionCount, { });
            for (uint32_t p = 0; p < gameFbPair.projectionCount; p++) {
                mapFbPair.projections[p].drawCalls.clear();
                mapFbPair.projections[p].drawCalls.resize(gameFbPair.projections[p].drawCallCount, { });
                mapFbPair.projections[p].transformCallMap.clear();
            }

            mapFbPair.prevFbPairIndex = 0;
            mapFbPair.mapped = false;
        }

        transformMap.transforms.clear();
        transformMap.transforms.resize(gameFrame.drawData.worldTransforms.size(), { });
    }

    void makeTransformCallMap(GameProjection &gameProj, std::multimap<uint32_t, uint32_t> &callMap) {
        for (uint32_t d = 0; d < gameProj.drawCallCount; d++) {
            const auto &curCall = gameProj.drawCalls[d];
            const uint32_t startIndex = std::max(curCall.callDesc.minWorldMatrix, static_cast<uint16_t>(1));
            for (uint32_t t = startIndex; t <= curCall.callDesc.maxWorldMatrix; t++) {
                callMap.emplace(t, d);
            }
        }
    }

    void GameFrame::matchPreviousFrame(GameFrame &prevFrame, RenderWorker *worker, BufferUploader *bufferUploader) {
        assert(worker != nullptr);
        assert(bufferUploader != nullptr);
        transformMap.submissionFrame = prevFrame.submissionFrame;
        resetTransformMap(transformMap, *this);
        transformMap.mapped = true;

        static std::vector<bool> prevTransformMapped;
        static std::vector<TransformMatchCandidate> matchCandidates;
        bool uploadVelBuffer = false;
        const auto &prevDrawData = prevFrame.drawData;
        prevTransformMapped.clear();
        prevTransformMapped.resize(prevDrawData.worldTransforms.size(), false);
        for (uint32_t f = 0; f < fbPairCount; f++) {
            if (f >= prevFrame.fbPairCount) {
                continue;
            }

            // Check if the framebuffer pairs are actually compatible.
            auto &curFbPair = fbPairs[f];
            auto &prevFbPair = prevFrame.fbPairs[f];
            if ((curFbPair.colorImage.width != prevFbPair.colorImage.width) || (curFbPair.colorImage.fmt != prevFbPair.colorImage.fmt) || (curFbPair.colorImage.siz != prevFbPair.colorImage.siz)) {
                continue;
            }

            auto &fbPairMap = transformMap.fbPairs[f];
            fbPairMap.mapped = true;
            fbPairMap.prevFbPairIndex = f;

            for (uint32_t p = 0; p < curFbPair.projectionCount; p++) {
                if (p >= prevFbPair.projectionCount) {
                    continue;
                }

                // Check if the projections are actually compatible.
                auto &curProj = curFbPair.projections[p];
                auto &prevProj = prevFbPair.projections[p];
                if (curProj.type != prevProj.type) {
                    continue;
                }

                // TODO: Support more than just perspective projections.
                if (curProj.type != GameProjection::Type::Perspective) {
                    continue;
                }

                auto &projMap = fbPairMap.projections[p];
                auto &callMap = projMap.transformCallMap;
                projMap.mapped = true;
                projMap.prevProjectionIndex = p;
                makeTransformCallMap(curProj, callMap);

                // Make a map for the previous frame if it's empty.
                auto &prevFbMap = prevFrame.transformMap.fbPairs[fbPairMap.prevFbPairIndex];
                auto &prevCallMap = prevFbMap.projections[projMap.prevProjectionIndex].transformCallMap;
                if (prevCallMap.empty()) {
                    makeTransformCallMap(prevProj, prevCallMap);
                }

                // Find all candidates for all transforms.
                // TODO: This double loop explodes in complexity. Consider building a lookup table based on the
                // compatibility of the calls to reduce the iteration time drastically. These could be combiner
                // and other mode flags.
                matchCandidates.clear();
                const hlslpp::float4x4 &curViewProj = drawData.viewProjTransforms[curProj.transformsIndex];
                const hlslpp::float4x4 &prevViewProj = drawData.viewProjTransforms[prevProj.transformsIndex];
                for (auto curIt = callMap.begin(); curIt != callMap.end();) {
                    const uint32_t curTransform = curIt->first;
                    if (!transformMap.transforms[curTransform].mapped) {
                        const auto &curMatrix = drawData.worldTransforms[curTransform];
                        for (auto prevIt = prevCallMap.begin(); prevIt != prevCallMap.end();) {
                            const uint32_t prevTransform = prevIt->first;
                            if (!prevTransformMapped[prevTransform]) {
                                const auto prevMatrix = prevDrawData.worldTransforms[prevTransform];
                                if (isCallCompatible(curProj.drawCalls[curIt->second], drawData, prevProj.drawCalls[prevIt->second], prevDrawData)) {
                                    const TransformMatchResult matchResult = computeTransformMatch(curMatrix, curViewProj, prevMatrix, prevViewProj, prevFrame.transformMap.transforms[prevTransform].rigidBody);
                                    if (matchResult.valid) {
                                        matchCandidates.push_back({ curTransform, prevTransform, matchResult.computeDifference() });
                                    }
                                }
                            }

                            do { ++prevIt; } while (prevIt != prevCallMap.end() && (prevTransform == prevIt->first));
                        }
                    }

                    do { ++curIt; } while (curIt != callMap.end() && (curTransform == curIt->first));
                }

                // Sort all the candidates and assign them.
                std::stable_sort(matchCandidates.begin(), matchCandidates.end());
                for (const TransformMatchCandidate candidate : matchCandidates) {
                    if (transformMap.transforms[candidate.curTransformIndex].mapped) {
                        continue;
                    }

                    if (prevTransformMapped[candidate.prevTransformIndex]) {
                        continue;
                    }

                    auto &curTransformMap = transformMap.transforms[candidate.curTransformIndex];
                    const hlslpp::float4x4 &prevMatrix = prevDrawData.worldTransforms[candidate.prevTransformIndex];
                    const hlslpp::float4x4 &curMatrix = drawData.worldTransforms[candidate.curTransformIndex];
                    curTransformMap.rigidBody = prevFrame.transformMap.transforms[candidate.prevTransformIndex].rigidBody;
                    curTransformMap.rigidBody.updateLinear(prevMatrix, curMatrix);
                    curTransformMap.rigidBody.updateAngular(prevMatrix, curMatrix);
                    curTransformMap.rigidBody.updateDecomposition(curMatrix);
                    curTransformMap.prevTransformIndex = candidate.prevTransformIndex;
                    curTransformMap.mapped = true;
                    prevTransformMapped[candidate.prevTransformIndex] = true;

                    const auto prevRange = prevCallMap.equal_range(candidate.prevTransformIndex);
                    const auto curRange = callMap.equal_range(candidate.curTransformIndex);
                    auto prevRangeIt = prevRange.first;
                    auto curRangeIt = curRange.first;
                    while ((prevRangeIt != prevRange.second) && (curRangeIt != curRange.second)) {
                        // Check if any material defined a custom match callback for this call.
                        GameDrawCall *curCall = &curProj.drawCalls[curRangeIt->second];
                        GameDrawCall *prevCall = &prevProj.drawCalls[prevRangeIt->second];
                        if (curCall->lerpDesc.matchCallback != nullptr) {
                            curCall->lerpDesc.matchCallback(this, curCall, &prevFrame, prevCall);
                        }

                        // Check if the mesh positions are different and compute the velocity buffer if required.
                        // TODO: Compute the velocity buffer and compute the hash some way.
                        const bool meshHashDifference = false;
                        uploadVelBuffer = uploadVelBuffer || meshHashDifference;

                        // Check if the texture hashes are different to mark it as an animated texture material.
                        bool tmemHashDifference = false;
                        if (curCall->callDesc.tileCount == prevCall->callDesc.tileCount) {
                            const uint32_t curTileBase = curCall->callDesc.tileIndex;
                            const uint32_t prevTileBase = prevCall->callDesc.tileIndex;
                            for (uint32_t t = 0; t < curCall->callDesc.tileCount && !tmemHashDifference; t++) {
                                const auto &curTile = drawData.callTiles[curTileBase + t];
                                const auto &prevTile = prevDrawData.callTiles[prevTileBase + t];
                                tmemHashDifference = (curTile.tmemHashOrID != prevTile.tmemHashOrID);
                            }
                        }

                        // TODO: Reimplement.
                        //curCall->materialDesc.lockMask = tmemHashDifference ? 1.0f : 0.0f;

                        auto &drawCall = projMap.drawCalls[curRangeIt->second];
                        drawCall.prevCallIndex = prevRangeIt->second;
                        drawCall.mapped = true;
                        prevRangeIt++;
                        curRangeIt++;
                    }
                }
            }
        }
    }
    */
};