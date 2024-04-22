//
// RT64
//

#pragma once

#include <string>
#include <filesystem>
#include <rhi/rt64_render_interface_types.h>

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
        static std::atomic<bool> isOpen;

        static void initialize();
        static void finish();
        static std::filesystem::path getDirectoryPath();
        static std::filesystem::path getOpenFilename(const std::vector<FileFilter> &filters);
        static std::filesystem::path getSaveFilename(const std::vector<FileFilter> &filters);
    };
};