//
// RT64
//

#include "rt64_dynamic_libraries.h"

#include <stdio.h>
#include <filesystem>

#if defined(_WIN32)
#   include <Windows.h>
#   include "utf8conv/utf8conv.h"
#endif

namespace RT64 {
    typedef std::pair<std::string, bool> NameRequiredPair;
    
    const auto DynamicLibraryList = {
        NameRequiredPair("dxil.dll", true),
        NameRequiredPair("dxcompiler.dll", true),
#   if DLSS_ENABLED
        NameRequiredPair("nvngx_dlssd.dll", false),
#   endif
    };

    void LocalFunction() { }

    // DynamicLibraries

    bool DynamicLibraries::load() {
#   if defined(_WIN32)
        HMODULE moduleHandle = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, LPCWSTR(LocalFunction), &moduleHandle)) {
            fprintf(stderr, "GetModuleHandleExW failed with error code 0x%X\n", GetLastError());
            return false;
        }

        WCHAR modulePath[FILENAME_MAX];
        DWORD modulePathSz = GetModuleFileNameW(moduleHandle, modulePath, sizeof(modulePath));
        if ((modulePathSz == 0) || (modulePathSz >= sizeof(modulePath))) {
            fprintf(stderr, "GetModuleFileNameW failed with error code 0x%X\n", GetLastError());
            return false;
        }

        // Extract the directory from the module path.
        std::filesystem::path fullPath(modulePath);
        fullPath.remove_filename();

        const std::wstring fullPathStr = fullPath.wstring();
        for (const NameRequiredPair &pair : DynamicLibraryList) {
            const std::wstring libraryPath = fullPathStr + win32::Utf8ToUtf16(pair.first);
            HMODULE libraryModule = LoadLibraryW(libraryPath.c_str());
            if (pair.second && (libraryModule == nullptr)) {
                fprintf(stderr, "LoadLibraryW with path %ls failed with error code 0x%X\n", libraryPath.c_str(), GetLastError());
                return false;
            }
        }
#   endif

        return true;
    }
};