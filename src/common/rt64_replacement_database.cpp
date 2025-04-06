//
// RT64
//

#include "rt64_replacement_database.h"

#include <cinttypes>

#include "rt64_filesystem_directory.h"

namespace RT64 {
    const std::string ReplacementDatabaseFilename = "rt64.json";
    const std::string ReplacementLowMipCacheFilename = "rt64-low-mip-cache.bin";
    const std::string ReplacementPackExtension = ".rtz";
    const std::string ReplacementKnownExtensions[] = { ".dds", ".png" };
    const uint32_t ReplacementMipmapCacheHeaderMagic = 0x434D4F4CU;
    const uint32_t ReplacementMipmapCacheHeaderVersion = 3U;
    
    static bool checkWildcard(const std::string &str, const std::string &pat) {
        size_t strIndex = 0;
        size_t patIndex = 0;
        size_t wildcardIndex = std::string::npos;
        size_t matchIndex = std::string::npos;
        while (strIndex < str.size()) {
            // Characters match or pattern indicates any character here. Advance both cursors.
            if ((patIndex < pat.size()) && ((pat[patIndex] == '?') || (str[strIndex] == pat[patIndex]))) {
                strIndex++;
                patIndex++;
            }
            // Pattern indicates a wildcard. The match is accepted.
            else if ((patIndex < pat.size()) && (pat[patIndex] == '*')) {
                wildcardIndex = patIndex;
                matchIndex = strIndex;
                patIndex++;
            }
            // There's a match active.
            else if (wildcardIndex != std::string::npos) {
                assert(matchIndex != std::string::npos);
                matchIndex++;
                patIndex = wildcardIndex + 1;
                strIndex = matchIndex;
            }
            // It doesn't match and there's no wildcard, reject the match.
            else {
                return false;
            }
        }

        // Check if the rest of the pattern consists of wildcards.
        while ((patIndex < pat.size()) && (pat[patIndex] == '*')) {
            patIndex++;
        }

        // The match is accepted if we reached the end of the pattern.
        return (patIndex == pat.size());
    }

    // ReplacementDatabase

    uint32_t ReplacementDatabase::addReplacement(const ReplacementTexture &texture) {
        const uint64_t rt64 = stringToHash(texture.hashes.rt64);
        auto it = tmemHashToReplaceMap.find(rt64);
        if (it != tmemHashToReplaceMap.end()) {
            textures[it->second] = texture;
            return it->second;
        }
        else {
            uint32_t textureIndex = uint32_t(textures.size());
            tmemHashToReplaceMap[rt64] = textureIndex;
            textures.emplace_back(texture);
            return textureIndex;
        }
    }

    void ReplacementDatabase::fixReplacement(const std::string &hash, const ReplacementTexture &texture) {
        const uint64_t rt64Old = stringToHash(hash);
        const uint64_t rt64New = stringToHash(texture.hashes.rt64);
        auto it = tmemHashToReplaceMap.find(rt64Old);
        if (it != tmemHashToReplaceMap.end()) {
            textures[it->second] = texture;
            tmemHashToReplaceMap[rt64New] = it->second;
            tmemHashToReplaceMap.erase(it);
        }
    }

    ReplacementTexture ReplacementDatabase::getReplacement(uint64_t hash) const {
        auto it = tmemHashToReplaceMap.find(hash);
        if (it != tmemHashToReplaceMap.end()) {
            return textures[it->second];
        }
        else {
            return ReplacementTexture();
        }
    }

    ReplacementTexture ReplacementDatabase::getReplacement(const std::string &hash) const {
        return getReplacement(stringToHash(hash));
    }

    void ReplacementDatabase::buildHashMaps() {
        tmemHashToReplaceMap.clear();

        for (uint32_t i = 0; i < textures.size(); i++) {
            const ReplacementTexture &texture = textures[i];
            if (!texture.hashes.rt64.empty()) {
                const uint64_t rt64 = stringToHash(texture.hashes.rt64);
                tmemHashToReplaceMap[rt64] = i;
            }
        }
    }

    ReplacementOperation ReplacementDatabase::resolveOperation(const std::string &relativePath, const std::vector<ReplacementOperationFilter> &filters) {
        ReplacementOperation resolution = ReplacementOperation::Stream;

        // Resolve all filters in the order they appear in the database.
        for (const ReplacementOperationFilter &filter : filters) {
            if (checkWildcard(relativePath, filter.wildcard)) {
                resolution = filter.operation;
            }
        }

        return resolution;
    }
    
