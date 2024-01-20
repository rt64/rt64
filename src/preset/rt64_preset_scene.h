//
// RT64
//

#pragma once

#include "rt64_preset.h"
#include "rt64_preset_inspector.h"

namespace RT64 {
    struct PresetScene : public PresetBase {
        bool estimateAmbientLight;
        float ambientLightIntensity;
        hlslpp::float3 ambientBaseColor;
        hlslpp::float3 ambientNoGIColor;
        hlslpp::float3 eyeLightDiffuseColor;
        hlslpp::float3 eyeLightSpecularColor;
        float giDiffuseStrength;
        float giBackgroundStrength;
        float tonemapExposure;
        float tonemapWhite;
        float tonemapBlack;
        float minLuminance;
        float luminanceRange;
        float lumaUpdateTime;

        PresetScene();
        virtual bool readJson(const json &jsonObj) override;
        virtual bool writeJson(json &jsonObj) const override;
    };

    struct PresetSceneLibrary : public PresetLibrary<PresetScene> { };

    struct PresetSceneLibraryInspector : public PresetLibraryInspector<PresetScene, PresetSceneLibrary> {
        void inspectLibrary(PresetSceneLibrary &library, RenderWindow window);
        void inspectSelection(PresetSceneLibrary &library);
    };
};