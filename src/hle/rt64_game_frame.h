//
// RT64
//

#pragma once

#include "preset/rt64_preset_scene.h"
#include "render/rt64_buffer_uploader.h"

#include "rt64_projection.h"
#include "rt64_rigid_body.h"

namespace RT64 {
    struct Workload;
    struct WorkloadQueue;

    struct GameIndices {
        struct FramebufferPair {
            uint32_t workloadIndex;
            uint32_t fbPairIndex;
        };

        struct Projection {
            uint32_t workloadIndex;
            uint32_t fbPairIndex;
            uint32_t projectionIndex;
        };

        struct Call {
            uint32_t workloadIndex;
            uint32_t fbPairIndex;
            uint32_t projectionIndex;
            uint32_t callIndex;
        };
    };

    struct GameScene {
        std::vector<GameIndices::Projection> projections;
    };

    inline bool operator==(const GameIndices::FramebufferPair &lhs, const GameIndices::FramebufferPair &rhs) {
        return (lhs.workloadIndex == rhs.workloadIndex) && (lhs.fbPairIndex == rhs.fbPairIndex);
    }

    struct GameFrameMap {
        struct ViewProjectionMap {
            RigidBody rigidBody;
            uint32_t prevTransformIndex = 0;
            bool mapped = false;
        };

        struct TransformMap {
            RigidBody rigidBody;
            uint32_t prevTransformIndex = 0;
            bool mapped = false;
        };

        struct TileMap {
            float deltaUls = 0;
            float deltaUlt = 0;
            float deltaLrs = 0;
            float deltaLrt = 0;
            float prevUls = 0;
            float prevUlt = 0;
            float prevLrs = 0;
            float prevLrt = 0;
            bool mapped = false;
        };

        struct WorkloadMap {
            std::vector<ViewProjectionMap> viewProjections;
            std::vector<TransformMap> transforms;
            std::vector<TileMap> tiles;
            std::vector<bool> prevTransformsMapped;
            std::vector<bool> prevTilesMapped;
            uint32_t prevWorkloadIndex = 0;
            bool mapped = false;
        };

        std::vector<WorkloadMap> workloads;

        void clear() {
            workloads.clear();
        }
    };

    struct GameCallMap {
        uint32_t sceneProjIndex : 10;
        uint32_t callIndex : 20;
        uint32_t doTransformMatching : 1;
        uint32_t doTileMatching : 1;
    };

    struct GameFrame {
        PresetScene presetScene;
        GameFrameMap frameMap;
        std::vector<GameScene> perspectiveScenes;
        std::vector<GameScene> orthographicScenes;
        std::vector<uint32_t> workloads;
        bool matched = false;

        bool areFramebufferPairsCompatible(const WorkloadQueue &workloadQueue, const GameIndices::FramebufferPair &first, const GameIndices::FramebufferPair &second);
        bool isSceneCompatible(const WorkloadQueue &workloadQueue, const GameScene &scene, const GameIndices::Projection &proj);
        void set(WorkloadQueue &workloadQueue, const uint32_t *workloadIndices, uint32_t indicesCount);
        void match(RenderWorker *worker, WorkloadQueue &workloadQueue, const GameFrame &prevFrame, BufferUploader *velocityUploader, bool &velocityUploaderUsed, bool &tileInterpolationUsed);
        void matchScene(WorkloadQueue &workloadQueue, const GameFrame &prevFrame, const GameScene &curScene, const GameScene &prevScene, std::set<uint32_t> &workloadsModified, bool &tileInterpolationUsed);
        void matchTransform(Workload &curWorkload, const Workload &prevWorkload, GameFrameMap::WorkloadMap &curWorkloadMap, const GameFrameMap::WorkloadMap *prevWorkloadMap, uint32_t curTransformIndex, uint32_t prevTransformIndex, bool &modifiedVelocityBuffer);
        void buildCallHashMap(uint32_t sceneProjIndex, const Workload &workload, const Projection &proj, std::multimap<uint64_t, GameCallMap> &hashMap) const;
        void buildTransformIdMap(const Workload &workload, std::multimap<uint32_t, uint32_t> &idMap, std::vector<uint32_t> &ignoredIdVector) const;
        uint64_t hashFromCall(const GameCall &call, uint32_t matrixIdHash) const;
        bool isDebuggerCameraEnabled(const WorkloadQueue &workloadQueue);
    };
};