    void ReplacementDatabase::resolvePaths(const FileSystem *fileSystem, std::unordered_map<uint64_t, ReplacementResolvedPath> &resolvedPathMap, bool onlyDDS, std::vector<uint64_t> *hashesMissing, std::unordered_set<uint64_t> *hashesToPreload) {
        std::unordered_map<std::string, std::string> autoPathMap;
        for (const std::string &relativePath : *fileSystem) {
            const std::filesystem::path relativePathFs = std::filesystem::u8path(relativePath);
            std::string fileExtension = toLower(relativePathFs.extension().u8string());
            if (!isExtensionKnown(fileExtension)) {
                continue;
            }

            std::string fileName = relativePathFs.filename().u8string();
            if (config.autoPath == ReplacementAutoPath::Rice) {
                size_t firstHashSymbol = fileName.find_first_of("#");
                size_t lastUnderscoreSymbol = fileName.find_last_of("_");
                if ((firstHashSymbol != std::string::npos) && (lastUnderscoreSymbol != std::string::npos) && (lastUnderscoreSymbol > firstHashSymbol)) {
                    std::string riceHash = toLower(fileName.substr(firstHashSymbol + 1, lastUnderscoreSymbol - firstHashSymbol - 1));
                    autoPathMap[riceHash] = FileSystem::toForwardSlashes(relativePath);
                }
            }
            else if (config.autoPath == ReplacementAutoPath::RT64) {
                size_t firstDotSymbol = fileName.find_first_of(".");
                if (firstDotSymbol != std::string::npos) {
                    std::string rt64Hash = toLower(fileName.substr(0, firstDotSymbol));
                    autoPathMap[rt64Hash] = FileSystem::toForwardSlashes(relativePath);
                }
            }
        }

        std::vector<ReplacementOperationFilter> standarizedFilters = operationFilters;
        for (ReplacementOperationFilter &filter : standarizedFilters) {
            filter.wildcard = FileSystem::toForwardSlashes(filter.wildcard);
        }

        auto addResolvedPath = [&](uint64_t hash, const std::string &relativePath) {
            // Do not modify the entry if it's already in the map.
            if (resolvedPathMap.find(hash) != resolvedPathMap.end()) {
                return;
            }

            ReplacementResolvedPath &resolvedPath = resolvedPathMap[hash];
            resolvedPath.relativePath = relativePath;
            resolvedPath.operation = resolveOperation(relativePath, standarizedFilters);

            if ((resolvedPath.operation == ReplacementOperation::Preload) && (hashesToPreload != nullptr)) {
                hashesToPreload->insert(hash);
            }
        };

        // Assign paths to all entries in the database.
        // If the entry already has a relative path, look for textures with extensions that are valid.
        // If the entry doesn't have a path but uses auto-path logic, then it'll try to resolve the path using that scheme.
        uint32_t textureIndex = 0;
        for (const ReplacementTexture &texture : textures) {
            if (!texture.path.empty()) {
                uint64_t hashRT64 = ReplacementDatabase::stringToHash(texture.hashes.rt64);
                std::string relativePath = FileSystem::toForwardSlashes(texture.path);
                std::string relativePathBase = removeKnownExtension(relativePath);
                bool fileExists = false;
                for (uint32_t i = 0; i < std::size(ReplacementKnownExtensions); i++) {
                    const std::string relativePathKnown = relativePathBase + ReplacementKnownExtensions[i];
                    std::string canonicalPath = fileSystem->makeCanonical(relativePathKnown);
                    if (!canonicalPath.empty() && fileSystem->exists(canonicalPath)) {
                        if (!onlyDDS || (i == 0)) {
                            std::string relativePathFinal = FileSystem::toForwardSlashes(canonicalPath);
                            addResolvedPath(hashRT64, relativePathFinal);
                        }

                        fileExists = true;
                        break;
                    }
                }

                if (!fileExists && (hashesMissing != nullptr)) {
                    hashesMissing->emplace_back(hashRT64);
                }
            }
            else {
                // Assign the correct hash as the search string.
                std::string searchString;
                if (config.autoPath == ReplacementAutoPath::Rice) {
                    searchString = texture.hashes.rice;
                }
                else if (config.autoPath == ReplacementAutoPath::RT64) {
                    searchString = texture.hashes.rt64;
                }

                // Find in the auto path map the entry.
                auto it = autoPathMap.find(searchString);
                if (it != autoPathMap.end()) {
                    uint64_t hashRT64 = ReplacementDatabase::stringToHash(texture.hashes.rt64);
                    addResolvedPath(hashRT64, it->second);
                }
            }

            textureIndex++;
        }
    }

