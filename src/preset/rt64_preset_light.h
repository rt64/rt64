//
// RT64
//

#pragma once

#include "common/rt64_common.h"
#include "shared/rt64_point_light.h"

#include "rt64_preset.h"
#include "rt64_preset_inspector.h"

namespace RT64 {
    class View;

    struct PresetLights : public PresetBase {
        // The enabled status is stored in the light itself instead since more
        // often that not most of them will be enabled, so storing them in a
        // separate list would lead to unnecessary overhead.
        struct Light {
            interop::PointLight description;
            bool enabled;
        };

        std::map<std::string, Light> lightMap;

        virtual bool readJson(const json &jsonObj) override;
        virtual bool writeJson(json &jsonObj) const override;
    };

    struct PresetLightsLibrary : public PresetLibrary<PresetLights> { };

    struct PresetLightsLibraryInspector : public PresetLibraryInspector<PresetLights, PresetLightsLibrary> {
        std::string selectedLightName = "";
        bool skipGizmos = false;

        void inspectLibrary(PresetLightsLibrary &library, RenderWindow window);
        void inspectSelection(PresetLightsLibrary &library);
    };
};