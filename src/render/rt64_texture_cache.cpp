//
// RT64
//

#define STB_IMAGE_IMPLEMENTATION

#include "ddspp/ddspp.h"
#include "stb/stb_image.h"
#include "xxHash/xxh3.h"

#include "gbi/rt64_f3d.h"
#include "common/rt64_load_types.h"
#include "common/rt64_thread.h"
#include "common/rt64_tmem_hasher.h"
#include "hle/rt64_workload_queue.h"

#include "rt64_texture_cache.h"

namespace RT64 {
    // ReplacementMap

    static const interop::float2 IdentityScale = { 1.0f, 1.0f };
    static const std::string ReplacementDatabaseFilename = "rt64.json";

    ReplacementMap::ReplacementMap() {
        // Empty constructor.
    }

    ReplacementMap::~ReplacementMap() {
        for (Texture *texture : loadedTextures) {
            delete texture;
        }

        for (Texture *texture : evictedTextures) {
            delete texture;
        }
    }

    void ReplacementMap::clear() {
        evictedTextures.insert(evictedTextures.end(), loadedTextures.begin(), loadedTextures.end());
        loadedTextures.clear();
        evictedTextures.clear();
        pathHashToLoadMap.clear();
    }

    bool ReplacementMap::readDatabase(std::istream &stream) {
        try {
            json jroot;
            stream >> jroot;
            db = jroot;
            if (!stream.bad()) {
                return true;
            }
        }
        catch (const nlohmann::detail::exception &e) {
            fprintf(stderr, "JSON parsing error: %s\n", e.what());
            db = ReplacementDatabase();
        }

        return false;
    }

    bool ReplacementMap::saveDatabase(std::ostream &stream) {
        try {
            json jroot = db;
            stream << std::setw(4) << jroot << std::endl;
            return !stream.bad();
        }
        catch (const nlohmann::detail::exception &e) {
            fprintf(stderr, "JSON writing error: %s\n", e.what());
            return false;
        }
    }

