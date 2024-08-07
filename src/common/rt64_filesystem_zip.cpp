//
// RT64
//

#include "rt64_filesystem_zip.h"

#include <miniz/miniz.h>
#include <zstd.h>

#include "rt64_mapped_file.h"

namespace RT64 {
    enum {
        MZ_ZIP_LOCAL_DIR_HEADER_SIG = 0x04034b50,
        MZ_ZIP_LOCAL_DIR_HEADER_SIZE = 30,
        MZ_ZIP_LDH_FILENAME_LEN_OFS = 26,
        MZ_ZIP_LDH_EXTRA_LEN_OFS = 28,
        MZ_ZSTD = 93,
    };

    static bool startsWith(const std::string &str, const std::string &start) {
        if (str.length() >= start.length()) {
            return (str.compare(0, start.length(), start) == 0);
        }
        else {
            return false;
        }
    }

    // FileSystemZip::Implementation

    struct FileSystemZip::Implementation {
        std::unordered_map<std::string, FileSystemZipInfo> fileInfoMap;
        std::shared_ptr<FileSystemZipIterator> endIterator;
        std::filesystem::path zipPath;
        MappedFile zipMappedFile;
        std::string basePath;
        bool archiveOpen = false;
    };

    // FileSystemZip

    FileSystemZip::FileSystemZip(const std::filesystem::path &zipPath, const std::string &basePath) {
        assert(basePath.empty() || (basePath.back() != '\\') || (basePath.back() != '/'));

        impl = new Implementation();
        if (!impl->zipMappedFile.open(zipPath)) {
            return;
        }

        mz_zip_archive zipArchive = {};
        std::string zipPathStr = zipPath.u8string();
        impl->archiveOpen = mz_zip_reader_init_mem(&zipArchive, impl->zipMappedFile.data(), impl->zipMappedFile.size(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        impl->basePath = basePath.empty() ? std::string() : (basePath + "/");
        impl->zipPath = zipPath;
        
        if (impl->archiveOpen) {
            uint32_t fileCount = mz_zip_reader_get_num_files(&zipArchive);
            for (uint32_t i = 0; i < fileCount; i++) {
                mz_zip_archive_file_stat fileStat = {};
                if (!mz_zip_reader_file_stat(&zipArchive, i, &fileStat)) {
                    impl->archiveOpen = false;
                    break;
                }

                // Ignore directories.
                if (fileStat.m_is_directory) {
                    continue;
                }

                // Ignore entries that don't start with the base path. Take out the base path out of the filename.
                std::string fileName(fileStat.m_filename);
                if (!impl->basePath.empty()) {
                    if (!startsWith(fileName, impl->basePath)) {
                        continue;
                    }

                    fileName = fileName.substr(impl->basePath.size());
                }

                // Detect compression method.
                FileSystemZipInfo::Compression compression;
                switch (fileStat.m_method) {
                case 0:
                    compression = FileSystemZipInfo::Compression::None;
                    break;
                case MZ_DEFLATED:
                    compression = FileSystemZipInfo::Compression::Deflate;
                    break;
                case MZ_ZSTD:
                    compression = FileSystemZipInfo::Compression::Zstd;
                    break;
                default:
                    // Unknown compression method, ignore the file.
                    continue;
                }

                // Store the information necessary for extracting the file later.
                FileSystemZipInfo &fileInfo = impl->fileInfoMap[fileName];
                fileInfo.localHeaderOffset = fileStat.m_local_header_ofs;
                fileInfo.compressedSize = fileStat.m_comp_size;
                fileInfo.uncompressedSize = fileStat.m_uncomp_size;
                fileInfo.checksumCRC32 = fileStat.m_crc32;
                fileInfo.compression = compression;
            }

            mz_zip_reader_end(&zipArchive);
        }

        impl->endIterator = { std::make_shared<FileSystemZipIterator>(impl->fileInfoMap.end()) };
    }

    FileSystemZip::~FileSystemZip() {
        delete impl;
    }

    FileSystem::Iterator FileSystemZip::begin() const {
        return { std::make_shared<FileSystemZipIterator>(impl->fileInfoMap.begin()) };
    }
    
    FileSystem::Iterator FileSystemZip::end() const {
        return { impl->endIterator };
    }
    
    bool FileSystemZip::load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const {
        assert(impl->archiveOpen);

        auto it = impl->fileInfoMap.find(path);
        if (it == impl->fileInfoMap.end()) {
            return false;
        }

        // Must have enough bytes to read the entire file.
        if (fileDataMaxByteCount < it->second.uncompressedSize) {
            return false;
        }

        // Validate the local dir header.
        const char *localDirHeader = reinterpret_cast<const char *>(&impl->zipMappedFile.data()[it->second.localHeaderOffset]);
        if (MZ_READ_LE32(localDirHeader) != MZ_ZIP_LOCAL_DIR_HEADER_SIG) {
            return false;
        }

        // Skip over unused data of the header.
        uint32_t ldhFilenameLenOfs = MZ_READ_LE16(localDirHeader + MZ_ZIP_LDH_FILENAME_LEN_OFS);
        uint32_t ldhExtraLenOfs = MZ_READ_LE16(localDirHeader + MZ_ZIP_LDH_EXTRA_LEN_OFS);
        size_t dataAddress = it->second.localHeaderOffset + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + ldhFilenameLenOfs + ldhExtraLenOfs;

        // Make sure output vector has enough bytes to hold the data.
        const uint8_t *zipFileData = reinterpret_cast<const uint8_t *>(&impl->zipMappedFile.data()[dataAddress]);
        if (it->second.compression == FileSystemZipInfo::Compression::Zstd) {
            if (ZSTD_decompress(fileData, it->second.uncompressedSize, zipFileData, it->second.compressedSize) != it->second.uncompressedSize) {
                return false;
            }
        }
        else if (it->second.compression == FileSystemZipInfo::Compression::Deflate) {
            if (tinfl_decompress_mem_to_mem(fileData, it->second.uncompressedSize, zipFileData, it->second.compressedSize, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
                return false;
            }
        }
        else {
            memcpy(fileData, zipFileData, it->second.uncompressedSize);
        }

        return true;
    }

    size_t FileSystemZip::getSize(const std::string &path) const {
        assert(impl->archiveOpen);
        auto it = impl->fileInfoMap.find(path);
        if (it != impl->fileInfoMap.end()) {
            return it->second.uncompressedSize;
        }
        else {
            return 0;
        }
    }
    
    bool FileSystemZip::exists(const std::string &path) const {
        assert(impl->archiveOpen);

        auto it = impl->fileInfoMap.find(path);
        return (it != impl->fileInfoMap.end());
    }

    std::string FileSystemZip::makeCanonical(const std::string &path) const {
        // Has no effect on Zip files which are already assumed to be validated for case sensitivity.
        return path;
    }

    bool FileSystemZip::isOpen() const {
        return impl->archiveOpen;
    }

    std::unique_ptr<FileSystem> FileSystemZip::create(const std::filesystem::path &zipPath, const std::string &basePath) {
        std::unique_ptr<FileSystemZip> zipFileSystem = std::make_unique<FileSystemZip>(zipPath, basePath);
        if (!zipFileSystem->isOpen()) {
            zipFileSystem.reset();
        }

        return zipFileSystem;
    }

    // FileSystemZipIterator

    FileSystemZipIterator::FileSystemZipIterator(std::unordered_map<std::string, FileSystemZipInfo>::iterator iterator) {
        this->iterator = iterator;
    }

    const std::string &FileSystemZipIterator::value() {
        return iterator->first;
    }

    void FileSystemZipIterator::increment() {
        iterator++;
    }

    bool FileSystemZipIterator::compare(const FileSystemIteratorImplementation *other) const {
        const FileSystemZipIterator *otherZipIt = static_cast<const FileSystemZipIterator *>(other);
        return (iterator == otherZipIt->iterator);
    }
};