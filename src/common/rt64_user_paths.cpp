//
// RT64
//

#include "rt64_user_paths.h"

#if defined(__linux__)
#   include <unistd.h>
#   include <pwd.h>
#elif defined(_WIN32)
#   include <Shlobj.h>
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
#   elif defined(__linux__)
        const char *homedir;

        if ((homedir = getenv("HOME")) == nullptr) {
            homedir = getpwuid(getuid())->pw_dir;
        }

        if (homedir != nullptr) {
            resultPath = std::filesystem::path{ homedir } / (std::string{ "." } + appId.c_str());
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
