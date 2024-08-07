//
// RT64
//

#pragma once

#include <string>
#include <map>
#include <fstream>
#include <iomanip>
#include <set>
#include <filesystem>

#include <json/json.hpp>

#include "common/rt64_common.h"

#ifdef _WIN32
#define PRIpath "ls"
#else
#define PRIpath "s"
#endif

using json = nlohmann::json;

namespace hlslpp {
    extern void to_json(json &j, const float3 &v);
    extern void from_json(const json &j, float3 &v);
    extern void to_json(json &j, const float4 &v);
    extern void from_json(const json &j, float4 &v);
};

namespace RT64 {
    struct PresetBase {
        bool enabled = true;

        virtual ~PresetBase() = default;
        virtual bool readJson(const json &jsonObj);
        virtual bool writeJson(json &jsonObj) const;
    };

    template <class T, class = std::enable_if_t<std::is_base_of_v<PresetBase, T>>>
    struct PresetLibrary {
        // Use a sorted map since we want to preserve alphabetical order.
        std::map<std::string, T> presetMap;

        void readJson(const json &jroot) {
            for (const json &jpreset : jroot["presets"]) {
                std::string name = jpreset["name"];
                if (name.empty()) {
                    continue;
                }

                T preset;
                if (!preset.readJson(jpreset)) {
                    continue;
                }

                presetMap[name] = preset;
            }
        }

        void writeJson(json &jroot) {
            for (const auto &it : presetMap) {
                json jpreset;
                jpreset["name"] = it.first;
                it.second.writeJson(jpreset);
                jroot["presets"].push_back(jpreset);
            }
        }

        bool load(const std::filesystem::path &path) {
            std::ifstream i(path);
            if (!i.is_open()) {
                fprintf(stderr, "Unable to open file at %" PRIpath ".\n", path.c_str());
                return false;
            }

            json jroot;
            i >> jroot;
            readJson(jroot);
            i.close();

            return true;
        }

        bool save(const std::filesystem::path &path) {
            std::ofstream o(path);
            if (!o.is_open()) {
                fprintf(stderr, "Unable to save file to %" PRIpath ".\n", path.c_str());
                return false;
            }

            json jroot;
            writeJson(jroot);
            o << std::setw(4) << jroot << std::endl;
            o.close();

            if (o.bad()) {
                fprintf(stderr, "Error when saving file to %" PRIpath ".\n", path.c_str());
                return false;
            }

            return true;
        }
    };
};