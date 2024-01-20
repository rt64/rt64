//
// RT64
//

#pragma once

#include <string>
#include <filesystem>
#include <rhi/rt64_render_interface_types.h>

namespace RT64 {
    struct FileDialog {
        typedef std::pair<std::wstring, std::wstring> Filter;
        static std::atomic<bool> isOpen;

        static void initialize();
        static void finish();
        static std::filesystem::path getDirectoryPath();
        static std::filesystem::path getOpenFilename(const std::vector<Filter> &filters);
        static std::filesystem::path getSaveFilename(const std::vector<Filter> &filters);
    };
};