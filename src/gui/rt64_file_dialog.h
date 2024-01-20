//
// RT64
//

#pragma once

#include <string>
#include <filesystem>
#include <rhi/rt64_render_interface_types.h>

namespace RT64 {
    struct FileDialog {
        static std::filesystem::path getOpenFilename(RenderWindow window, const std::wstring &filter);
        static std::filesystem::path getSaveFilename(RenderWindow window, const std::wstring &filter);
    };
};