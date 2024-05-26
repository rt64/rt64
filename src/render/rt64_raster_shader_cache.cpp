//
// RT64
//

#include "rt64_raster_shader_cache.h"

#include "common/rt64_thread.h"

#define ENABLE_OPTIMIZED_SHADER_GENERATION

namespace RT64 {
    // RasterShaderCache::OfflineList

    static const uint32_t OfflineMagic = 0x43535452;
    static const uint32_t OfflineVersion = 2;

    RasterShaderCache::OfflineList::OfflineList() {
        entryIterator = entries.end();
    }

    bool RasterShaderCache::OfflineList::load(std::istream &stream) {
        Entry entry;
        while (!stream.eof()) {
            uint32_t magic = 0;
            uint32_t version = 0;
            uint64_t vsHash = 0;
            uint64_t psHash = 0;
            stream.read(reinterpret_cast<char *>(&magic), sizeof(uint32_t));
            stream.read(reinterpret_cast<char *>(&version), sizeof(uint32_t));
            stream.read(reinterpret_cast<char *>(&vsHash), sizeof(uint64_t));
            stream.read(reinterpret_cast<char *>(&psHash), sizeof(uint64_t));
            if (stream.eof()) {
                break;
            }

            if ((magic != OfflineMagic) || (version != OfflineVersion) || (vsHash != RasterShaderUber::RasterVSTextHash) || (psHash != RasterShaderUber::RasterPSTextHash)) {
                return false;
            }

            stream.read(reinterpret_cast<char *>(&entry.shaderDesc), sizeof(ShaderDescription));

            uint32_t vsDxilSize = 0;
            stream.read(reinterpret_cast<char *>(&vsDxilSize), sizeof(uint32_t));
            entry.vsDxilBytes.resize(vsDxilSize);
            stream.read(reinterpret_cast<char *>(entry.vsDxilBytes.data()), vsDxilSize);
            if (stream.bad() || entry.vsDxilBytes.empty()) {
                return false;
            }

            uint32_t psDxilSize = 0;
            stream.read(reinterpret_cast<char *>(&psDxilSize), sizeof(uint32_t));
            entry.psDxilBytes.resize(psDxilSize);
            stream.read(reinterpret_cast<char *>(entry.psDxilBytes.data()), psDxilSize);
            if (stream.bad() || entry.psDxilBytes.empty()) {
                return false;
            }

            entries.emplace_back(entry);
        }

        entryIterator = entries.begin();

        return true;
    }

    void RasterShaderCache::OfflineList::reset() {
        if (!entries.empty()) {
            entryIterator = entries.begin();
        }
    }

    void RasterShaderCache::OfflineList::step(Entry &entry) {
        if (entryIterator != entries.end()) {
            entry = *entryIterator;
            entryIterator++;
        }
    }

    bool RasterShaderCache::OfflineList::atEnd() const {
        if (!entries.empty()) {
            return entryIterator == entries.end();
        }
        else {
            return true;
        }
    }

    // RasterShaderCache::OfflineDumper

    bool RasterShaderCache::OfflineDumper::startDumping(const std::filesystem::path &path) {
        assert(!dumpStream.is_open());
        dumpStream.open(path, std::ios::binary);
        return dumpStream.is_open();
    }

    bool RasterShaderCache::OfflineDumper::stepDumping(const ShaderDescription &shaderDesc, const std::vector<uint8_t> &vsDxilBytes, const std::vector<uint8_t> &psDxilBytes) {
        assert(!vsDxilBytes.empty());
        assert(!psDxilBytes.empty());
        uint32_t vsDxilSize = uint32_t(vsDxilBytes.size());
        uint32_t psDxilSize = uint32_t(psDxilBytes.size());
        dumpStream.write(reinterpret_cast<const char *>(&OfflineMagic), sizeof(uint32_t));
        dumpStream.write(reinterpret_cast<const char *>(&OfflineVersion), sizeof(uint32_t));
        dumpStream.write(reinterpret_cast<const char *>(&RasterShaderUber::RasterVSTextHash), sizeof(uint64_t));
        dumpStream.write(reinterpret_cast<const char *>(&RasterShaderUber::RasterPSTextHash), sizeof(uint64_t));
        dumpStream.write(reinterpret_cast<const char *>(&shaderDesc), sizeof(ShaderDescription));
        dumpStream.write(reinterpret_cast<const char *>(&vsDxilSize), sizeof(uint32_t));
        dumpStream.write(reinterpret_cast<const char *>(vsDxilBytes.data()), vsDxilSize);
        dumpStream.write(reinterpret_cast<const char *>(&psDxilSize), sizeof(uint32_t));
        dumpStream.write(reinterpret_cast<const char *>(psDxilBytes.data()), psDxilSize);
        return dumpStream.bad();
    }

    bool RasterShaderCache::OfflineDumper::stopDumping() {
        assert(dumpStream.is_open());
        dumpStream.close();
        return dumpStream.bad();
    }

