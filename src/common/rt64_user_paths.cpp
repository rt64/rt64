//
// RT64
//

#include "rt64_user_paths.h"

#if defined(__linux__)
#   include <unistd.h>
#   include <pwd.h>
#elif defined(_WIN32)
#   include <Shlobj.h>
#elif defined(__APPLE__)
#   include "apple/rt64_apple.h"
#endif

namespace RT64 {
    // UserPaths

    const std::filesystem::path GamesDirectory = "games";
    const std::filesystem::path ConfigurationFile = "rt64.json";
    const std::filesystem::path ImGuiFile = "rt64-imgui.ini";
    const std::filesystem::path LogFile = "rt64.log";

    std::filesystem::path UserPaths::detectDataPath(const std::filesystem::path &appId) {
        std::filesystem::path resultPath;

#   if defined(_WIN32)
        PWSTR knownPath = NULL;
        HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &knownPath);
        if (result == S_OK) {
            resultPath = std::filesystem::path{ knownPath } / appId;
        }

        CoTaskMemFree(knownPath);
#   elif defined(__linux__) || defined(__APPLE__)
        const char *homeDir = getenv("HOME");
        if (homeDir == nullptr) {
#       if defined(__linux__)
            homeDir = getpwuid(getuid())->pw_dir;
#       elif defined(__APPLE__)
            homeDir = GetHomeDirectory();
#       endif
        }

        if (homeDir != nullptr) {
            // Prefer to store in the .config directory if it exists. Use the home directory otherwise.
            const std::string appDirName = std::string(".") + std::string(appId.c_str());
            std::filesystem::path homePath = homeDir;
            std::filesystem::path configPath = homePath / ".config";
            if (std::filesystem::exists(configPath)) {
                resultPath = configPath / appDirName;
            }
            else {
                resultPath = homePath / appDirName;
            }
        }
#   endif

        return resultPath;
    }

    void UserPaths::setupPaths(const std::filesystem::path &dataPath) {
        this->dataPath = dataPath;

        if (!dataPath.empty()) {
            gamesPath = dataPath / GamesDirectory;
            configurationPath = dataPath / ConfigurationFile;
            imguiPath = dataPath / ImGuiFile;
            logPath = dataPath / LogFile;
        }
    }

    bool UserPaths::isEmpty() const {
        return dataPath.empty();
    }
};
