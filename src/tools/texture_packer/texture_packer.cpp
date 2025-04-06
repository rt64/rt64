//
// RT64
//

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

#include <ddspp/ddspp.h>
#include <miniz/miniz.h>
#include <plainargs/plainargs.h>
#include <zstd.h>

#include "../../common/rt64_replacement_database.cpp"

enum {
    MZ_ZIP_LDH_METHOD_OFS = 8,
    MZ_ZIP_CDH_METHOD_OFS = 10,
    MZ_ZSTD = 93,
};

static const uint32_t TextureDataPitchAlignment = 256;
static const uint32_t TextureDataPlacementAlignment = 512;

static uint32_t nextSizeAlignedTo(uint32_t size, uint32_t alignment) {
    if (size % alignment) {
        return size + (alignment - (size % alignment));
    }
    else {
        return size;
    }
}

struct CompressionInput {
    std::string zipPath;
    std::filesystem::path filePath;
    bool preferDeflateOverZstd = false;
};

struct CompressionOutput {
    std::string zipPath;
    std::vector<uint8_t> fileData;
    uint32_t uncompressedSize = 0;
    uint32_t checksum = 0;
    mz_uint16 compressionMethod = 0;
};

std::queue<CompressionInput> inputQueue;
std::queue<CompressionOutput> outputQueue;
std::mutex inputQueueMutex;
std::mutex outputQueueMutex;
std::condition_variable outputQueueChanged;
std::atomic<bool> compressionFailed;
std::atomic<bool> useCompression;
std::atomic<bool> useZstd;

void compressionThread() {
    std::vector<uint8_t> fileData;
    while (!compressionFailed) {
        CompressionInput input;
        {
            std::unique_lock lock(inputQueueMutex);
            if (!inputQueue.empty()) {
                input = std::move(inputQueue.front());
                inputQueue.pop();
            }
            else {
                break;
            }
        }

        std::ifstream fileStream(input.filePath, std::ios::binary);
        if (!fileStream.is_open()) {
            std::string filePathStr = input.filePath.u8string();
            fprintf(stderr, "Failed to open %s.\n", filePathStr.c_str());
            compressionFailed = true;
            break;
        }

        fileStream.seekg(0, std::ios::end);
        fileData.resize(fileStream.tellg());
        fileStream.seekg(0, std::ios::beg);
        fileStream.read((char *)(fileData.data()), fileData.size());

        if (fileStream.bad()) {
            fprintf(stderr, "Failed to read data for %s.\n", input.zipPath.c_str());
            compressionFailed = true;
            break;
        }

        CompressionOutput output;
        output.fileData.resize(fileData.size());

        mz_uint16 compressionMethod = 0;
        size_t compressedSize = 0;
        if (useCompression) {
            if (useZstd && !input.preferDeflateOverZstd) {
                // Max compression level has shown significant advantages in size reduction.
                compressedSize = ZSTD_compress(output.fileData.data(), output.fileData.size(), fileData.data(), fileData.size(), ZSTD_maxCLevel());
                compressionMethod = MZ_ZSTD;

                // ZSTD uses a special error code to indicate compression has failed.
                if (ZSTD_isError(compressedSize)) {
                    compressedSize = 0;
                }
            }
            else {
                // Probe number extracted from miniz's compression level 10.
                int flags = 768;
                compressedSize = tdefl_compress_mem_to_mem(output.fileData.data(), output.fileData.size(), fileData.data(), fileData.size(), flags);
                compressionMethod = MZ_DEFLATED;
            }
        }

        if (compressedSize > 0) {
            output.fileData.resize(compressedSize);
            output.compressionMethod = compressionMethod;
        }
        else {
            memcpy(output.fileData.data(), fileData.data(), output.fileData.size());
        }

        output.zipPath = input.zipPath;
        output.uncompressedSize = fileData.size();
        output.checksum = mz_crc32(MZ_CRC32_INIT, fileData.data(), fileData.size());

        {
            std::unique_lock lock(outputQueueMutex);
            outputQueue.emplace(std::move(output));
        }

        outputQueueChanged.notify_all();
    }

    if (compressionFailed) {
        outputQueueChanged.notify_all();
    }
}

