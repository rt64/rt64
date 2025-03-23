//;
// RT64
//

#include "rt64_shader_common.h"

#include "xxHash/xxh3.h"

#include "common/rt64_common.h"

#include "shared/rt64_other_mode.h"

namespace RT64 {
    // ShaderDescription

    void ShaderDescription::maskUnusedParameters() {
        // TODO: Mask out all unused parameters based on the status of some of the flags and the combiner.
    }

    uint64_t ShaderDescription::hash() const {
        return XXH3_64bits(this, sizeof(ShaderDescription));
    }

    std::string ShaderDescription::toShader() const {
        std::stringstream ss;
        ss << "RenderParams rp;";
        ss << "rp.omL = " << std::to_string(otherMode.L) << "U;";
        ss << "rp.omH = " << std::to_string(otherMode.H) << "U;";
        ss << "rp.ccL = " << std::to_string(colorCombiner.L) << "U;";
        ss << "rp.ccH = " << std::to_string(colorCombiner.H) << "U;";
        ss << "rp.flags = " << std::to_string(flags.value) << ";";
        return ss.str();
    }

    bool ShaderDescription::outputDepth(bool useMSAA) const {
        (void)useMSAA;
        bool copyMode = (otherMode.cycleType() == G_CYC_COPY);
        bool zSourcePrim = (otherMode.zSource() == G_ZS_PRIM);

        // TODO: validate if issue in the following comment is fixed with the new decal solution:
        // FIXME: Depth output is forced when using multisampling to avoid problems from interactions when sampling the depth buffer directly on decals. 
        // The true case of this issue is still pending investigation (https://github.com/rt64/rt64/issues/24).
        return !copyMode && zSourcePrim;
    }
};