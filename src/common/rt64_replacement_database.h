//
// RT64
//

#include <json/json.hpp>

using json = nlohmann::json;

namespace RT64 {
    enum class ReplacementLoad {
        Preload,
        Stream,
        Async,
        Stall
    };

    enum class ReplacementLife {
        Permanent,
        Pool,
        Age
    };

    enum class ReplacementAutoPath {
        RT64,
        Rice
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ReplacementLoad, {
        { ReplacementLoad::Preload, "preload" },
        { ReplacementLoad::Stream, "stream" },
        { ReplacementLoad::Async, "async" },
        { ReplacementLoad::Stall, "stall" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(ReplacementLife, {
        { ReplacementLife::Permanent, "permanent" },
        { ReplacementLife::Pool, "pool" },
        { ReplacementLife::Age, "age" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(ReplacementAutoPath, {
        { ReplacementAutoPath::RT64, "rt64" },
        { ReplacementAutoPath::Rice, "rice" }
    });

    struct ReplacementConfiguration {
        ReplacementAutoPath autoPath = ReplacementAutoPath::RT64;
        uint32_t configurationVersion = 2;
        uint32_t hashVersion = 2;
    };

    struct ReplacementHashes {
        std::string rt64;
        std::string rice;
    };

    struct ReplacementTexture {
        std::string path;
        ReplacementLoad load = ReplacementLoad::Stream;
        ReplacementLife life = ReplacementLife::Pool;
        ReplacementHashes hashes;

        bool isEmpty() const {
            return hashes.rt64.empty();
        }
    };

    struct ReplacementDatabase {
        ReplacementConfiguration config;
        std::vector<ReplacementTexture> textures;
        std::unordered_map<uint64_t, uint32_t> tmemHashToReplaceMap;

        void addReplacement(const ReplacementTexture &texture);
        void fixReplacement(const std::string &hash, const ReplacementTexture &texture);
        ReplacementTexture getReplacement(const std::string &hash) const;
        void buildHashMaps();
        static uint64_t stringToHash(const std::string &str);
        static std::string hashToString(uint32_t hash);
        static std::string hashToString(uint64_t hash);
    };

    extern void to_json(json &j, const ReplacementConfiguration &config);
    extern void from_json(const json &j, ReplacementConfiguration &config);
    extern void to_json(json &j, const ReplacementHashes &hashes);
    extern void from_json(const json &j, ReplacementHashes &hashes);
    extern void to_json(json &j, const ReplacementTexture &texture);
    extern void from_json(const json &j, ReplacementTexture &texture);
    extern void to_json(json &j, const ReplacementDatabase &db);
    extern void from_json(const json &j, ReplacementDatabase &db);
};