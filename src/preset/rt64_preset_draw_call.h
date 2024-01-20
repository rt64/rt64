//
// RT64
//

#pragma once

#include "rt64_preset.h"

#include <unordered_map>

#include "hle/rt64_draw_call.h"
#include "hle/rt64_workload.h"
#include "render/rt64_texture_cache.h"

#include "rt64_preset_inspector.h"
#include "rt64_preset_material.h"

namespace RT64 {
    struct DrawCallKey {
        uint64_t tmemHashes[8];
        interop::ColorCombiner colorCombiner;
        interop::OtherMode otherMode;
        uint32_t geometryMode;

        uint64_t hash() const;
        static DrawCallKey fromDrawCall(const DrawCall &call, const DrawData &drawData, const TextureCache &textureCache);
        bool operator==(const DrawCallKey &k) const;
    };

    struct DrawCallMask {
        uint32_t otherModeL;
        uint32_t otherModeH;
        uint32_t geometryMode;
        uint64_t attribute;

        static DrawCallMask defaultAll();
    };

    struct PresetDrawCall : public PresetBase {
        DrawCallKey key;
        DrawCallMask mask;
        std::string materialPresetName;

        bool matches(const DrawCallKey &otherKey) const;
        virtual bool readJson(const json &jsonObj) override;
        virtual bool writeJson(json &jsonObj) const override;
    };

    struct PresetDrawCallLibrary : public PresetLibrary<PresetDrawCall> {
        typedef std::unordered_multimap<uint64_t, uint32_t> KeyIndexMap;
        typedef std::pair<KeyIndexMap::iterator, KeyIndexMap::iterator> KeyIndexMapRange;

        std::vector<PresetMaterial> materialCache; // Copies of the material library for faster access.
        std::unordered_map<std::string, uint32_t> nameIndexMap; // Stores the index of the materials pushed to the cache by name.
        KeyIndexMap cachedKeyMaterialMap; // Maps the draw call keys to the indices of the material cache.
        std::unordered_map<uint64_t, bool> cachedKeyMap; // Indicates the key has been processed already.

        void clearCache();

        // Quickly updates a material in the cache without the need to rebuild it entirely. Ignores it if it's not in the cache.
        void updateMaterialInCache(const std::string &materialName, const PresetMaterial &material); 

        KeyIndexMapRange findMaterialsInCache(const DrawCallKey &drawCallKey, const PresetMaterialLibrary &materialLibrary);
    };

    struct PresetDrawCallLibraryInspector : public PresetLibraryInspector<PresetDrawCall, PresetDrawCallLibrary> {
        const char WindowName[32] = "Draw call library";
        PresetDrawCall newPresetTemplate;
        bool newPresetRequested;
        bool scrollToSelection;

        PresetDrawCallLibraryInspector();
        void promptForNew(const DrawCallKey &key);
        void highlightDrawCallIngame(const std::string &presetName, PresetDrawCallLibrary &library, const TextureCache &textureCache);
        void inspectLibrary(PresetDrawCallLibrary &library, RenderWindow window, const TextureCache &textureCache);
        void inspectSelection(PresetDrawCallLibrary &library, const PresetMaterialLibrary &materialLibrary);
    };
};