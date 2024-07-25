//
// RT64
//

#pragma once

#include "rt64_filesystem.h"

#include <unordered_map>

namespace RT64 {
    struct FileSystemCombinedIterator : FileSystemIteratorImplementation {
        std::unordered_map<std::string, uint32_t>::const_iterator iterator;

        FileSystemCombinedIterator(std::unordered_map<std::string, uint32_t>::const_iterator iterator) {
            this->iterator = iterator;
        }

        const std::string &value() override {
            return iterator->first;
        }

        void increment() override {
            iterator++;
        }

        bool compare(const FileSystemIteratorImplementation *other) const override {
            const FileSystemCombinedIterator *otherCombinedIt = static_cast<const FileSystemCombinedIterator *>(other);
            return (iterator == otherCombinedIt->iterator);
        }
    };

    struct FileSystemCombined : FileSystem {
        std::vector<std::unique_ptr<FileSystem>> fileSystems;
        std::unordered_map<std::string, uint32_t> pathFileSystemMap;
        std::shared_ptr<FileSystemCombinedIterator> endIterator;

        FileSystemCombined(std::vector<std::unique_ptr<FileSystem>> &fileSystemsTaken) {
            fileSystems = std::move(fileSystemsTaken);
            
            // Build map out of the file systems in order.
            for (uint32_t i = 0; i < uint32_t(fileSystems.size()); i++) {
                for (const std::string &path : *fileSystems[i]) {
                    pathFileSystemMap[toForwardSlashes(path)] = i;
                }
            }

            endIterator = std::make_shared<FileSystemCombinedIterator>(pathFileSystemMap.cend());
        }

        ~FileSystemCombined() {};

        Iterator begin() const override {
            return { std::make_shared<FileSystemCombinedIterator>(pathFileSystemMap.cbegin()) };
        }

        Iterator end() const override {
            return { endIterator };
        }

        bool load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const override {
            auto it = pathFileSystemMap.find(path);
            if (it != pathFileSystemMap.end()) {
                return fileSystems[it->second]->load(path, fileData, fileDataMaxByteCount);
            }
            else {
                return false;
            }
        }
        
        size_t getSize(const std::string &path) const override {
            auto it = pathFileSystemMap.find(path);
            if (it != pathFileSystemMap.end()) {
                return fileSystems[it->second]->getSize(path);
            }
            else {
                return 0;
            }
        }

        bool exists(const std::string &path) const override {
            return pathFileSystemMap.find(path) != pathFileSystemMap.end();
        }

        std::string makeCanonical(const std::string &path) const override {
            auto it = pathFileSystemMap.find(path);
            if (it != pathFileSystemMap.end()) {
                return fileSystems[it->second]->makeCanonical(path);
            }
            else {
                return std::string();
            }
        }

        const std::vector<std::unique_ptr<FileSystem>> &getFileSystems() const {
            return fileSystems;
        }

        // Takes ownership of the vector of file systems passed through.
        static std::unique_ptr<FileSystem> create(std::vector<std::unique_ptr<FileSystem>> &fileSystems) {
            return std::make_unique<FileSystemCombined>(fileSystems);
        }
    };
};