    uint64_t ReplacementDatabase::stringToHash(const std::string &str) {
        return strtoull(str.c_str(), nullptr, 16);
    }

    std::string ReplacementDatabase::hashToString(uint32_t hash) {
        char hexStr[32];
        snprintf(hexStr, sizeof(hexStr), "%08x", hash);
        return std::string(hexStr);
    }

    std::string ReplacementDatabase::hashToString(uint64_t hash) {
        char hexStr[32];
        snprintf(hexStr, sizeof(hexStr), "%016" PRIx64, hash);
        return std::string(hexStr);
    }

    bool ReplacementDatabase::isExtensionKnown(const std::string &extension) {
        uint32_t knownExtensionCount = std::size(ReplacementKnownExtensions);
        for (uint32_t i = 0; i < knownExtensionCount; i++) {
            if (extension == ReplacementKnownExtensions[i]) {
                return true;
            }
        }

        return false;
    }

    bool ReplacementDatabase::endsWith(const std::string &str, const std::string &end) {
        if (str.length() >= end.length()) {
            return (str.compare(str.length() - end.length(), end.length(), end) == 0);
        }
        else {
            return false;
        }
    }

    std::string ReplacementDatabase::toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
        return str;
    };

    std::string ReplacementDatabase::removeKnownExtension(const std::string &path) {
        const std::string lowerCasePath = toLower(path);
        for (uint32_t i = 0; i < std::size(ReplacementKnownExtensions); i++) {
            if (endsWith(lowerCasePath, ReplacementKnownExtensions[i])) {
                return path.substr(0, path.size() - ReplacementKnownExtensions[i].size());
            }
        }

        return path;
    }

    void to_json(json &j, const ReplacementConfiguration &config) {
        // Always update the configuration version to the latest one when saving.
        ReplacementConfiguration defaultConfig;
        j["autoPath"] = config.autoPath;
        j["configurationVersion"] = defaultConfig.configurationVersion;
        j["hashVersion"] = config.hashVersion;
    }

    void from_json(const json &j, ReplacementConfiguration &config) {
        ReplacementConfiguration defaultConfig;
        config.autoPath = j.value("autoPath", defaultConfig.autoPath);
        config.configurationVersion = j.value("configurationVersion", 1);
        config.hashVersion = j.value("hashVersion", 1);
    }

    void to_json(json &j, const ReplacementHashes &hashes) {
        j["rt64"] = hashes.rt64;
        j["rice"] = hashes.rice;
    }
    
    void from_json(const json &j, ReplacementHashes &hashes) {
        ReplacementHashes defaultHashes;

        // First version of the replacement database specified the hash version directly in the key name.
        // Later versions choose to keep the version global to the file and make RT64 the unique key.
        hashes.rt64 = j.value("rt64v1", defaultHashes.rt64);
        hashes.rt64 = j.value("rt64", hashes.rt64);
        hashes.rice = j.value("rice", defaultHashes.rice);
    }

    void to_json(json &j, const ReplacementTexture &texture) {
        j["path"] = texture.path;
        j["hashes"] = texture.hashes;
    }

    void from_json(const json &j, ReplacementTexture &texture) {
        ReplacementTexture defaultTexture;
        texture.path = j.value("path", defaultTexture.path);
        texture.hashes = j.value("hashes", defaultTexture.hashes);
    }

    void to_json(json &j, const ReplacementOperationFilter &operationFilter) {
        j["wildcard"] = operationFilter.wildcard;
        j["operation"] = operationFilter.operation;
    }

    void from_json(const json &j, ReplacementOperationFilter &operationFilter) {
        ReplacementOperationFilter defaultFilter;
        operationFilter.wildcard = j.value("wildcard", defaultFilter.wildcard);
        operationFilter.operation = j.value("operation", defaultFilter.operation);
    }

    void to_json(json &j, const ReplacementDatabase &db) {
        j["configuration"] = db.config;
        j["textures"] = db.textures;
        j["operationFilters"] = db.operationFilters;
        j["extraFiles"] = db.extraFiles;
    }

    void from_json(const json &j, ReplacementDatabase &db) {
        db.config = j.value("configuration", ReplacementConfiguration());
        db.textures = j.value("textures", std::vector<ReplacementTexture>());
        db.operationFilters = j.value("operationFilters", std::vector<ReplacementOperationFilter>());
        db.extraFiles = j.value("extraFiles", std::vector<std::string>());
        db.buildHashMaps();
    }
};