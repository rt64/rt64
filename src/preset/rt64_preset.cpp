//
// RT64
//

#include "rt64_preset.h"

namespace hlslpp {
    void to_json(json &j, const float3 &v) {
        j = { v[0], v[1], v[2] };
    }

    void from_json(const json &j, float3 &v) {
        j.at(0).get_to(v[0]);
        j.at(1).get_to(v[1]);
        j.at(2).get_to(v[2]);
    }

    void to_json(json &j, const float4 &v) {
        j = { v[0], v[1], v[2], v[3] };
    }

    void from_json(const json &j, float4 &v) {
        j.at(0).get_to(v[0]);
        j.at(1).get_to(v[1]);
        j.at(2).get_to(v[2]);
        j.at(3).get_to(v[3]);
    }
};

namespace RT64 {
    bool PresetBase::readJson(const json &jsonObj) {
        enabled = jsonObj.value("enabled", false);
        return true;
    }

    bool PresetBase::writeJson(json &jsonObj) const {
        jsonObj["enabled"] = enabled;
        return true;
    }
}