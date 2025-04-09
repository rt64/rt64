//
// RT64
//

#include <filesystem>
#include <unordered_set>

#include <json/json.hpp>

#include "rt64_filesystem.h"

using json = nlohmann::json;

namespace RT64 {
    extern const std::string ReplacementDatabaseFilename;
    extern const std::string ReplacementLowMipCacheFilename;
    extern const std::string ReplacementPackExtension;
    extern const std::string ReplacementKnownExtensions[];
    extern const uint32_t ReplacementMipmapCacheHeaderMagic;
    extern const uint32_t ReplacementMipmapCacheHeaderVersion;

    enum class ReplacementOperation {
        Preload,
        Stream,
        Stall
    };

    enum class ReplacementAutoPath {
        RT64,
        Rice
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ReplacementOperation, {
        { ReplacementOperation::Preload, "preload" },
        { ReplacementOperation::Stream, "stream" },
        { ReplacementOperation::Stall, "stall" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(ReplacementAutoPath, {
        { ReplacementAutoPath::RT64, "rt64" },
        { ReplacementAutoPath::Rice, "rice" }
    });

    struct ReplacementConfiguration {
        ReplacementAutoPath autoPath = ReplacementAutoPath::RT64;
        uint32_t configurationVersion = 2;
        uint32_t hashVersion = 5;
    };

    struct ReplacementHashes {
        std::string rt64;
        std::string rice;
    };

    struct ReplacementTexture {
        std::string path;
        ReplacementHashes hashes;

        bool isEmpty() const {
            return hashes.rt64.empty();
        }
    };

    struct ReplacementOperationFilter {
        std::string wildcard;
        ReplacementOperation operation = ReplacementOperation::Stream;
    };

    struct ReplacementResolvedPath {
        std::string relativePath;
        ReplacementOperation operation = ReplacementOperation::Stream;
    };

    struct ReplacementMipmapCacheHeader {
        uint32_t magic = ReplacementMipmapCacheHeaderMagic;
        uint32_t version = ReplacementMipmapCacheHeaderVersion;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t dxgiFormat = 0;
        uint32_t mipCount = 0;
        uint32_t pathLength = 0;
    };

    struct ReplacementDatabase {
        ReplacementConfiguration config;
        std::vector<ReplacementTexture> textures;
        std::vector<ReplacementOperationFilter> operationFilters;
        std::vector<std::string> extraFiles;
        std::unordered_map<uint64_t, uint32_t> tmemHashToReplaceMap;

        uint32_t addReplacement(const ReplacementTexture &texture);
        void fixReplacement(const std::string &hash, const ReplacementTexture &texture);
        ReplacementTexture getReplacement(uint64_t hash) const;
        ReplacementTexture getReplacement(const std::string &hash) const;
        void buildHashMaps();
        ReplacementOperation resolveOperation(const std::string &relativePath, const std::vector<ReplacementOperationFilter> &filters);
        void resolvePaths(const FileSystem *fileSystem, std::unordered_map<uint64_t, ReplacementResolvedPath> &resolvedPathMap, bool onlyDDS, std::vector<uint64_t> *hashesMissing = nullptr, std::unordered_set<uint64_t> *hashesToPreload = nullptr);
        static uint64_t stringToHash(const std::string &str);
        static std::string hashToString(uint32_t hash);
        static std::string hashToString(uint64_t hash);
        static bool isExtensionKnown(const std::string &extension);
        static bool endsWith(const std::string &str, const std::string &end);
        static std::string toLower(std::string str);
        static std::string removeKnownExtension(const std::string &path);
    };

    extern void to_json(json &j, const ReplacementConfiguration &config);
    extern void from_json(const json &j, ReplacementConfiguration &config);
    extern void to_json(json &j, const ReplacementHashes &hashes);
    extern void from_json(const json &j, ReplacementHashes &hashes);
    extern void to_json(json &j, const ReplacementTexture &texture);
    extern void from_json(const json &j, ReplacementTexture &texture);
    extern void to_json(json &j, const ReplacementOperationFilter &operationFilter);
    extern void from_json(const json &j, ReplacementOperationFilter &operationFilter);
    extern void to_json(json &j, const ReplacementDatabase &db);
    extern void from_json(const json &j, ReplacementDatabase &db);
};
