//
// RT64
//

#include "rt64_hlsl_json.h"

namespace interop {
    void to_json(json &j, const float3 &v) {
        j = { v.x, v.y, v.z };
    }

    void from_json(const json &j, float3 &v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
    }

    void to_json(json &j, const float4 &v) {
        j = { v.x, v.y, v.z, v.w };
    }

    void from_json(const json &j, float4 &v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }
};