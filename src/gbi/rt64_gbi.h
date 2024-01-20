//
// RT64
//

#pragma once

#include "hle/rt64_state.h"

#include "rt64_display_list.h"
#include "rt64_f3d.h"

#define UCODE_MAP_SIZE 256

namespace RT64 {
    typedef void (*GBIReset)(State *state);
    typedef void (*GBIFunction)(State *state, DisplayList **dl);

    enum class GBIUCode : uint32_t {
        Unknown = 0,
        RDP,
        F3D,
        F3DGOLDEN,
        F3DPD,
        F3DWAVE,
        F3DEX,
        F3DEX2,
        F3DZEX2,
        S2DEX,
        S2DEX2,
        L3DEX2,
        Count
    };

    struct GBIFlags {
        bool LowP = false;
        bool NoN = false;
        bool ReJ = false;
        bool computeMVP = false;
        bool pointLighting = false;
    };

    struct GBIInstance {
        const char *name = "";
        GBIUCode ucode = GBIUCode::Unknown;
        GBIFlags flags;
    };

    struct GBISegment {
        uint32_t hashLength = 0;
        uint64_t hashValue = 0;
        std::vector<const GBIInstance *> instances;

        bool operator<(const GBISegment &other) const {
            return hashLength < other.hashLength;
        }
    };

    struct GBI {
        GBIUCode ucode = GBIUCode::Unknown;
        GBIReset resetFromTask = nullptr;
        GBIReset resetFromLoad = nullptr;
        GBIFunction map[UCODE_MAP_SIZE] = {};
        std::unordered_map<F3DENUM, uint32_t> constants;
        GBIFlags flags;
    };

    struct GBIManager {
        std::array<GBI, static_cast<uint32_t>(GBIUCode::Count)> gbiCache;

        GBIManager();
        ~GBIManager();
        GBI *getGBIForRDP();
        GBI *getGBIForUCode(uint8_t *RDRAM, uint32_t textAddress, uint32_t dataAddress);
        void deduceGBIInformation(uint8_t *RDRAM, uint32_t textAddress, uint32_t dataAddress);
        GBIFunction getExtendedFunction() const;
    };
};