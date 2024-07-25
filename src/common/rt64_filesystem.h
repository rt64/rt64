//
// RT64
//

#pragma once

#include "rt64_common.h"

#include <algorithm>
#include <memory>

namespace RT64 {
    struct FileSystemIteratorImplementation {
        virtual ~FileSystemIteratorImplementation() {}
        virtual const std::string &value() = 0;
        virtual void increment() = 0;
        virtual bool compare(const FileSystemIteratorImplementation *other) const = 0;
    };

    struct FileSystem {
        struct Iterator {
            std::shared_ptr<FileSystemIteratorImplementation> implementation;

            const std::string &operator*() {
                return implementation->value();
            }

            Iterator &operator++() {
                implementation->increment();
                return *this;
            }

            bool operator!=(const Iterator &other) const {
                return !implementation->compare(other.implementation.get());
            }
        };

        virtual ~FileSystem() {};
        virtual Iterator begin() const = 0;
        virtual Iterator end() const = 0;
        virtual bool load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const = 0;
        virtual size_t getSize(const std::string &path) const = 0;
        virtual bool exists(const std::string &path) const = 0;
        virtual std::string makeCanonical(const std::string &path) const = 0;

        // Concrete implementation shortcut.
        bool load(const std::string &path, std::vector<uint8_t> &fileData) {
            size_t fileDataSize = getSize(path);
            if (fileDataSize == 0) {
                return false;
            }

            fileData.resize(fileDataSize);
            return load(path, fileData.data(), fileDataSize);
        }

        static std::string toForwardSlashes(std::string str) {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return (c == '\\') ? '/' : c; });
            return str;
        }
    };
};