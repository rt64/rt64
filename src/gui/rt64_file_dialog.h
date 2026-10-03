//
// RT64
//

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#   include <utf8conv/utf8conv.h>
#endif

namespace RT64 {
    struct FileFilter {
#ifdef _WIN32
        std::wstring description;
        std::wstring extensions;

        FileFilter(const std::string &description, const std::string &extensions) {
            this->description = win32::Utf8ToUtf16(description);
            this->extensions = win32::Utf8ToUtf16(extensions);
        }
#else
        std::string description;
        std::string extensions;

        FileFilter(const std::string &description, const std::string &extensions) {
            this->description = description;
            this->extensions = extensions;
        }
#endif
    };

    struct FileDialog {
        typedef std::function<void(const std::filesystem::path &)> Callback;

        static std::atomic<bool> isOpen;

        static void initialize();
        static void finish();
        static void getDirectoryPath(const Callback &callback);
        static void getOpenFilename(const std::vector<FileFilter> &filters, const Callback &callback);
        static void getSaveFilename(const std::vector<FileFilter> &filters, const Callback &callback);
    };
};
