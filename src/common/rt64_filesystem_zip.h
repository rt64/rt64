//
// RT64
//

#pragma once

#include "rt64_filesystem.h"

#include <filesystem>
#include <unordered_map>

namespace RT64 {
    struct FileSystemZip;

    struct FileSystemZipInfo {
        enum class Compression {
            None,
            Deflate,
            Zstd
        };

        size_t localHeaderOffset = 0;
        size_t compressedSize = 0;
        size_t uncompressedSize = 0;
        uint32_t checksumCRC32 = 0;
        Compression compression = Compression::None;
    };

    struct FileSystemZip : FileSystem {
        struct Implementation;
        Implementation *impl = nullptr;

        FileSystemZip(const std::filesystem::path &zipPath, const std::string &basePath);
        ~FileSystemZip() override;
        Iterator begin() const override;
        Iterator end() const override;
        bool load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const override;
        size_t getSize(const std::string &path) const override;
        bool exists(const std::string &path) const override;
        std::string makeCanonical(const std::string &path) const override;
        bool isOpen() const;
        static std::unique_ptr<FileSystem> create(const std::filesystem::path &zipPath, const std::string &basePath);
    };

    struct FileSystemZipIterator : FileSystemIteratorImplementation {
        const FileSystemZip *fileSystem = nullptr;
        std::unordered_map<std::string, FileSystemZipInfo>::iterator iterator;

        FileSystemZipIterator(std::unordered_map<std::string, FileSystemZipInfo>::iterator iterator);
        const std::string &value() override;
        void increment() override;
        bool compare(const FileSystemIteratorImplementation *other) const override;
    };
};