void showHelp() {
    fprintf(stdout,
        "texture_packer <path> --create-low-mip-cache\n"
        "\tGenerate the cache used for streaming textures in by extracting the lowest quality mipmaps.\n\n"
        "texture_packer <path> --create-pack [--deflate] [--store] [--threads number]\n"
        "\tCreate the pack by including all the textures supported by the database and the low mip cache.\n"
        "\tUse '--deflate' to downgrade the compression algorithm and make the resulting package compatible\n"
        "\twith more third-party zip software. This is not recommended as load times will be worse.\n"
        "\tUse '--store' to disable compression entirely. Loading times may be better or worse depending on"
        "\tthe speed of the storage where the pack is loaded from.\n"
        "\tUse '--threads number' to specify the amount of compression threads. By default, the tool will\n"
        "\tuse all threads of the system available.\n"
        "\t\n"
    );
}

bool extractLowMipsToStream(const std::filesystem::path &directoryPath, const std::string &relativePath, std::ofstream &lowMipCacheStream) {
    std::ifstream ddsStream(directoryPath / std::filesystem::u8path(relativePath), std::ios::binary);
    if (!ddsStream.is_open() && !ddsStream.bad()) {
        return false;
    }

    uint8_t headerBytes[ddspp::MAX_HEADER_SIZE] = {};
    ddsStream.read(reinterpret_cast<char *>(headerBytes), ddspp::MAX_HEADER_SIZE);

    ddspp::Descriptor ddsDescriptor;
    ddspp::Result result = ddspp::decode_header(headerBytes, ddsDescriptor);
    if (result != ddspp::Success) {
        return false;
    }

    // Search for the lowest mipmap to start extracting from.
    const uint32_t minPixelCount = 96 * 96;
    uint32_t mipStart = 0;
    while (mipStart < (ddsDescriptor.numMips - 1)) {
        const uint32_t mipPixelCount = (ddsDescriptor.width >> mipStart) * (ddsDescriptor.height >> mipStart);
        if (mipPixelCount <= minPixelCount) {
            break;
        }

        mipStart++;
    }

    // Get the block size for the DDS.
    uint32_t blockWidth, blockHeight;
    ddspp::get_block_size(ddsDescriptor.format, blockWidth, blockHeight);

    // Write out the cache header.
    RT64::ReplacementMipmapCacheHeader cacheHeader;
    cacheHeader.width = std::max(ddsDescriptor.width >> mipStart, 1U);
    cacheHeader.height = std::max(ddsDescriptor.height >> mipStart, 1U);
    cacheHeader.dxgiFormat = ddsDescriptor.format;
    cacheHeader.mipCount = ddsDescriptor.numMips - mipStart;
    cacheHeader.pathLength = relativePath.size();

    // Force texture's dimensions to be aligned to the block width and height.
    if ((cacheHeader.width % blockWidth) || (cacheHeader.height % blockHeight)) {
        cacheHeader.width = ((cacheHeader.width + blockWidth - 1) / blockWidth) * blockWidth;
        cacheHeader.height = ((cacheHeader.height + blockHeight - 1) / blockHeight) * blockHeight;
    }

    lowMipCacheStream.write(reinterpret_cast<const char *>(&cacheHeader), sizeof(cacheHeader));

    // Compute the size of each mipmap and write it out.
    thread_local std::vector<uint32_t> mipmapSizes;
    thread_local std::vector<uint32_t> mipmapRowPitches;
    thread_local std::vector<uint32_t> mipmapAlignedRowPitches;
    thread_local std::vector<uint32_t> mipmapRowCounts;
    mipmapSizes.clear();
    mipmapRowPitches.clear();
    mipmapAlignedRowPitches.clear();
    mipmapRowCounts.clear();

    for (uint32_t i = 0; i < cacheHeader.mipCount; i++) {
        uint32_t mipHeight = std::max(cacheHeader.height >> i, 1U);
        uint32_t rowPitch = ddspp::get_row_pitch(ddsDescriptor, mipStart + i);
        uint32_t alignedRowPitch = nextSizeAlignedTo(rowPitch, TextureDataPitchAlignment);
        uint32_t rowCount = (mipHeight + blockHeight - 1) / blockHeight;
        uint32_t mipSize = rowCount * alignedRowPitch;
        mipmapSizes.emplace_back(mipSize);
        mipmapRowPitches.emplace_back(rowPitch);
        mipmapAlignedRowPitches.emplace_back(alignedRowPitch);
        mipmapRowCounts.emplace_back(rowCount);
    }

    lowMipCacheStream.write(reinterpret_cast<const char *>(mipmapSizes.data()), sizeof(uint32_t) * cacheHeader.mipCount);
    lowMipCacheStream.write(reinterpret_cast<const char *>(mipmapAlignedRowPitches.data()), sizeof(uint32_t) * cacheHeader.mipCount);
    lowMipCacheStream.write(relativePath.c_str(), relativePath.size());

    thread_local std::vector<char> fileBytes;
    thread_local std::vector<char> zeroBytes;
    for (uint32_t i = 0; i < cacheHeader.mipCount; i++) {
        // Add padding until the placement alignment condition is met.
        while ((lowMipCacheStream.tellp() % TextureDataPlacementAlignment) != 0) {
            lowMipCacheStream.put(0);
        }

        const uint32_t rowPitch = mipmapRowPitches[i];
        const uint32_t alignedRowPitch = mipmapAlignedRowPitches[i];
        if (alignedRowPitch > rowPitch) {
            fileBytes.resize(rowPitch);
            
            const uint32_t rowCount = mipmapRowCounts[i];
            const uint32_t paddingByteCount = alignedRowPitch - rowPitch;
            zeroBytes.resize(paddingByteCount, 0);

            for (uint32_t j = 0; j < rowCount; j++) {
                uint32_t ddsOffset = ddsDescriptor.headerSize + ddspp::get_offset(ddsDescriptor, mipStart + i, 0) + j * rowPitch;
                ddsStream.seekg(ddsOffset);
                ddsStream.read(fileBytes.data(), rowPitch);
                lowMipCacheStream.write(fileBytes.data(), rowPitch);
                lowMipCacheStream.write(zeroBytes.data(), paddingByteCount);
            }
        }
        else {
            uint32_t ddsOffset = ddsDescriptor.headerSize + ddspp::get_offset(ddsDescriptor, mipStart + i, 0);
            fileBytes.resize(mipmapSizes[i]);
            ddsStream.seekg(ddsOffset);
            ddsStream.read(fileBytes.data(), mipmapSizes[i]);
            lowMipCacheStream.write(fileBytes.data(), mipmapSizes[i]);
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    enum class Mode {
        Unknown,
        CreateLowMipCache,
        CreatePack
    };

    plainargs::Result args = plainargs::parse(argc, argv);
    if (args.getArgumentCount() < 1) {
        showHelp();
        return 1;
    }

    // First argument is always expected to be the search directory.
    std::filesystem::path searchDirectory(args.getArgument(0));
    if (!std::filesystem::is_directory(searchDirectory)) {
        std::string u8string = searchDirectory.u8string();
        fprintf(stderr, "The directory %s does not exist.", u8string.c_str());
        return 1;
    }

    Mode mode = Mode::Unknown;
    if (args.hasOption("create-low-mip-cache", "m")) {
        fprintf(stdout, "Creating low mip cache.\n");
        mode = Mode::CreateLowMipCache;
    }
    else if (args.hasOption("create-pack", "p")) {
        fprintf(stdout, "Creating pack.\n");
        mode = Mode::CreatePack;
    }
    else {
        fprintf(stderr, "No operation mode was specified.\n");
        showHelp();
        return 1;
    }

    RT64::ReplacementDatabase database;
    std::filesystem::path databasePath = searchDirectory / RT64::ReplacementDatabaseFilename;
    if (!std::filesystem::exists(databasePath)) {
        fprintf(stderr, "Database file %s is missing.\n", RT64::ReplacementDatabaseFilename.c_str());
        return 1;
    }

    fprintf(stdout, "Opening database file...\n");

    std::ifstream databaseStream(databasePath);
    if (databaseStream.is_open()) {
        try {
            json jroot;
            databaseStream >> jroot;
            database = jroot;
            if (databaseStream.bad()) {
                std::string u8string = databasePath.u8string();
                fprintf(stderr, "Failed to read database file at %s.", u8string.c_str());
                return 1;
            }
        }
        catch (const nlohmann::detail::exception &e) {
            fprintf(stderr, "JSON parsing error: %s\n", e.what());
            return 1;
        }

        databaseStream.close();
    }

    fprintf(stdout, "Resolving database paths...\n");

    // Resolve all paths for the database and build a unique set of files.
    std::set<std::string> resolvedPathSet;
    std::vector<uint64_t> hashesMissing;
    std::unordered_map<uint64_t, RT64::ReplacementResolvedPath> resolvedPathMap;
    std::unique_ptr<RT64::FileSystem> fileSystem = RT64::FileSystemDirectory::create(searchDirectory);
    database.resolvePaths(fileSystem.get(), resolvedPathMap, mode == Mode::CreateLowMipCache, &hashesMissing);

    for (auto it : resolvedPathMap) {
        if ((mode != Mode::CreateLowMipCache) || (it.second.operation == RT64::ReplacementOperation::Stream)) {
            resolvedPathSet.insert(it.second.relativePath);
        }
    }

    if (!hashesMissing.empty()) {
        fprintf(stderr, "The following files are specified in the database but are missing from the directory.\n\n");

        for (uint64_t hash : hashesMissing) {
            fprintf(stderr, "rt64: %016" PRIx64 " path: %s\n", hash, database.getReplacement(hash).path.c_str());
        }

        return 1;
    }

    if (mode == Mode::CreateLowMipCache) {
        std::filesystem::path lowMipCachePath = searchDirectory / RT64::ReplacementLowMipCacheFilename;
        std::ofstream lowMipCacheStream(lowMipCachePath, std::ios::binary);
        if (!lowMipCacheStream.is_open()) {
            std::string u8string = lowMipCachePath.u8string();
            fprintf(stderr, "Failed to open low mip cache file at %s for writing.", u8string.c_str());
            return 1;
        }

        uint32_t processCount = 0;
        uint32_t processTotal = resolvedPathSet.size();
        for (auto it : resolvedPathSet) {
            if (!extractLowMipsToStream(searchDirectory, it, lowMipCacheStream)) {
                fprintf(stderr, "Failed to extract low mip to cache from file %s.", it.c_str());
                return 1;
            }

            processCount++;

            if ((processCount % 100) == 0 || (processCount == processTotal)) {
                fprintf(stdout, "Processing (%d/%d): %s.\n", processCount, processTotal, it.c_str());
            }
        }
    }
    else if (mode == Mode::CreatePack) {
        uint32_t threadCount = 0;
        std::string threadsValue = args.getValue("threads", "t");
        if (threadsValue.empty()) {
            threadCount = std::max(std::thread::hardware_concurrency() - 1U, 1U);
        }
        else {
            threadCount = std::stoi(threadsValue);
        }

        compressionFailed = false;

        if (args.hasOption("deflate", "d")) {
            fprintf(stdout, "Using deflate for compression.\n");
            useZstd = false;
            useCompression = true;
        }
        else if (args.hasOption("store", "d")) {
            fprintf(stdout, "Disabling compression.\n");
            useZstd = false;
            useCompression = false;
        }
        else {
            fprintf(stdout, "Using zstd for compression.\n");
            useZstd = true;
            useCompression = true;
        }

        // Perform some file case validation before creating the pack.
        fprintf(stdout, "Validating database files...\n");

        bool failedCaseValidation = false;
        for (auto it : resolvedPathMap) {
            std::string dbPath = RT64::ReplacementDatabase::removeKnownExtension(database.getReplacement(it.first).path);
            dbPath = RT64::FileSystem::toForwardSlashes(dbPath);
            if (dbPath.empty()) {
                continue;
            }

            std::string filePath = RT64::ReplacementDatabase::removeKnownExtension(it.second.relativePath);
            if (filePath != dbPath) {
                fprintf(stderr, "The path %s in the database is different from %s in the directory (case sensitivity in Windows).\n", dbPath.c_str(), filePath.c_str());
                failedCaseValidation = true;
            }
        }
        
        if (failedCaseValidation) {
            fprintf(stderr, 
                "Pack was not created due to validation errors.\n"
                "Please fix any validation errors for the pack to work correctly on all platforms.\n"
                "Case sensitivity errors can be fixed by editing the database manually.\n");

            return 1;
        }

        std::filesystem::path packName = searchDirectory.has_filename() ? searchDirectory.filename() : "rt64";
        packName += std::filesystem::u8path(RT64::ReplacementPackExtension);

        std::string packNameStr = packName.u8string();
        fprintf(stdout, "Creating pack file with name %s...\n", packNameStr.c_str());

        std::filesystem::path packPath = searchDirectory / packName;
        std::string packPathStr = packPath.u8string();
        mz_zip_archive zipArchive = {};
        if (!mz_zip_writer_init_file_v2(&zipArchive, packPathStr.c_str(), 0, MZ_ZIP_FLAG_WRITE_ZIP64)) {
            fprintf(stderr, "Failed to open %s for writing.\n", packPathStr.c_str());
            return 1;
        }

        uint32_t outputQueueTotal = 0;
        auto addToQueue = [&](const std::string &file, bool preferDeflateOverZstd = false) {
            CompressionInput input;
            input.filePath = searchDirectory / std::filesystem::u8path(file);
            input.zipPath = file;
            input.preferDeflateOverZstd = preferDeflateOverZstd;
            inputQueue.emplace(input);
            outputQueueTotal++;
        };

        // Add all resolved files to queue.
        for (auto it : resolvedPathSet) {
            addToQueue(it);
        }

        // Add extra files to queue.
        for (auto it : database.extraFiles) {
            addToQueue(it, true);
        }

        // Add database file to queue.
        addToQueue(RT64::ReplacementDatabaseFilename);

        // Add low mip cache if it exists. Warn user that the file does not exist.
        if (std::filesystem::exists(searchDirectory / std::filesystem::u8path(RT64::ReplacementLowMipCacheFilename))) {
            addToQueue(RT64::ReplacementLowMipCacheFilename);
        }
        else {
            fprintf(stderr, "Unable to find %s. Make sure to generate this file using --create-low-mip-cache before creating the texture pack so DDS files can have proper streaming.\n", RT64::ReplacementLowMipCacheFilename.c_str());
            compressionFailed = true;
        }

        if (!compressionFailed) {
            // Create all worker threads.
            std::list<std::unique_ptr<std::thread>> compressionThreads;
            for (uint32_t i = 0; i < threadCount; i++) {
                compressionThreads.emplace_back(std::make_unique<std::thread>(&compressionThread));
            }

            uint32_t outputQueueProcessed = 0;
            while ((outputQueueProcessed < outputQueueTotal) && !compressionFailed) {
                CompressionOutput output;

                {
                    std::unique_lock lock(outputQueueMutex);
                    outputQueueChanged.wait(lock, []() {
                        return !outputQueue.empty() || compressionFailed;
                        });

                    output = std::move(outputQueue.front());
                    outputQueue.pop();
                }

                if (!output.zipPath.empty()) {
                    mz_uint flags = useCompression ? MZ_ZIP_FLAG_COMPRESSED_DATA : 0;
                    if (!mz_zip_writer_add_mem_ex_v2(&zipArchive, output.zipPath.c_str(), output.fileData.data(), output.fileData.size(), nullptr, 0, flags, useCompression ? output.uncompressedSize : 0, output.checksum, nullptr, nullptr, 0, nullptr, 0, output.compressionMethod)) {
                        fprintf(stderr, "Failed to add %s to pack.\n", output.zipPath.c_str());
                        compressionFailed = true;
                    }

                    outputQueueProcessed++;
                    if ((outputQueueProcessed % 100) == 0 || (outputQueueProcessed == outputQueueTotal)) {
                        fprintf(stdout, "Processed (%d/%d): %s.\n", outputQueueProcessed, outputQueueTotal, output.zipPath.c_str());
                    }
                }
            }

            for (auto &thread : compressionThreads) {
                thread->join();
                thread.reset();
            }
        }

        if (!compressionFailed) {
            if (!mz_zip_writer_finalize_archive(&zipArchive)) {
                fprintf(stderr, "Failed to finalize archive %s.\n", packPathStr.c_str());
                compressionFailed = true;
            }
        }

        if (!mz_zip_writer_end(&zipArchive)) {
            fprintf(stderr, "Failed to close %s for writing.\n", packPathStr.c_str());
            compressionFailed = true;
        }

        if (compressionFailed) {
            std::filesystem::remove(packPath);
            fprintf(stderr, "Pack creation has failed.\n");
            return 1;
        }
    }

    return 0;
}
