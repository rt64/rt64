//
// RT64
//

#pragma once

#include "rt64_hlsl.h"

#include <json/json.hpp>

using json = nlohmann::json;

namespace interop {
    extern void to_json(json &j, const float3 &v);
    extern void from_json(const json &j, float3 &v);
    extern void to_json(json &j, const float4 &v);
    extern void from_json(const json &j, float4 &v);
};