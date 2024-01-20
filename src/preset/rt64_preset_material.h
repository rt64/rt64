//
// RT64
//

#pragma once

#include "shared/rt64_extra_params.h"

#include "rt64_preset.h"
#include "rt64_preset_inspector.h"
#include "rt64_preset_light.h"

#if SCRIPT_ENABLED
#   include "script/rt64_script.h"
#endif

namespace RT64 {
    struct PresetMaterial : public PresetBase {
        interop::ExtraParams description;

        struct {
            std::string presetName;
            float primColorTint;
            float primAlphaAttenuation;
            float envColorTint;
            float envAlphaAttenuation;
            float scale;
        } light;

        struct {
            bool enabled;
#       if SCRIPT_ENABLED
            CallMatchCallback *callMatchCallback;
#       endif
        } interpolation;

        PresetMaterial();
        virtual bool readJson(const json &jsonObj) override;
        virtual bool writeJson(json &jsonObj) const override;
    };

    struct PresetMaterialLibrary : public PresetLibrary<PresetMaterial> { };

    struct PresetMaterialLibraryInspector : public PresetLibraryInspector<PresetMaterial, PresetMaterialLibrary> {
        void inspectLibrary(PresetMaterialLibrary &library, RenderWindow window);
        bool inspectSelection(PresetMaterialLibrary &library, const PresetLightsLibrary &lightsLibrary);
    };
};