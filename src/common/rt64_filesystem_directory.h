//
// RT64
//

#pragma once

#include "rt64_filesystem.h"

#include <filesystem>

namespace RT64 {
    struct FileSystemDirectoryIterator : FileSystemIteratorImplementation {
        std::filesystem::recursive_directory_iterator dirIterator;
        std::filesystem::path directoryPath;
        std::string pathStr;
        bool pathStrValid = false;

        FileSystemDirectoryIterator() {
            // Default constructor.
        }

        FileSystemDirectoryIterator(const std::filesystem::path &directoryPath) {
            this->directoryPath = directoryPath;
            dirIterator = std::filesystem::recursive_directory_iterator(directoryPath);
        }

        const std::string &value() override {
            if (!pathStrValid) {
                pathStr = std::filesystem::relative(dirIterator->path(), directoryPath).u8string();
                pathStrValid = true;
            }

            return pathStr;
        }
        
        void increment() override {
            pathStrValid = false;

            static const std::filesystem::recursive_directory_iterator endIterator;
            do {
                dirIterator++;
            } while ((dirIterator != endIterator) && !dirIterator->is_regular_file());
        }

        bool compare(const FileSystemIteratorImplementation *other) const override {
            const FileSystemDirectoryIterator *otherDirIt = static_cast<const FileSystemDirectoryIterator *>(other);
            return (dirIterator == otherDirIt->dirIterator);
        }
    };

    struct FileSystemDirectory : FileSystem {
        std::filesystem::path directoryPath;
        std::shared_ptr<FileSystemDirectoryIterator> endIterator;

        FileSystemDirectory(const std::filesystem::path &directoryPath) {
            this->directoryPath = directoryPath;
            endIterator = { std::make_shared<FileSystemDirectoryIterator>() };
        }
        
        Iterator begin() const override {
            std::shared_ptr<FileSystemDirectoryIterator> beginIterator = std::make_shared<FileSystemDirectoryIterator>(directoryPath);

            // Search for the first iterator that isn't a directory.
            static const std::filesystem::recursive_directory_iterator endIterator;
            while ((beginIterator->dirIterator != endIterator) && !beginIterator->dirIterator->is_regular_file()) {
                beginIterator->dirIterator++;
            }

            return { beginIterator };
        }

        Iterator end() const override {
            return { endIterator };
        }

        std::string makeCanonical(const std::string &path) const override {
            std::error_code ec;
            std::filesystem::path canonicalPath = std::filesystem::canonical(directoryPath / std::filesystem::u8path(path), ec);
            if (ec) {
                return std::string();
            }

            std::filesystem::path relativePath = std::filesystem::relative(canonicalPath, directoryPath, ec);
            if (ec) {
                return std::string();
            }

            return relativePath.u8string();
        }
        
        bool load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const override {
            std::ifstream fileStream(directoryPath / std::filesystem::u8path(path), std::ios::binary);
            if (fileStream.is_open()) {
                fileStream.read((char *)(fileData), fileDataMaxByteCount);
                return !fileStream.bad();
            }
            else {
                return false;
            }
        }

        size_t getSize(const std::string &path) const override {
            std::error_code ec;
            size_t fileSize = std::filesystem::file_size(directoryPath / std::filesystem::u8path(path), ec);
            if (!ec) {
                return fileSize;
            }
            else {
                return 0;
            }
        }

        bool exists(const std::string &path) const override {
            if (path.empty()) {
                return false;
            }

            return std::filesystem::exists(directoryPath / std::filesystem::u8path(path));
        }

        static std::unique_ptr<FileSystem> create(const std::filesystem::path &directoryPath) {
            if (std::filesystem::exists(directoryPath)) {
                return std::make_unique<FileSystemDirectory>(directoryPath);
            }
            else {
                return nullptr;
            }
        }
    };
};