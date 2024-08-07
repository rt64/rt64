//
// RT64
//

#pragma once

#include <filesystem>

#if defined(_WIN32)
#   include <Windows.h>
#else
#   include <sys/mman.h>
#endif

namespace RT64 {
    struct MappedFile {
#if defined(_WIN32)
        HANDLE fileHandle = nullptr;
        HANDLE fileMappingHandle = nullptr;
        LPVOID fileView = nullptr;
        LARGE_INTEGER fileSize = {};
#else
        int fileHandle = -1;
        void *fileView = MAP_FAILED;
        off_t fileSize = 0;
#endif

        MappedFile();
        MappedFile(const std::filesystem::path &path);
        ~MappedFile();
        bool open(const std::filesystem::path &path);
        bool isOpen() const;
        uint8_t *data() const;
        size_t size() const;
    };
};