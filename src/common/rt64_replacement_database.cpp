//
// RT64
//

#include "rt64_replacement_database.h"

#include <cinttypes>

namespace RT64 {
    // ReplacementDatabase

    void ReplacementDatabase::addReplacement(const ReplacementTexture &texture) {
        const uint64_t rt64 = stringToHash(texture.hashes.rt64);
        auto it = tmemHashToReplaceMap.find(rt64);
        if (it != tmemHashToReplaceMap.end()) {
            textures[it->second] = texture;
        }
        else {
            tmemHashToReplaceMap[rt64] = uint32_t(textures.size());
            textures.emplace_back(texture);
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

    ReplacementTexture ReplacementDatabase::getReplacement(const std::string &hash) const {
        const uint64_t rt64 = stringToHash(hash);
        auto it = tmemHashToReplaceMap.find(rt64);
        if (it != tmemHashToReplaceMap.end()) {
            return textures[it->second];
        }
        else {
            return ReplacementTexture();
        }
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
        j["load"] = texture.load;
        j["life"] = texture.life;
        j["hashes"] = texture.hashes;
    }

    void from_json(const json &j, ReplacementTexture &texture) {
        ReplacementTexture defaultTexture;
        texture.path = j.value("path", defaultTexture.path);
        texture.load = j.value("load", defaultTexture.load);
        texture.life = j.value("life", defaultTexture.life);
        texture.hashes = j.value("hashes", defaultTexture.hashes);
    }

    void to_json(json &j, const ReplacementDatabase &db) {
        j["configuration"] = db.config;
        j["textures"] = db.textures;
    }

    void from_json(const json &j, ReplacementDatabase &db) {
        db.config = j.value("configuration", ReplacementConfiguration());
        db.textures = j.value("textures", std::vector<ReplacementTexture>());
        db.buildHashMaps();
    }
};