    bool RasterShaderCache::OfflineDumper::isDumping() const {
        return dumpStream.is_open();
    }

    // RasterShaderCache::CompilationThread
    
    RasterShaderCache::CompilationThread::CompilationThread(RasterShaderCache *shaderCache) {
        assert(shaderCache != nullptr);

        this->shaderCache = shaderCache;

        thread = std::make_unique<std::thread>(&CompilationThread::loop, this);
        threadRunning = false;
    }

    RasterShaderCache::CompilationThread::~CompilationThread() {
        threadRunning = false;
        shaderCache->descQueueChanged.notify_all();
        thread->join();
        thread.reset(nullptr);
    }

    void RasterShaderCache::CompilationThread::loop() {
        Thread::setCurrentThreadName("RT64 Shader");

        // The shader compilation thread should have idle priority by default as the application can use the ubershader in the meantime.
        Thread::setCurrentThreadPriority(Thread::Priority::Idle);

        threadRunning = true;

        OfflineList::Entry offlineListEntry;
        std::vector<uint8_t> dumperVsBytes;
        std::vector<uint8_t> dumperPsBytes;
        while (threadRunning) {
            ShaderDescription shaderDesc;
            bool fromPriorityQueue = false;
            bool fromOfflineList = false;
            
            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock<std::mutex> queueLock(shaderCache->descQueueMutex);
                shaderCache->descQueueActiveCount--;
                shaderCache->descQueueChanged.wait(queueLock, [this]() {
                    return !threadRunning || !shaderCache->descQueue.empty() || !shaderCache->offlineList.atEnd();
                });

                shaderCache->descQueueActiveCount++;
                if (!shaderCache->descQueue.empty()) {
                    shaderDesc = shaderCache->descQueue.front();
                    shaderCache->descQueue.pop();
                    fromPriorityQueue = true;
                }
                else if (!shaderCache->offlineList.atEnd()) {
                    std::unique_lock<std::mutex> queueLock(shaderCache->submissionMutex);
                    while (!shaderCache->offlineList.atEnd() && !fromOfflineList) {
                        shaderCache->offlineList.step(offlineListEntry);

                        // Make sure the hash hasn't been submitted yet by the game. If it hasn't, mark it as such and use this entry of the list.
                        // Also make sure the internal color format used by the shader is compatible.
                        uint64_t shaderHash = offlineListEntry.shaderDesc.hash();
                        const bool matchesColorFormat = (offlineListEntry.shaderDesc.flags.usesHDR == shaderCache->usesHDR);
                        const bool hashMissing = (shaderCache->shaderHashes.find(shaderHash) == shaderCache->shaderHashes.end());
                        if (matchesColorFormat && hashMissing) {
                            shaderDesc = offlineListEntry.shaderDesc;
                            shaderCache->shaderHashes[shaderHash] = true;
                            fromOfflineList = true;
                        }
                    }
                }
            }
            
            // Compile the shader at the top of the queue.
            if (fromPriorityQueue || fromOfflineList) {
                // Check if the shader dumper is active. Specify the shader's bytes should be stored in the thread's vectors.
                std::vector<uint8_t> *shaderVsBytes = nullptr;
                std::vector<uint8_t> *shaderPsBytes = nullptr;
                bool useShaderBytes = false;
                if (fromPriorityQueue) {
                    const std::unique_lock<std::mutex> lock(shaderCache->offlineDumperMutex);
                    if (shaderCache->offlineDumper.isDumping()) {
                        shaderVsBytes = &dumperVsBytes;
                        shaderPsBytes = &dumperPsBytes;
                    }
                }
                else {
                    shaderVsBytes = &offlineListEntry.vsDxilBytes;
                    shaderPsBytes = &offlineListEntry.psDxilBytes;
                    useShaderBytes = true;
                }

                assert((shaderCache->shaderUber != nullptr) && "Ubershader should've been created by the time a new shader is submitted to the cache.");
                const RenderPipelineLayout *uberPipelineLayout = shaderCache->shaderUber->pipelineLayout.get();
                const RenderMultisampling multisampling = shaderCache->multisampling;
                std::unique_ptr<RasterShader> newShader = std::make_unique<RasterShader>(shaderCache->device, shaderDesc, uberPipelineLayout, shaderCache->shaderFormat, multisampling, shaderCache->shaderCompiler.get(), shaderVsBytes, shaderPsBytes, useShaderBytes);

                // Dump the bytes of the shader if requested.
                if (!useShaderBytes && (shaderVsBytes != nullptr) && (shaderPsBytes != nullptr)) {
                    const std::unique_lock<std::mutex> lock(shaderCache->offlineDumperMutex);
                    if (shaderCache->offlineDumper.isDumping()) {
                        shaderCache->offlineDumper.stepDumping(shaderDesc, dumperVsBytes, dumperPsBytes);

                        // Toggle the use of HDR and compile another shader.
                        ShaderDescription shaderDescAlt = shaderDesc;
                        shaderDescAlt.flags.usesHDR = (shaderDescAlt.flags.usesHDR == 0);
                        std::make_unique<RasterShader>(shaderCache->device, shaderDescAlt, uberPipelineLayout, shaderCache->shaderFormat, multisampling, shaderCache->shaderCompiler.get(), shaderVsBytes, shaderPsBytes, useShaderBytes);
                        shaderCache->offlineDumper.stepDumping(shaderDescAlt, dumperVsBytes, dumperPsBytes);
                    }
                }

                {
                    const std::unique_lock<std::mutex> lock(shaderCache->GPUShadersMutex);
                    shaderCache->GPUShaders[shaderDesc.hash()] = std::move(newShader);
                }
            }
        }
    }

    // RasterShaderCache

    RasterShaderCache::RasterShaderCache(uint32_t threadCount) {
        assert(threadCount > 0);

        this->threadCount = threadCount;

#ifdef ENABLE_OPTIMIZED_SHADER_GENERATION
#   ifdef _WIN32
        shaderCompiler = std::make_unique<ShaderCompiler>();
#   endif

        descQueueActiveCount = threadCount;

        for (uint32_t t = 0; t < threadCount; t++) {
            compilationThreads.push_back(std::make_unique<CompilationThread>(this));
        }
#endif
    }

    RasterShaderCache::~RasterShaderCache() {
        compilationThreads.clear();
    }

    void RasterShaderCache::setup(RenderDevice *device, RenderShaderFormat shaderFormat, const ShaderLibrary *shaderLibrary, const RenderMultisampling &multisampling) {
        assert(device != nullptr);

        this->device = device;
        this->shaderFormat = shaderFormat;
        this->multisampling = multisampling;

        shaderUber = std::make_unique<RasterShaderUber>(device, shaderFormat, multisampling, shaderLibrary, threadCount);
        usesHDR = shaderLibrary->usesHDR;
    }

    void RasterShaderCache::submit(const ShaderDescription &desc) {
        {
            std::unique_lock<std::mutex> queueLock(submissionMutex);

            // Verify if an entry with the same hash was already submitted before.
            const uint64_t shaderHash = desc.hash();
            bool &found = shaderHashes[shaderHash];
            if (found) {
                return;
            }

            found = true;
        }

        // Push a new shader compilation to the queue.
        {
            const std::unique_lock<std::mutex> queueLock(descQueueMutex);
            descQueue.push(desc);
        }

        descQueueChanged.notify_all();
    }
    
    void RasterShaderCache::waitForAll() {
        {
            std::unique_lock<std::mutex> queueLock(descQueueMutex);
            descQueue = std::queue<ShaderDescription>();
        }

        bool keepWaiting = false;
        do {
            std::unique_lock<std::mutex> queueLock(descQueueMutex);
            keepWaiting = (descQueueActiveCount > 0);
        } while (keepWaiting);
    }

    void RasterShaderCache::destroyAll() {
        {
            std::unique_lock<std::mutex> lock(GPUShadersMutex);
            GPUShaders.clear();
        }

        {
            std::unique_lock<std::mutex> queueLock(submissionMutex);
            shaderHashes.clear();
        }
    }

    RasterShader *RasterShaderCache::getGPUShader(const ShaderDescription &desc) {
        const uint64_t shaderHash = desc.hash();

        const std::unique_lock<std::mutex> lock(GPUShadersMutex);
        auto shaderIt = GPUShaders.find(shaderHash);
        if (shaderIt == GPUShaders.end()) {
            return nullptr;
        }

        return shaderIt->second.get();
    }

    RasterShaderUber *RasterShaderCache::getGPUShaderUber() const {
        return shaderUber.get();
    }

    bool RasterShaderCache::isOfflineDumperActive() {
        const std::unique_lock<std::mutex> lock(offlineDumperMutex);
        return offlineDumper.isDumping();
    }

    bool RasterShaderCache::startOfflineDumper(const std::filesystem::path &path) {
        const std::unique_lock<std::mutex> lock(offlineDumperMutex);
        return offlineDumper.startDumping(path);
    }

    bool RasterShaderCache::stopOfflineDumper() {
        const std::unique_lock<std::mutex> lock(offlineDumperMutex);
        return offlineDumper.stopDumping();
    }

    bool RasterShaderCache::loadOfflineList(std::istream &stream) {
        bool result = false;

        {
            std::unique_lock<std::mutex> queueLock(descQueueMutex);
            result = offlineList.load(stream);
        }

        descQueueChanged.notify_all();
        return result;
    }

    void RasterShaderCache::resetOfflineList() {
        {
            std::unique_lock<std::mutex> queueLock(descQueueMutex);
            offlineList.reset();
        }

        descQueueChanged.notify_all();
    }
};