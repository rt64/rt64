//
// RT64
//

#include "rt64_raster_shader_cache.h"

#include "common/rt64_thread.h"

#define ENABLE_OPTIMIZED_SHADER_GENERATION

namespace RT64 {
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

        while (threadRunning) {
            ShaderDescription shaderDesc;
            bool fromPriorityQueue = false;
            
            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock<std::mutex> queueLock(shaderCache->descQueueMutex);
                shaderCache->descQueueActiveCount--;
                shaderCache->descQueueChanged.wait(queueLock, [this]() {
                    return !threadRunning || !shaderCache->descQueue.empty();
                });

                shaderCache->descQueueActiveCount++;
                if (!shaderCache->descQueue.empty()) {
                    shaderDesc = shaderCache->descQueue.front();
                    shaderCache->descQueue.pop();
                    fromPriorityQueue = true;
                }
            }
            
            // Compile the shader at the top of the queue.
            if (fromPriorityQueue) {
                assert((shaderCache->shaderUber != nullptr) && "Ubershader should've been created by the time a new shader is submitted to the cache.");
                const RenderPipelineLayout *uberPipelineLayout = shaderCache->shaderUber->pipelineLayout.get();
                const RenderMultisampling multisampling = shaderCache->multisampling;
                std::unique_ptr<RasterShader> newShader = std::make_unique<RasterShader>(shaderCache->device, shaderDesc, uberPipelineLayout, shaderCache->shaderFormat, multisampling, shaderCache->shaderCompiler.get(), &shaderCache->optimizerCacheSPIRV);

                {
                    const std::unique_lock<std::mutex> lock(shaderCache->GPUShadersMutex);
                    shaderCache->GPUShaders[shaderDesc.hash()] = std::move(newShader);
                }
            }
        }
    }

    // RasterShaderCache

    RasterShaderCache::RasterShaderCache(uint32_t threadCount, uint32_t ubershaderThreadCount) {
        assert(threadCount > 0);

        this->threadCount = threadCount;
        this->ubershaderThreadCount = ubershaderThreadCount;

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

        shaderUber = std::make_unique<RasterShaderUber>(device, shaderFormat, multisampling, shaderLibrary, ubershaderThreadCount);
        usesHDR = shaderLibrary->usesHDR;

        // Initialize the re-spirv optimizer cache.
        if (shaderFormat == RenderShaderFormat::SPIRV) {
            optimizerCacheSPIRV.initialize();
        }
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

    uint32_t RasterShaderCache::shaderCount() {
        std::unique_lock<std::mutex> lock(GPUShadersMutex);
        return GPUShaders.size();
    }
};