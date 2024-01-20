//
// RT64
//

#pragma once

#include <string>
#include <filesystem>

namespace RT64 {
    struct UserPaths {
        std::filesystem::path dataPath;
        std::filesystem::path gamesPath;
        std::filesystem::path configurationPath;
        std::filesystem::path imguiPath;
        std::filesystem::path logPath;

        std::filesystem::path detectDataPath(const std::filesystem::path &appId);
        void setupPaths(const std::filesystem::path &dataPath);
        bool isEmpty() const;
    };
};