    void ReplacementMap::resolveAutoPaths() {
        auto toLower = [](std::string str) {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
            return str;
        };

        // Scan all possible candidates on the filesystem first.
        std::unordered_map<std::string, std::string> ricePathMap;
        for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string fileExtension = toLower(entry.path().extension().u8string());
                if ((fileExtension != ".dds") && (fileExtension != ".png")) {
                    continue;
                }

                if (db.config.autoPath == ReplacementAutoPath::Rice) {
                    std::string fileName = entry.path().filename().u8string();
                    size_t firstHashSymbol = fileName.find_first_of("#");
                    size_t lastUnderscoreSymbol = fileName.find_last_of("_");
                    if ((firstHashSymbol != std::string::npos) && (lastUnderscoreSymbol != std::string::npos) && (lastUnderscoreSymbol > firstHashSymbol)) {
                        std::string riceHash = toLower(fileName.substr(firstHashSymbol + 1, lastUnderscoreSymbol - firstHashSymbol - 1));
                        ricePathMap[riceHash] = std::filesystem::relative(entry.path(), directoryPath).u8string();
                    }
                }
            }
        }

        // Clear any existing automatic assignments.
        autoPathMap.clear();

        // Look for possible matches of all current replacements that don't have a path assigned.
        for (const ReplacementTexture &texture : db.textures) {
            if (texture.path.empty()) {
                if (db.config.autoPath == ReplacementAutoPath::Rice) {
                    auto it = ricePathMap.find(texture.hashes.rice);
                    if (it != ricePathMap.end()) {
                        uint64_t rt64 = ReplacementDatabase::stringToHash(texture.hashes.rt64);
                        autoPathMap[rt64] = it->second;
                    }
                }
            }
        }
    }

    void ReplacementMap::removeUnusedEntriesFromDatabase() {
        std::vector<ReplacementTexture> newTextures;
        for (const ReplacementTexture &texture : db.textures) {
            if (texture.path.empty()) {
                uint64_t rt64 = ReplacementDatabase::stringToHash(texture.hashes.rt64);
                auto pathIt = autoPathMap.find(rt64);
                if (pathIt == autoPathMap.end()) {
                    continue;
                }

                if (pathIt->second.empty()) {
                    continue;
                }
            }

            newTextures.emplace_back(texture);
        }

        db.textures = newTextures;
        db.buildHashMaps();
    }

    std::string ReplacementMap::getRelativePathFromHash(uint64_t tmemHash) const {
        auto pathIt = autoPathMap.find(tmemHash);
        if (pathIt != autoPathMap.end()) {
            return pathIt->second;
        }

        auto replaceIt = db.tmemHashToReplaceMap.find(tmemHash);
        if (replaceIt != db.tmemHashToReplaceMap.end()) {
            return db.textures[replaceIt->second].path;
        }

        return std::string();
    }

    Texture *ReplacementMap::getFromRelativePath(const std::string &relativePath) const {
        uint64_t pathHash = hashFromRelativePath(relativePath);
        auto it = pathHashToLoadMap.find(pathHash);
        if (it != pathHashToLoadMap.end()) {
            return loadedTextures[it->second];
        }
        else {
            return nullptr;
        }
    }

    Texture *ReplacementMap::loadFromBytes(RenderWorker *worker, uint64_t hash, const std::string &relativePath, const std::vector<uint8_t> &fileBytes, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *resourcePool, uint32_t minMipWidth, uint32_t minMipHeight) {
        uint64_t pathHash = hashFromRelativePath(relativePath);
        if (pathHashToLoadMap.find(pathHash) != pathHashToLoadMap.end()) {
            assert(false && "Can't add a replacement texture with an existing hash.");
            return nullptr;
        }

        const uint32_t PNG_MAGIC = 0x474E5089;
        Texture *replacementTexture = new Texture();
        uint32_t magicNumber = *reinterpret_cast<const uint32_t *>(fileBytes.data());
        bool loadedTexture = false;
        switch (magicNumber) {
        case ddspp::DDS_MAGIC:
            loadedTexture = TextureCache::setDDS(replacementTexture, worker, fileBytes.data(), fileBytes.size(), dstUploadResource, resourcePool, minMipWidth, minMipHeight);
            break;
        case PNG_MAGIC: {
            int width, height;
            stbi_uc *data = stbi_load_from_memory(fileBytes.data(), fileBytes.size(), &width, &height, nullptr, 4);
            if (data != nullptr) {
                uint32_t rowPitch = uint32_t(width) * 4;
                size_t byteCount = uint32_t(height) * rowPitch;
                TextureCache::setRGBA32(replacementTexture, worker, data, byteCount, uint32_t(width), uint32_t(height), rowPitch, dstUploadResource, resourcePool);
                stbi_image_free(data);
                loadedTexture = true;
            }

            break;
        }
        default:
            // Unknown format.
            break;
        }

        if (loadedTexture) {
            pathHashToLoadMap[pathHash] = uint32_t(loadedTextures.size());
            loadedTextures.emplace_back(replacementTexture);
            return replacementTexture;
        }
        else {
            delete replacementTexture;
            return nullptr;
        }
    }

    uint64_t ReplacementMap::hashFromRelativePath(const std::string &relativePath) const {
        return XXH3_64bits(relativePath.data(), relativePath.size());
    }

    // TextureMap

    TextureMap::TextureMap() {
        globalVersion = 0;
        replacementMapEnabled = true;
    }

    TextureMap::~TextureMap() {
        for (Texture *texture : textures) {
            delete texture;
        }

        for (Texture *texture : evictedTextures) {
            delete texture;
        }
    }

    void TextureMap::clearReplacements() {
        for (size_t i = 0; i < textureReplacements.size(); i++) {
            if (textureReplacements[i] != nullptr) {
                textureReplacements[i] = nullptr;
                versions[i]++;
            }
        }

        globalVersion++;
    }

    void TextureMap::add(uint64_t hash, uint64_t creationFrame, Texture *texture) {
        assert(hashMap.find(hash) == hashMap.end());

        // Check for free spaces on the LIFO queue first.
        uint32_t textureIndex;
        if (!freeSpaces.empty()) {
            textureIndex = freeSpaces.back();
            freeSpaces.pop_back();
        }
        else {
            textureIndex = static_cast<uint32_t>(textures.size());
            textures.push_back(nullptr);
            textureReplacements.push_back(nullptr);
            textureScales.push_back(IdentityScale);
            hashes.push_back(0);
            versions.push_back(0);
            creationFrames.push_back(0);
            listIterators.push_back(accessList.end());
        }

        hashMap[hash] = textureIndex;
        textures[textureIndex] = texture;
        textureReplacements[textureIndex] = nullptr;
        textureScales[textureIndex] = IdentityScale;
        hashes[textureIndex] = hash;
        versions[textureIndex]++;
        creationFrames[textureIndex] = creationFrame;
        globalVersion++;

        accessList.push_front({ textureIndex, creationFrame });
        listIterators[textureIndex] = accessList.begin();
    }

    void TextureMap::replace(uint64_t hash, Texture *texture) {
        const auto it = hashMap.find(hash);
        if (it == hashMap.end()) {
            return;
        }

        Texture *replacedTexture = textures[it->second];
        textureReplacements[it->second] = texture;
        textureScales[it->second] = { float(texture->width) / float(replacedTexture->width), float(texture->height) / float(replacedTexture->height) };
        versions[it->second]++;
        globalVersion++;
    }

    bool TextureMap::use(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, bool &textureReplaced, bool &hasMipmaps) {
        // Find the matching texture index in the hash map.
        const auto it = hashMap.find(hash);
        if (it == hashMap.end()) {
            textureIndex = 0;
            textureScale = IdentityScale;
            return false;
        }

        textureIndex = it->second;
        textureReplaced = replacementMapEnabled && (textureReplacements[textureIndex] != nullptr);

        if (textureReplaced) {
            textureScale = textureScales[textureIndex];
            hasMipmaps = (textureReplacements[textureIndex]->mipmaps > 1);
        }
        else {
            textureScale = IdentityScale;
            hasMipmaps = false;
        }

        // Remove the existing entry from the list if it exists.
        AccessList::iterator listIt = listIterators[textureIndex];
        if (listIt != accessList.end()) {
            accessList.erase(listIt);
        }

        // Push a new access entry to the front of the list and store the new iterator.
        accessList.push_front({ textureIndex, submissionFrame });
        listIterators[textureIndex] = accessList.begin();
        return true;
    }

    bool TextureMap::evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes) {
        evictedHashes.clear();

        auto it = accessList.rbegin();
        while (it != accessList.rend()) {
            assert(submissionFrame >= it->second);

            // The max age allowed is the difference between the last time the texture was used and the time it was uploaded.
            // Ensure the textures live long enough for the frame queue to use them.
            const uint64_t MinimumMaxAge = WORKLOAD_QUEUE_SIZE * 2;
            const uint64_t age = submissionFrame - it->second;
            const uint64_t maxAge = std::max(it->second - creationFrames[it->first], MinimumMaxAge);

            // Evict all entries that are present in the access list and are older than the frame by the specified margin.
            if (age >= maxAge) {
                const uint32_t textureIndex = it->first;
                const uint64_t textureHash = hashes[textureIndex];
                evictedTextures.emplace_back(textures[textureIndex]);
                textures[textureIndex] = nullptr;
                textureScales[textureIndex] = { 1.0f, 1.0f };
                textureReplacements[textureIndex] = nullptr;
                hashes[textureIndex] = 0;
                creationFrames[textureIndex] = 0;
                freeSpaces.push_back(textureIndex);
                listIterators[textureIndex] = accessList.end();
                hashMap.erase(textureHash);
                evictedHashes.push_back(textureHash);
                it = decltype(it)(accessList.erase(std::next(it).base()));
            }
            // Stop iterating if we reach an entry that has been used in the present.
            else if (age == 0) {
                break;
            }
            else {
                it++;
            }
        }

        return !evictedHashes.empty();
    }

    Texture *TextureMap::get(uint32_t index) const {
        assert(index < textures.size());
        return textures[index];
    }

    size_t TextureMap::getMaxIndex() const {
        return textures.size();
    }

    // TextureCache

    TextureCache::TextureCache(RenderWorker *worker, const ShaderLibrary *shaderLibrary, bool developerMode) {
        assert(worker != nullptr);

        this->worker = worker;
        this->shaderLibrary = shaderLibrary;
        this->developerMode = developerMode;

        lockCounter = 0;
        uploadThread = new std::thread(&TextureCache::uploadThreadLoop, this);

        RenderPoolDesc poolDesc;
        poolDesc.heapType = RenderHeapType::UPLOAD;
        poolDesc.useLinearAlgorithm = true;
        poolDesc.allowOnlyBuffers = true;
        threadResourcePool = worker->device->createPool(poolDesc);
    }

    TextureCache::~TextureCache() {
        if (uploadThread != nullptr) {
            uploadThreadRunning = false;
            uploadQueueChanged.notify_all();
            uploadThread->join();
            delete uploadThread;
        }
        
        descriptorSets.clear();
        tmemUploadResources.clear();
        replacementUploadResources.clear();
        threadResourcePool.reset(nullptr);
    }
    
    void TextureCache::setRGBA32(Texture *dstTexture, RenderWorker *worker, const uint8_t *bytes, size_t byteCount, uint32_t width, uint32_t height, uint32_t rowPitch, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool) {
        assert(dstTexture != nullptr);
        assert(worker != nullptr);
        assert(bytes != nullptr);
        assert(width > 0);
        assert(height > 0);

        dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
        dstTexture->width = width;
        dstTexture->height = height;
        dstTexture->mipmaps = 1;

        // Calculate the minimum row width required to store the texture.
        uint32_t rowByteWidth, rowBytePadding;
        CalculateTextureRowWidthPadding(rowPitch, rowByteWidth, rowBytePadding);

        dstTexture->texture = worker->device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, dstTexture->format));

        if (uploadResourcePool != nullptr) {
            dstUploadResource = uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(rowByteWidth * height));
        }
        else {
            dstUploadResource = worker->device->createBuffer(RenderBufferDesc::UploadBuffer(rowByteWidth * height));
        }

        uint8_t *dstData = reinterpret_cast<uint8_t *>(dstUploadResource->map());
        if (rowBytePadding == 0) {
            memcpy(dstData, bytes, byteCount);
        }
        else {
            const uint8_t *srcData = reinterpret_cast<const uint8_t *>(bytes);
            size_t offset = 0;
            while ((offset + size_t(rowPitch)) <= byteCount) {
                memcpy(dstData, srcData, rowPitch);
                srcData += rowPitch;
                offset += rowPitch;
                dstData += rowByteWidth;
            }
        }

        dstUploadResource->unmap();

        uint32_t rowWidth = rowByteWidth / RenderFormatSize(dstTexture->format);
        worker->commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::COPY_DEST));
        worker->commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(dstTexture->texture.get()), RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), dstTexture->format, width, height, 1, rowWidth));
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::SHADER_READ));
    }

    static RenderTextureDimension toRenderDimension(ddspp::TextureType type) {
        switch (type) {
        case ddspp::Texture1D:
            return RenderTextureDimension::TEXTURE_1D;
        case ddspp::Texture2D:
            return RenderTextureDimension::TEXTURE_2D;
        case ddspp::Texture3D:
            return RenderTextureDimension::TEXTURE_3D;
        default:
            assert(false && "Unknown texture type from DDS.");
            return RenderTextureDimension::UNKNOWN;
        }
    }

    static RenderFormat toRenderFormat(ddspp::DXGIFormat format) {
        switch (format) {
        case ddspp::R32G32B32A32_TYPELESS:
            return RenderFormat::R32G32B32A32_TYPELESS;
        case ddspp::R32G32B32A32_FLOAT:
            return RenderFormat::R32G32B32A32_FLOAT;
        case ddspp::R32G32B32A32_UINT:
            return RenderFormat::R32G32B32A32_UINT;
        case ddspp::R32G32B32A32_SINT:
            return RenderFormat::R32G32B32A32_SINT;
        case ddspp::R32G32B32_TYPELESS:
            return RenderFormat::R32G32B32_TYPELESS;
        case ddspp::R32G32B32_FLOAT:
            return RenderFormat::R32G32B32_FLOAT;
        case ddspp::R32G32B32_UINT:
            return RenderFormat::R32G32B32_UINT;
        case ddspp::R32G32B32_SINT:
            return RenderFormat::R32G32B32_SINT;
        case ddspp::R16G16B16A16_TYPELESS:
            return RenderFormat::R16G16B16A16_TYPELESS;
        case ddspp::R16G16B16A16_FLOAT:
            return RenderFormat::R16G16B16A16_FLOAT;
        case ddspp::R16G16B16A16_UNORM:
            return RenderFormat::R16G16B16A16_UNORM;
        case ddspp::R16G16B16A16_UINT:
            return RenderFormat::R16G16B16A16_UINT;
        case ddspp::R16G16B16A16_SNORM:
            return RenderFormat::R16G16B16A16_SNORM;
        case ddspp::R16G16B16A16_SINT:
            return RenderFormat::R16G16B16A16_SINT;
        case ddspp::R32G32_TYPELESS:
            return RenderFormat::R32G32_TYPELESS;
        case ddspp::R32G32_FLOAT:
            return RenderFormat::R32G32_FLOAT;
        case ddspp::R32G32_UINT:
            return RenderFormat::R32G32_UINT;
        case ddspp::R32G32_SINT:
            return RenderFormat::R32G32_SINT;
        case ddspp::R8G8B8A8_TYPELESS:
            return RenderFormat::R8G8B8A8_TYPELESS;
        case ddspp::R8G8B8A8_UNORM:
            return RenderFormat::R8G8B8A8_UNORM;
        case ddspp::R8G8B8A8_UINT:
            return RenderFormat::R8G8B8A8_UINT;
        case ddspp::R8G8B8A8_SNORM:
            return RenderFormat::R8G8B8A8_SNORM;
        case ddspp::R8G8B8A8_SINT:
            return RenderFormat::R8G8B8A8_SINT;
        case ddspp::B8G8R8A8_UNORM:
            return RenderFormat::B8G8R8A8_UNORM;
        case ddspp::R16G16_TYPELESS:
            return RenderFormat::R16G16_TYPELESS;
        case ddspp::R16G16_FLOAT:
            return RenderFormat::R16G16_FLOAT;
        case ddspp::R16G16_UNORM:
            return RenderFormat::R16G16_UNORM;
        case ddspp::R16G16_UINT:
            return RenderFormat::R16G16_UINT;
        case ddspp::R16G16_SNORM:
            return RenderFormat::R16G16_SNORM;
        case ddspp::R16G16_SINT:
            return RenderFormat::R16G16_SINT;
        case ddspp::R32_TYPELESS:
            return RenderFormat::R32_TYPELESS;
        case ddspp::D32_FLOAT:
            return RenderFormat::D32_FLOAT;
        case ddspp::R32_FLOAT:
            return RenderFormat::R32_FLOAT;
        case ddspp::R32_UINT:
            return RenderFormat::R32_UINT;
        case ddspp::R32_SINT:
            return RenderFormat::R32_SINT;
        case ddspp::R8G8_TYPELESS:
            return RenderFormat::R8G8_TYPELESS;
        case ddspp::R8G8_UNORM:
            return RenderFormat::R8G8_UNORM;
        case ddspp::R8G8_UINT:
            return RenderFormat::R8G8_UINT;
        case ddspp::R8G8_SNORM:
            return RenderFormat::R8G8_SNORM;
        case ddspp::R8G8_SINT:
            return RenderFormat::R8G8_SINT;
        case ddspp::R16_TYPELESS:
            return RenderFormat::R16_TYPELESS;
        case ddspp::R16_FLOAT:
            return RenderFormat::R16_FLOAT;
        case ddspp::D16_UNORM:
            return RenderFormat::D16_UNORM;
        case ddspp::R16_UNORM:
            return RenderFormat::R16_UNORM;
        case ddspp::R16_UINT:
            return RenderFormat::R16_UINT;
        case ddspp::R16_SNORM:
            return RenderFormat::R16_SNORM;
        case ddspp::R16_SINT:
            return RenderFormat::R16_SINT;
        case ddspp::R8_TYPELESS:
            return RenderFormat::R8_TYPELESS;
        case ddspp::R8_UNORM:
            return RenderFormat::R8_UNORM;
        case ddspp::R8_UINT:
            return RenderFormat::R8_UINT;
        case ddspp::R8_SNORM:
            return RenderFormat::R8_SNORM;
        case ddspp::R8_SINT:
            return RenderFormat::R8_SINT;
        case ddspp::BC1_TYPELESS:
            return RenderFormat::BC1_TYPELESS;
        case ddspp::BC1_UNORM:
            return RenderFormat::BC1_UNORM;
        case ddspp::BC1_UNORM_SRGB:
            return RenderFormat::BC1_UNORM_SRGB;
        case ddspp::BC2_TYPELESS:
            return RenderFormat::BC2_TYPELESS;
        case ddspp::BC2_UNORM:
            return RenderFormat::BC2_UNORM;
        case ddspp::BC2_UNORM_SRGB:
            return RenderFormat::BC2_UNORM_SRGB;
        case ddspp::BC3_TYPELESS:
            return RenderFormat::BC3_TYPELESS;
        case ddspp::BC3_UNORM:
            return RenderFormat::BC3_UNORM;
        case ddspp::BC3_UNORM_SRGB:
            return RenderFormat::BC3_UNORM_SRGB;
        case ddspp::BC4_TYPELESS:
            return RenderFormat::BC4_TYPELESS;
        case ddspp::BC4_UNORM:
            return RenderFormat::BC4_UNORM;
        case ddspp::BC4_SNORM:
            return RenderFormat::BC4_SNORM;
        case ddspp::BC5_TYPELESS:
            return RenderFormat::BC5_TYPELESS;
        case ddspp::BC5_UNORM:
            return RenderFormat::BC5_UNORM;
        case ddspp::BC5_SNORM:
            return RenderFormat::BC5_SNORM;
        case ddspp::BC6H_TYPELESS:
            return RenderFormat::BC6H_TYPELESS;
        case ddspp::BC6H_UF16:
            return RenderFormat::BC6H_UF16;
        case ddspp::BC6H_SF16:
            return RenderFormat::BC6H_SF16;
        case ddspp::BC7_TYPELESS:
            return RenderFormat::BC7_TYPELESS;
        case ddspp::BC7_UNORM:
            return RenderFormat::BC7_UNORM;
        case ddspp::BC7_UNORM_SRGB:
            return RenderFormat::BC7_UNORM_SRGB;
        default:
            assert(false && "Unsupported format from DDS.");
            return RenderFormat::UNKNOWN;
        }
    }

    bool TextureCache::setDDS(Texture *dstTexture, RenderWorker *worker, const uint8_t *bytes, size_t byteCount, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool, uint32_t minMipWidth, uint32_t minMipHeight) {
        assert(dstTexture != nullptr);
        assert(worker != nullptr);
        assert(bytes != nullptr);

        ddspp::Descriptor ddsDescriptor;
        ddspp::Result result = ddspp::decode_header((unsigned char *)(bytes), ddsDescriptor);
        if (result != ddspp::Success) {
            return false;
        }

        RenderTextureDesc desc;
        desc.dimension = toRenderDimension(ddsDescriptor.type);
        desc.width = ddsDescriptor.width;
        desc.height = ddsDescriptor.height;
        desc.depth = 1;
        desc.mipLevels = 1;
        desc.format = toRenderFormat(ddsDescriptor.format);

        // Only load mipmaps as long as they're above a certain width and height.
        for (uint32_t mip = 1; mip < ddsDescriptor.numMips; mip++) {
            uint32_t mipWidth = std::max(desc.width >> mip, 1U);
            uint32_t mipHeight = std::max(desc.height >> mip, 1U);
            if ((mipWidth < minMipWidth) || (mipHeight < minMipHeight)) {
                break;
            }
            
            desc.mipLevels++;
        }

        dstTexture->texture = worker->device->createTexture(desc);
        dstTexture->width = ddsDescriptor.width;
        dstTexture->height = ddsDescriptor.height;
        dstTexture->mipmaps = desc.mipLevels;
        dstTexture->format = desc.format;

        const uint8_t *imageData = &bytes[ddsDescriptor.headerSize];
        size_t imageDataSize = byteCount - ddsDescriptor.headerSize;

        // Compute the additional padding that will be required on the buffer to align the mipmap data.
        std::vector<uint32_t> mipmapOffsets;
        uint32_t imageDataPadding = 0;
        const uint32_t imageDataAlignment = 16;
        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            uint32_t ddsOffset = ddspp::get_offset(ddsDescriptor, mip, 0);
            uint32_t alignedOffset = ddsOffset + imageDataPadding;
            if ((alignedOffset % imageDataAlignment) != 0) {
                imageDataPadding += imageDataAlignment - (alignedOffset % imageDataAlignment);
            }

            mipmapOffsets.emplace_back(ddsOffset + imageDataPadding);
        }

        const size_t uploadBufferSize = imageDataSize + imageDataPadding;
        if (uploadResourcePool != nullptr) {
            dstUploadResource = uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(uploadBufferSize));
        }
        else {
            dstUploadResource = worker->device->createBuffer(RenderBufferDesc::UploadBuffer(uploadBufferSize));
        }

        // Copy each mipmap into the buffer with the correct padding applied.
        uint32_t mipmapOffset = 0;
        uint8_t *dstData = reinterpret_cast<uint8_t *>(dstUploadResource->map());
        memset(dstData, 0, uploadBufferSize);
        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            uint32_t ddsOffset = ddspp::get_offset(ddsDescriptor, mip, 0);
            uint32_t ddsSize = ((mip + 1) < ddsDescriptor.numMips) ? (ddspp::get_offset(ddsDescriptor, mip + 1, 0) - ddsOffset) : (imageDataSize - ddsOffset);
            uint32_t mipOffset = mipmapOffsets[mip];
            memcpy(&dstData[mipOffset], &imageData[ddsOffset], ddsSize);
        }

        dstUploadResource->unmap();
        worker->commandList->barriers(RenderBarrierStage::COPY, RenderBufferBarrier(dstUploadResource.get(), RenderBufferAccess::READ));
        worker->commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::COPY_DEST));

        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            uint32_t offset = mipmapOffsets[mip];
            uint32_t mipWidth = std::max(desc.width >> mip, 1U);
            uint32_t mipHeight = std::max(desc.height >> mip, 1U);
            uint32_t rowWidth = mipWidth;
            worker->commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(dstTexture->texture.get(), mip), RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), desc.format, mipWidth, mipHeight, 1, rowWidth, offset));
        }

        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::SHADER_READ));

        return true;
    }

    bool TextureCache::loadBytesFromPath(const std::filesystem::path &path, std::vector<uint8_t> &bytes) {
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.seekg(0, std::ios::end);
            bytes.resize(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read((char *)(bytes.data()), bytes.size());
            return !file.bad();
        }
        else {
            return false;
        }
    }

    void TextureCache::uploadThreadLoop() {
        Thread::setCurrentThreadName("RT64 Texture");

        uploadThreadRunning = true;

        std::vector<TextureUpload> queueCopy;
        std::vector<TextureUpload> newQueue;
        std::vector<ReplacementCheck> replacementQueueCopy;
        std::vector<HashTexturePair> texturesUploaded;
        std::vector<HashTexturePair> texturesReplaced;
        std::vector<RenderTextureBarrier> beforeCopyBarriers;
        std::vector<RenderTextureBarrier> beforeDecodeBarriers;
        std::vector<RenderTextureBarrier> afterDecodeBarriers;
        std::vector<uint8_t> replacementBytes;
        bool replacementsUploaded = false;

        while (uploadThreadRunning) {
            replacementQueueCopy.clear();

            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
                uploadQueueChanged.wait(queueLock, [this]() {
                    return !uploadThreadRunning || !uploadQueue.empty() || !replacementQueue.empty();
                });

                if (!uploadQueue.empty()) {
                    queueCopy = uploadQueue;
                }

                if (!replacementQueue.empty()) {
                    replacementQueueCopy.insert(replacementQueueCopy.end(), replacementQueue.begin(), replacementQueue.end());
                    replacementQueue.clear();
                }
            }

            if (!queueCopy.empty() || !replacementQueueCopy.empty()) {
                // Create new upload buffers and descriptor heaps to fill out the required size.
                const size_t queueSize = queueCopy.size();
                const uint64_t TMEMSize = 0x1000;
                for (size_t i = tmemUploadResources.size(); i < queueSize; i++) {
                    tmemUploadResources.emplace_back(threadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(TMEMSize)));
                }

                for (size_t i = descriptorSets.size(); i < queueSize; i++) {
                    descriptorSets.emplace_back(std::make_unique<TextureDecodeDescriptorSet>(worker->device));
                }

                // Upload all textures in the queue.
                {
                    RenderWorkerExecution execution(worker);
                    texturesUploaded.clear();
                    beforeCopyBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        static uint32_t TMEMGlobalCounter = 0;
                        const TextureUpload &upload = queueCopy[i];
                        Texture *newTexture = new Texture();
                        newTexture->creationFrame = upload.creationFrame;
                        texturesUploaded.emplace_back(HashTexturePair{ upload.hash, newTexture });

                        if (developerMode) {
                            newTexture->bytesTMEM = upload.bytesTMEM;
                        }

                        newTexture->format = RenderFormat::R8_UINT;
                        newTexture->width = upload.width;
                        newTexture->height = upload.height;
                        newTexture->tmem = worker->device->createTexture(RenderTextureDesc::Texture1D(uint32_t(upload.bytesTMEM.size()), 1, newTexture->format));
                        newTexture->tmem->setName("Texture Cache TMEM #" + std::to_string(TMEMGlobalCounter++));

                        void *dstData = tmemUploadResources[i]->map();
                        memcpy(dstData, upload.bytesTMEM.data(), upload.bytesTMEM.size());
                        tmemUploadResources[i]->unmap();

                        beforeCopyBarriers.emplace_back(RenderTextureBarrier(newTexture->tmem.get(), RenderTextureLayout::COPY_DEST));
                    }

                    worker->commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers);

                    beforeDecodeBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        const TextureUpload &upload = queueCopy[i];
                        const uint32_t byteCount = uint32_t(upload.bytesTMEM.size());
                        Texture *dstTexture = texturesUploaded[i].texture;
                        worker->commandList->copyTextureRegion(
                            RenderTextureCopyLocation::Subresource(dstTexture->tmem.get()),
                            RenderTextureCopyLocation::PlacedFootprint(tmemUploadResources[i].get(), RenderFormat::R8_UINT, byteCount, 1, 1, byteCount)
                        );

                        beforeDecodeBarriers.emplace_back(RenderTextureBarrier(dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ));

                        if (upload.decodeTMEM) {
                            static uint32_t TextureGlobalCounter = 0;
                            TextureDecodeDescriptorSet *descSet = descriptorSets[i].get();
                            dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
                            dstTexture->texture = worker->device->createTexture(RenderTextureDesc::Texture2D(upload.width, upload.height, 1, dstTexture->format, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
                            dstTexture->texture->setName("Texture Cache RGBA32 #" + std::to_string(TextureGlobalCounter++));
                            descSet->setTexture(descSet->TMEM, dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ);
                            descSet->setTexture(descSet->RGBA32, dstTexture->texture.get(), RenderTextureLayout::GENERAL);
                            beforeDecodeBarriers.emplace_back(RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::GENERAL));
                        }
                    }
                    
                    worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeDecodeBarriers);

                    const ShaderRecord &textureDecode = shaderLibrary->textureDecode;
                    bool pipelineSet = false;
                    afterDecodeBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        const TextureUpload &upload = queueCopy[i];
                        if (upload.decodeTMEM) {
                            if (!pipelineSet) {
                                worker->commandList->setPipeline(textureDecode.pipeline.get());
                                worker->commandList->setComputePipelineLayout(textureDecode.pipelineLayout.get());
                            }

                            interop::TextureDecodeCB decodeCB;
                            decodeCB.Resolution.x = upload.width;
                            decodeCB.Resolution.y = upload.height;
                            decodeCB.fmt = upload.loadTile.fmt;
                            decodeCB.siz = upload.loadTile.siz;
                            decodeCB.address = interop::uint(upload.loadTile.tmem) << 3;
                            decodeCB.stride = interop::uint(upload.loadTile.line) << 3;
                            decodeCB.tlut = upload.tlut;
                            decodeCB.palette = upload.loadTile.palette;

                            // Dispatch compute shader for decoding texture.
                            const uint32_t ThreadGroupSize = 8;
                            const uint32_t dispatchX = (decodeCB.Resolution.x + ThreadGroupSize - 1) / ThreadGroupSize;
                            const uint32_t dispatchY = (decodeCB.Resolution.y + ThreadGroupSize - 1) / ThreadGroupSize;
                            worker->commandList->setComputePushConstants(0, &decodeCB);
                            worker->commandList->setComputeDescriptorSet(descriptorSets[i]->get(), 0);
                            worker->commandList->dispatch(dispatchX, dispatchY, 1);

                            afterDecodeBarriers.emplace_back(RenderTextureBarrier(texturesUploaded[i].texture->texture.get(), RenderTextureLayout::SHADER_READ));
                        }
                        
                        if ((upload.width > 0) && (upload.height > 0)) {
                            // If the database uses an older hash version, we hash TMEM again with the version corresponding to the database.
                            uint32_t databaseVersion = textureMap.replacementMap.db.config.hashVersion;
                            uint64_t databaseHash = upload.hash;
                            if (databaseVersion < TMEMHasher::CurrentHashVersion) {
                                databaseHash = TMEMHasher::hash(upload.bytesTMEM.data(), upload.loadTile, upload.width, upload.height, upload.tlut, databaseVersion);
                            }

                            // Add this hash so it's checked for a replacement.
                            replacementQueueCopy.emplace_back(ReplacementCheck{ upload.hash, databaseHash, uint32_t(upload.width), uint32_t(upload.height) });
                        }
                    }

                    if (!afterDecodeBarriers.empty()) {
                        worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterDecodeBarriers);
                    }
                    
                    for (const ReplacementCheck &replacementCheck : replacementQueueCopy) {
                        std::string relativePath = textureMap.replacementMap.getRelativePathFromHash(replacementCheck.databaseHash);
                        if (!relativePath.empty()) {
                            std::filesystem::path filePath = textureMap.replacementMap.directoryPath / std::filesystem::u8path(relativePath);
                            Texture *replacementTexture = textureMap.replacementMap.getFromRelativePath(relativePath);
                            if ((replacementTexture == nullptr) && TextureCache::loadBytesFromPath(filePath, replacementBytes)) {
                                replacementUploadResources.emplace_back();
                                replacementTexture = textureMap.replacementMap.loadFromBytes(worker, replacementCheck.databaseHash, relativePath, replacementBytes, replacementUploadResources.back(), nullptr, replacementCheck.minMipWidth, replacementCheck.minMipHeight);
                                replacementsUploaded = true;
                            }

                            if (replacementTexture != nullptr) {
                                texturesReplaced.emplace_back(HashTexturePair{ replacementCheck.textureHash, replacementTexture });
                            }
                        }
                    }
                }

                if (replacementsUploaded) {
                    replacementUploadResources.clear();
                    replacementsUploaded = false;
                }

                // Add all the textures to the map once they're ready.
                {
                    const std::unique_lock<std::mutex> lock(textureMapMutex);
                    for (const HashTexturePair &pair : texturesUploaded) {
                        textureMap.add(pair.hash, pair.texture->creationFrame, pair.texture);
                    }

                    for (const HashTexturePair &pair : texturesReplaced) {
                        textureMap.replace(pair.hash, pair.texture);
                    }
                }

                // Make the new queue the remaining subsection of the upload queue that wasn't processed in this batch.
                {
                    const std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
                    newQueue = std::vector<TextureUpload>(uploadQueue.begin() + queueSize, uploadQueue.end());
                    uploadQueue = std::move(newQueue);
                }

                queueCopy.clear();
                uploadQueueFinished.notify_all();
            }
        }
    }

    void TextureCache::queueGPUUploadTMEM(uint64_t hash, uint64_t creationFrame, const uint8_t *bytes, int bytesCount, int width, int height, uint32_t tlut, const LoadTile &loadTile, bool decodeTMEM) {
        assert(bytes != nullptr);
        assert(bytesCount > 0);

        TextureUpload newUpload;
        newUpload.hash = hash;
        newUpload.creationFrame = creationFrame;
        newUpload.width = width;
        newUpload.height = height;
        newUpload.tlut = tlut;
        newUpload.loadTile = loadTile;
        newUpload.bytesTMEM = std::vector<uint8_t>(bytes, bytes + bytesCount);
        newUpload.decodeTMEM = decodeTMEM;

        {
            const std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
            uploadQueue.emplace_back(newUpload);
        }

        uploadQueueChanged.notify_all();
    }

    void TextureCache::waitForGPUUploads() {
        std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
        uploadQueueFinished.wait(queueLock, [this]() {
            return uploadQueue.empty();
        });
    }

    bool TextureCache::useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, bool &textureReplaced, bool &hasMipmaps) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.use(hash, submissionFrame, textureIndex, textureScale, textureReplaced, hasMipmaps);
    }

    bool TextureCache::useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex) {
        interop::float2 textureScale;
        bool textureReplaced;
        bool hasMipmaps;
        return useTexture(hash, submissionFrame, textureIndex, textureScale, textureReplaced, hasMipmaps);
    }
    
    bool TextureCache::addReplacement(uint64_t hash, const std::string &relativePath) {
        // TODO: The case where a replacement is reloaded needs to be handled correctly. Multiple hashes can point to the same path. All hashes pointing to that path must be reloaded correctly.

        const std::unique_lock<std::mutex> lock(textureMapMutex);
        std::vector<uint8_t> replacementBytes;
        if (!TextureCache::loadBytesFromPath(textureMap.replacementMap.directoryPath / std::filesystem::u8path(relativePath), replacementBytes)) {
            return false;
        }

        // Load texture replacement immediately.
        std::unique_ptr<RenderBuffer> dstUploadBuffer;
        Texture *newTexture = nullptr;
        {
            RenderWorkerExecution execution(worker);
            newTexture = textureMap.replacementMap.loadFromBytes(worker, hash, relativePath, replacementBytes, dstUploadBuffer);
        }

        if (newTexture == nullptr) {
            delete newTexture;
            return false;
        }

        // Store replacement in the texture pack configuration.
        ReplacementTexture replacement;
        replacement.hashes.rt64 = ReplacementDatabase::hashToString(hash);
        replacement.path = relativePath;
        textureMap.replacementMap.db.addReplacement(replacement);

        // Replace the texture in the cache.
        if (newTexture != nullptr) {
            textureMap.replace(hash, newTexture);
            return true;
        }
        else {
            return false;
        }
    }

    bool TextureCache::loadReplacementDirectory(const std::filesystem::path &directoryPath) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        textureMap.clearReplacements();
        textureMap.replacementMap.clear();
        textureMap.replacementMap.directoryPath = directoryPath;

        std::ifstream databaseFile(directoryPath / ReplacementDatabaseFilename);
        if (databaseFile.is_open()) {
            textureMap.replacementMap.readDatabase(databaseFile);
        }
        else {
            textureMap.replacementMap.db = ReplacementDatabase();
        }

        textureMap.replacementMap.resolveAutoPaths();

        // Queue all currently loaded hashes to detect replacements with.
        {
            const std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
            replacementQueue.clear();
            for (size_t i = 0; i < textureMap.hashes.size(); i++) {
                if (textureMap.hashes[i] != 0) {
                    uint32_t minMipWidth = 0;
                    uint32_t minMipHeight = 0;
                    if (textureMap.textures[i] != nullptr) {
                        minMipWidth = textureMap.textures[i]->width;
                        minMipHeight = textureMap.textures[i]->height;
                    }

                    replacementQueue.emplace_back(ReplacementCheck{ textureMap.hashes[i], textureMap.hashes[i], minMipWidth, minMipHeight });
                }
            }
        }

        uploadQueueChanged.notify_all();

        return true;
    }

    bool TextureCache::saveReplacementDatabase() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        if (textureMap.replacementMap.directoryPath.empty()) {
            return false;
        }

        const std::filesystem::path databasePath = textureMap.replacementMap.directoryPath / ReplacementDatabaseFilename;
        const std::filesystem::path databaseNewPath = textureMap.replacementMap.directoryPath / (ReplacementDatabaseFilename + ".new");
        const std::filesystem::path databaseOldPath = textureMap.replacementMap.directoryPath / (ReplacementDatabaseFilename + ".old");
        std::ofstream databaseNewFile(databaseNewPath);
        if (!textureMap.replacementMap.saveDatabase(databaseNewFile)) {
            return false;
        }

        databaseNewFile.close();

        std::error_code ec;
        if (std::filesystem::exists(databasePath)) {
            if (std::filesystem::exists(databaseOldPath)) {
                std::filesystem::remove(databaseOldPath, ec);
                if (ec) {
                    fprintf(stderr, "%s\n", ec.message().c_str());
                    return false;
                }
            }

            std::filesystem::rename(databasePath, databaseOldPath, ec);
            if (ec) {
                fprintf(stderr, "%s\n", ec.message().c_str());
                return false;
            }
        }

        std::filesystem::rename(databaseNewPath, databasePath, ec);
        if (ec) {
            fprintf(stderr, "%s\n", ec.message().c_str());
            return false;
        }

        return true;
    }

    void TextureCache::removeUnusedEntriesFromDatabase() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        if (textureMap.replacementMap.directoryPath.empty()) {
            return;
        }

        textureMap.replacementMap.removeUnusedEntriesFromDatabase();
    }

    Texture *TextureCache::getTexture(uint32_t textureIndex) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.get(textureIndex);
    }

    bool TextureCache::evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.evict(submissionFrame, evictedHashes);
    }

    void TextureCache::incrementLock() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        lockCounter++;
    }

    void TextureCache::decrementLock() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        lockCounter--;

        if (lockCounter == 0) {
            // Delete evicted textures from texture map.
            for (Texture *texture : textureMap.evictedTextures) {
                delete texture;
            }

            textureMap.evictedTextures.clear();

            // Delete evicted textures from replacement map.
            for (Texture *texture : textureMap.replacementMap.evictedTextures) {
                delete texture;
            }

            textureMap.replacementMap.evictedTextures.clear();
        }
    }
};