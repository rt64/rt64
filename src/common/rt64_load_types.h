//
// RT64
//

#pragma once

#include <json/json.hpp>

using json = nlohmann::json;

namespace RT64 {
    struct LoadTile {
        uint8_t fmt;
        uint8_t siz;
        uint16_t line;
        uint16_t tmem;
        uint8_t palette;
        uint8_t cms;
        uint8_t cmt;
        uint8_t masks;
        uint8_t maskt;
        uint8_t shifts;
        uint8_t shiftt;
        uint16_t uls;
        uint16_t ult;
        uint16_t lrs;
        uint16_t lrt;
    };

    extern void to_json(json &j, const LoadTile &loadTile);
    extern void from_json(const json &j, LoadTile &loadTile);

    struct LoadTexture {
        uint32_t address;
        uint8_t fmt;
        uint8_t siz;
        uint16_t width;
    };

    extern void to_json(json &j, const LoadTexture &loadTexture);
    extern void from_json(const json &j, LoadTexture &loadTexture);

    struct LoadOperationTile {
        uint8_t tile;
        uint16_t uls;
        uint16_t ult;
        uint16_t lrs;
        uint16_t lrt;
    };

    struct LoadOperationBlock {
        uint8_t tile;
        uint16_t uls;
        uint16_t ult;
        uint16_t lrs;
        uint16_t dxt;
    };

    struct LoadOperationTLUT {
        uint8_t tile;
        uint16_t uls;
        uint16_t ult;
        uint16_t lrs;
        uint16_t lrt;
    };

    struct LoadOperation {
        enum class Type {
            Tile,
            Block,
            TLUT
        };

        Type type;
        LoadTile tile;
        LoadTexture texture;

        union {
            LoadOperationTile operationTile;
            LoadOperationBlock operationBlock;
            LoadOperationTLUT operationTLUT;
        };
    };

    enum class LoadTLUT {
        None,
        RGBA16,
        IA16
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(LoadOperation::Type, {
        { LoadOperation::Type::Tile, "Tile" },
        { LoadOperation::Type::Block, "Block" },
        { LoadOperation::Type::TLUT, "TLUT" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(LoadTLUT, {
        { LoadTLUT::None, "None" },
        { LoadTLUT::RGBA16, "RGBA16" },
        { LoadTLUT::IA16, "IA16" }
    });
};