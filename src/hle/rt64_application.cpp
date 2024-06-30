//
// RT64
//

#include "rt64_application.h"
#include "rhi/rt64_render_hooks.h"

#include <filesystem>

#include "common/rt64_dynamic_libraries.h"
#include "common/rt64_elapsed_timer.h"
#include "common/rt64_math.h"
#include "common/rt64_sommelier.h"

#if RT_ENABLED
#   include "res/bluenoise/LDR_64_64_64_RGB1.h"
#endif

#if RT64_BUILD_PLUGIN
#   include "api/rt64_api_common.h"
#endif

//#define TEST_RENDER_INTERFACE
//#define LOG_DISPLAY_LISTS

namespace RT64 {
    // External functions to create the backends.

    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();

    // Application::Core

    VI Application::Core::decodeVI() const {
        VI vi;
        vi.status.word = *VI_STATUS_REG;
        vi.origin = (*VI_ORIGIN_REG) & 0xFFFFFFU;
        vi.width = (*VI_WIDTH_REG) & 0xFFFU;
        vi.intr = (*VI_INTR_REG) & 0x3FF;
        vi.vCurrentLine = (*VI_V_CURRENT_LINE_REG) & 0x3FF;
        vi.burst.word = *VI_TIMING_REG;
        vi.vSync = (*VI_V_SYNC_REG) & 0x3FF;
        vi.hSync.word = *VI_H_SYNC_REG;
        vi.leap.word = *VI_LEAP_REG;
        vi.hRegion.word = *VI_H_START_REG;
        vi.vRegion.word = *VI_V_START_REG;
        vi.vBurst.word = *VI_V_BURST_REG;
        vi.xTransform.word = *VI_X_SCALE_REG;
        vi.yTransform.word = *VI_Y_SCALE_REG;
        return vi;
    }

    // Application

    Application::Application(const Core &core, const ApplicationConfiguration &appConfig) {
        Timer::initialize();
//        FileDialog::initialize();

        this->core = core;
        this->appConfig = appConfig;

        frameCounter = 0;
        threadsAvailable = std::max(std::thread::hardware_concurrency(), 1U);
        freeCamClearQueued = false;

        if (appConfig.detectDataPath) {
            this->appConfig.dataPath = userPaths.detectDataPath(appConfig.appId);
        }

        userPaths.setupPaths(this->appConfig.dataPath);

        // Open the log if we're able to access the directory.
        if (!userPaths.isEmpty() && checkDirectoryCreated(userPaths.dataPath)) {
            RT64_LOG_OPEN(userPaths.logPath.c_str());
        }

#   ifdef _WIN64
        wineDetected = Sommelier::detectWine();

        // Change the default graphics API when running under Wine to avoid using the D3D12 translation layer when possible. Recreate the default user configuration right afterwards so this new value is assigned.
        UserConfiguration::DefaultGraphicsAPI = wineDetected ? UserConfiguration::GraphicsAPI::Vulkan : UserConfiguration::GraphicsAPI::D3D12;
        userConfig = UserConfiguration();
#   else
        // Wine can't be detected in other platform's binaries.
        wineDetected = false;
#   endif
    }
    
    Application::SetupResult Application::setup(uint32_t threadId) {
#   if defined(_WIN64) && !defined(RT64_BUILD_PLUGIN)
        if (!DynamicLibraries::load()) {
            fprintf(stderr, "Failed to load dynamic libraries. Make sure the dependencies are next to the Plugin's DLL.\n");
            return SetupResult::Success;
        }
#   endif

        // Attempt to load the configuration. If it couldn't be loaded, save a new file instead.
        if (!loadConfiguration()) {
            saveConfiguration();
        }

        // Create the interpreter and its associated state.
        interpreter = std::make_unique<Interpreter>();
        state = std::make_unique<State>(core.RDRAM, core.MI_INTR_REG, core.checkInterrupts);
        interpreter->setup(state.get());

#   if SCRIPT_ENABLED
        // Create the script API and library and check if there's a compatible game script with the current ROM.
        scriptAPI = std::make_unique<ScriptAPI>(this);
        if (!userPaths.isEmpty() && checkDirectoryCreated(userPaths.gamesPath)) {
            currentScript = std::move(scriptLibrary.loadGameScript(scriptAPI.get()));
            if ((currentScript != nullptr) && !currentScript->initialize()) {
                fprintf(stderr, "Failed to initialize game script.\n");
                return false;
            }
        }
#   endif
        
        // Create a render interface with the preferred backend.
        switch (userConfig.graphicsAPI) {
        case UserConfiguration::GraphicsAPI::D3D12:
#       if defined(_WIN64) && !defined(RT64_BUILD_PLUGIN)
            renderInterface = CreateD3D12Interface();
            break;
#       else
            fprintf(stderr, "D3D12 is not supported on this platform. Please select a different Graphics API.\n");
            return SetupResult::InvalidGraphicsAPI;
#       endif
        case UserConfiguration::GraphicsAPI::Vulkan:
            renderInterface = CreateVulkanInterface();
            break;
        default:
            fprintf(stderr, "Unknown Graphics API specified in configuration.\n");
            return SetupResult::InvalidGraphicsAPI;
        }

        if (renderInterface == nullptr) {
            fprintf(stderr, "Unable to initialize graphics API.\n");
            return SetupResult::GraphicsAPINotFound;
        }

        createdGraphicsAPI = userConfig.graphicsAPI;

#   ifdef TEST_RENDER_INTERFACE
        // Execute a blocking test that creates a window and draws some geometry to test the render interface.
        RenderInterfaceTest(renderInterface.get());
#   endif

#   ifdef RT64_BUILD_PLUGIN
        const bool isPJ64 = (RT64::API.apiType == RT64::APIType::Project64);
        if (!isPJ64) {
            appWindow = std::make_unique<ApplicationWindow>();
        }
        else {
#   endif
        // Create the application window.
        const char *windowTitle = "RT64";
        appWindow = std::make_unique<ApplicationWindow>();
        if (core.window != RenderWindow{}) {
            appWindow->setup(core.window, this, threadId);
        }
        else {
            appWindow->setup(windowTitle, this);
        }
#   ifdef RT64_BUILD_PLUGIN
        }
#   endif
        // Detect refresh rate from the display the window is located at.
        appWindow->detectRefreshRate();

        // Create the render device for the window
        device = renderInterface->createDevice();
        if (device == nullptr) {
            fprintf(stderr, "Unable to find compatible graphics device.\n");
            return SetupResult::GraphicsDeviceNotFound;
        }

        // Driver workarounds.
        //
        // Wireframe artifacts have been reported when using a high-precision color format on RDNA3 GPUs in D3D12. The workaround is to switch to Vulkan if this is the case.
        bool isRDNA3 = device->getDescription().name.find("AMD Radeon RX 7") != std::string::npos;
        bool useHDRinD3D12 = (userConfig.graphicsAPI == UserConfiguration::GraphicsAPI::D3D12) && (userConfig.internalColorFormat == UserConfiguration::InternalColorFormat::Automatic) && device->getCapabilities().preferHDR;
        if (isRDNA3 && useHDRinD3D12) {
            device.reset();
            renderInterface.reset();
            renderInterface = CreateVulkanInterface();
            if (renderInterface == nullptr) {
                fprintf(stderr, "Unable to initialize graphics API.\n");
                return SetupResult::GraphicsAPINotFound;
            }

            createdGraphicsAPI = UserConfiguration::GraphicsAPI::Vulkan;

            device = renderInterface->createDevice();
            if (device == nullptr) {
                fprintf(stderr, "Unable to find compatible graphics device.\n");
                return SetupResult::GraphicsDeviceNotFound;
            }
        }

        // Call the init hook if one was attached.
        RenderHookInit *initHook = GetRenderHookInit();
        if (initHook != nullptr) {
            initHook(renderInterface.get(), device.get());
        }

        // Create all the render workers.
        drawDataUploader = std::make_unique<BufferUploader>(device.get());
        transformsUploader = std::make_unique<BufferUploader>(device.get());
        tilesUploader = std::make_unique<BufferUploader>(device.get());
        workloadVelocityUploader = std::make_unique<BufferUploader>(device.get());
        workloadTilesUploader = std::make_unique<BufferUploader>(device.get());
        framebufferGraphicsWorker = std::make_unique<RenderWorker>(device.get(), "Framebuffer Graphics", RenderCommandListType::DIRECT);
        textureComputeWorker = std::make_unique<RenderWorker>(device.get(), "Texture Compute", RenderCommandListType::COMPUTE);
        workloadGraphicsWorker = std::make_unique<RenderWorker>(device.get(), "Workload Graphics", RenderCommandListType::DIRECT);
        presentGraphicsWorker = std::make_unique<RenderWorker>(device.get(), "Present Graphics", RenderCommandListType::DIRECT);
        swapChain = presentGraphicsWorker->commandQueue->createSwapChain(appWindow->windowHandle, 2, RenderFormat::B8G8R8A8_UNORM);

        // Detect if the application should use HDR framebuffers or not.
        bool usesHDR;
        switch (userConfig.internalColorFormat) {
        case UserConfiguration::InternalColorFormat::High:
            usesHDR = true;
            break;
        case UserConfiguration::InternalColorFormat::Automatic:
            usesHDR = device->getCapabilities().preferHDR;
            break;
        case UserConfiguration::InternalColorFormat::Standard:
        default:
            usesHDR = false;
            break;
        }
        
        // Before configuring multisampling, make sure the device actually supports it for the formats we'll use. If it doesn't, turn off antialiasing in the configuration.
        const RenderSampleCounts colorSampleCounts = device->getSampleCountsSupported(RenderTarget::colorBufferFormat(usesHDR));
        const RenderSampleCounts depthSampleCounts = device->getSampleCountsSupported(RenderTarget::depthBufferFormat());
        const RenderSampleCounts commonSampleCounts = colorSampleCounts & depthSampleCounts;
        if ((commonSampleCounts & userConfig.msaaSampleCount()) == 0) {
            userConfig.antialiasing = UserConfiguration::Antialiasing::None;
        }

        // Create the shader library.
        const RenderMultisampling multisampling = RasterShader::generateMultisamplingPattern(userConfig.msaaSampleCount(), device->getCapabilities().sampleLocations);
        shaderLibrary = std::make_unique<ShaderLibrary>(usesHDR);
        shaderLibrary->setupCommonShaders(renderInterface.get(), device.get());
        shaderLibrary->setupMultisamplingShaders(renderInterface.get(), device.get(), multisampling);
        
        // Create the shader caches. Estimate the amount of shader compiler threads by trying to use about half of the system's available threads.
        const uint32_t rasterShaderThreads = std::max(threadsAvailable / 2U, 1U);
        rasterShaderCache = std::make_unique<RasterShaderCache>(rasterShaderThreads);
        rasterShaderCache->setup(device.get(), renderInterface->getCapabilities().shaderFormat, shaderLibrary.get(), multisampling);

#   if RT_ENABLED
        if (device->getCapabilities().raytracing) {
            rtShaderCache = std::make_unique<RaytracingShaderCache>(device.get(), renderInterface->getCapabilities().shaderFormat, shaderLibrary.get());
        }
#   endif

        // Create the texture cache.
        textureCache = std::make_unique<TextureCache>(textureComputeWorker.get(), shaderLibrary.get(), userConfig.developerMode);

#   if RT_ENABLED
        // Create the blue noise texture, upload it and wait for it to finish.
        std::unique_ptr<RenderBuffer> blueNoiseUploadBuffer;
        workloadGraphicsWorker->commandList->begin();
        TextureCache::setRGBA32(&blueNoiseTexture, workloadGraphicsWorker.get(), LDR_64_64_64_RGB1_BGRA8, int(sizeof(LDR_64_64_64_RGB1_BGRA8)), 512, 512, 512 * 4, blueNoiseUploadBuffer, textureCache->uploadResourcePool.get());
        workloadGraphicsWorker->commandList->end();
        workloadGraphicsWorker->execute();
        workloadGraphicsWorker->wait();
        blueNoiseUploadBuffer.reset();
#   endif
        
        // Create the queues.
        workloadQueue = std::make_unique<WorkloadQueue>();
        presentQueue = std::make_unique<PresentQueue>();

        // Create the shared resources for the queues.
        sharedQueueResources = std::make_unique<SharedQueueResources>();
        sharedQueueResources->setUserConfig(userConfig, false);
        sharedQueueResources->setEmulatorConfig(emulatorConfig);
        sharedQueueResources->setEnhancementConfig(enhancementConfig);
        sharedQueueResources->setSwapChainSize(swapChain->getWidth(), swapChain->getHeight());
        sharedQueueResources->setSwapChainRate(60); // TODO...
        sharedQueueResources->renderTargetManager.setMultisampling(multisampling);
        sharedQueueResources->renderTargetManager.setUsesHDR(usesHDR);

        WorkloadQueue::External workloadExt;
        workloadExt.device = device.get();
        workloadExt.workloadGraphicsWorker = workloadGraphicsWorker.get();
        workloadExt.workloadVelocityUploader = workloadVelocityUploader.get();
        workloadExt.workloadTilesUploader = workloadTilesUploader.get();
        workloadExt.presentQueue = presentQueue.get();
        workloadExt.sharedResources = sharedQueueResources.get();
        workloadExt.rasterShaderCache = rasterShaderCache.get();
        workloadExt.textureCache = textureCache.get();
        workloadExt.shaderLibrary = shaderLibrary.get();
        workloadExt.createdGraphicsAPI = createdGraphicsAPI;
#   if RT_ENABLED
        workloadExt.rtShaderCache = rtShaderCache.get();
        workloadExt.blueNoiseTexture = blueNoiseTexture.texture.get();
#   endif
        workloadQueue->setup(workloadExt);

        PresentQueue::External presentExt;
        presentExt.appWindow = appWindow.get();
        presentExt.device = device.get();
        presentExt.swapChain = swapChain.get();
        presentExt.presentGraphicsWorker = presentGraphicsWorker.get();
        presentExt.workloadQueue = workloadQueue.get();
        presentExt.sharedResources = sharedQueueResources.get();
        presentExt.shaderLibrary = shaderLibrary.get();
        presentQueue->setup(presentExt);

        // Configure the state to use all the created components.
        State::External stateExt;
        stateExt.app = this;
        stateExt.appWindow = appWindow.get();
        stateExt.interpreter = interpreter.get();
        stateExt.device = device.get();
        stateExt.swapChain = swapChain.get();
        stateExt.framebufferGraphicsWorker = framebufferGraphicsWorker.get();
        stateExt.shaderLibrary = shaderLibrary.get();
        stateExt.drawDataUploader = drawDataUploader.get();
        stateExt.transformsUploader = transformsUploader.get();
        stateExt.tilesUploader = tilesUploader.get();
        stateExt.workloadQueue = workloadQueue.get();
        stateExt.presentQueue = presentQueue.get();
        stateExt.sharedQueueResources = sharedQueueResources.get();
        stateExt.rasterShaderCache = rasterShaderCache.get();
        stateExt.textureCache = textureCache.get();
        stateExt.emulatorConfig = &emulatorConfig;
        stateExt.enhancementConfig = &enhancementConfig;
        stateExt.userConfig = &userConfig;
        stateExt.dlApiProfiler = &dlApiProfiler;
        stateExt.screenApiProfiler = &screenApiProfiler;
        stateExt.createdGraphicsAPI = createdGraphicsAPI;
#   if RT_ENABLED
        stateExt.rtConfig = &rtConfig;
#   endif
#   if SCRIPT_ENABLED
        stateExt.currentScript = currentScript.get();
#   endif
        state->setup(stateExt);

        // Set up the RDP 
        state->rdp->setGBI();

        return SetupResult::Success;
    }
    
    Application::~Application() { }

    void Application::processDisplayLists(uint8_t *memory, uint32_t dlStartAddress, uint32_t dlEndAddress, bool isHLE) {
        if (state->debuggerInspector.paused) {
            // TODO: It'd be necessary to parse the display list to see if it actually does a fullSync before sending the interrupt.
            state->dpInterrupt();
        }
        else {
            if (isHLE) {
                dlApiProfiler.logAndRestart();
            }

#   ifdef LOG_DISPLAY_LISTS
            RT64_LOG_PRINTF("Application::processDisplayLists(0x%X, 0x%X)", dlStartAddress, dlEndAddress);
#   endif

            ElapsedTimer displayListTimer;
            DisplayList *dlStart = reinterpret_cast<DisplayList *>(&memory[dlStartAddress]);
            DisplayList *dlEnd = (dlEndAddress > 0) ? reinterpret_cast<RT64::DisplayList *>(&memory[dlEndAddress]) : nullptr;

#       if SCRIPT_ENABLED
            if (currentScript != nullptr) {
                currentScript->processDisplayLists(reinterpret_cast<uint64_t *>(dlStart), reinterpret_cast<uint64_t *>(dlEnd));
            }
#       endif

            if (isHLE) {
                interpreter->processDisplayLists(dlStartAddress, dlStart);
            }
            else {
                interpreter->processRDPLists(dlStartAddress, dlStart, dlEnd);
            }
        }

        if (isHLE) {
            state->spInterrupt();
        }

        // Save the configuration if the state has changed it and the renderer has validated it.
        bool saveValidConfig = false;
        if (state->configurationSaveQueued) {
            std::scoped_lock configurationLock(sharedQueueResources->configurationMutex);
            saveValidConfig = sharedQueueResources->newConfigValidated;
        }

        if (saveValidConfig) {
            saveConfiguration();
            state->configurationSaveQueued = false;
        }
    }
    
    void Application::updateScreen() {
        screenApiProfiler.logAndRestart();
        state->updateScreen(core.decodeVI(), false);
    }

    bool Application::loadOfflineShaderCache(std::istream &stream) {
        return rasterShaderCache->loadOfflineList(stream);
    }

    void Application::destroyShaderCache() {
        workloadQueue->waitForWorkloadId(state->workloadId);
        presentQueue->waitForPresentId(state->presentId);
        workloadQueue->waitForIdle();
        presentQueue->waitForIdle();
        rasterShaderCache->waitForAll();
        rasterShaderCache->destroyAll();
    }
    
    void Application::updateMultisampling() {
#   if SAMPLE_LOCATIONS_REQUIRED
        // This operation does nothing if sample locations isn't supported.
        if (!device->getCapabilities().sampleLocations) {
            return;
        }
#   endif

        // Destroy shader cache by waiting for the queues to be idle.
        destroyShaderCache();

        // Recreate the multisampling shaders on the shader library.
        const RenderMultisampling multisampling = RasterShader::generateMultisamplingPattern(userConfig.msaaSampleCount(), device->getCapabilities().sampleLocations);
        shaderLibrary->setupMultisamplingShaders(renderInterface.get(), device.get(), multisampling);
        
        // Set up shader cache again with new ubershaders.
        rasterShaderCache->setup(device.get(), renderInterface->getCapabilities().shaderFormat, shaderLibrary.get(), multisampling);
        
        // Destroy the render target managers for both the state and the queues and configure them with the new multisampling.
        // This also implies the destruction of the contents of the framebuffer managers too.
        sharedQueueResources->updateMultisampling(multisampling);
        workloadQueue->updateMultisampling();
        state->updateMultisampling();

        // Wait for all pipelines of the ubershader to be ready again.
        rasterShaderCache->shaderUber->waitForPipelineCreation();
        rasterShaderCache->resetOfflineList();
    }

#ifdef _WIN32
    bool Application::windowMessageFilter(unsigned int message, WPARAM wParam, LPARAM lParam) {
        if (userConfig.developerMode && (presentQueue != nullptr) && (state != nullptr) && !FileDialog::isOpen) {
            const std::lock_guard<std::mutex> lock(presentQueue->inspectorMutex);
            if ((presentQueue->inspector != nullptr) && presentQueue->inspector->handleMessage(message, wParam, lParam)) {
                return true;
            }
        }

        switch (message) {
        case WM_KEYDOWN: {
            if (wParam == VK_F1) {
                if (userConfig.developerMode) {
                    const std::lock_guard<std::mutex> lock(presentQueue->inspectorMutex);
                    if (presentQueue->inspector == nullptr) {
                        presentQueue->inspector = std::make_unique<Inspector>(device.get(), swapChain.get(), createdGraphicsAPI);
                        if (!userPaths.isEmpty()) {
                            presentQueue->inspector->setIniPath(userPaths.imguiPath);
                        }

                        freeCamClearQueued = true;
                        appWindow->blockSdlKeyboard();
                    }
                    else if (presentQueue->inspector != nullptr) {
                        presentQueue->inspector.reset(nullptr);
                        appWindow->unblockSdlKeyboard();
                    }
                }
                else {
                    fprintf(stdout, "Inspector is not available: developer mode is not enabled in the configuration.\n");
                }

                return true;
            }
            else if (wParam == VK_F2) {
#           if RT_ENABLED
                // Only load the RT pipeline if the device supports it.
                if (device->getCapabilities().raytracing && !rtShaderCache->isSetup()) {
                    rtShaderCache->setup();
                }
#           endif
                
                workloadQueue->rtEnabled = !workloadQueue->rtEnabled;
                return true;
            }
            else if (wParam == VK_F3) {
                presentQueue->viewRDRAM = !presentQueue->viewRDRAM;
                return true;
            }
        }
        };
        
        return false;
    }

    bool Application::usesWindowMessageFilter() {
        return userConfig.developerMode;
    }
#endif

    void Application::end() {
#   if SCRIPT_ENABLED
        if (currentScript != nullptr) {
            currentScript->finalize();
        }
#   endif

        state.reset();
        workloadQueue.reset();
        presentQueue.reset();

        // Call the deinit hook if one was attached.
        RenderHookDeinit *deinitHook = GetRenderHookDeinit();
        if (deinitHook != nullptr) {
            deinitHook();
        }

        drawDataUploader.reset();
        transformsUploader.reset();
        tilesUploader.reset();
        workloadVelocityUploader.reset();
        workloadTilesUploader.reset();
        sharedQueueResources.reset();
        rasterShaderCache.reset();
#   if RT_ENABLED
        rtShaderCache.reset();
        blueNoiseTexture.texture.reset();
#   endif
        textureCache.reset();
        framebufferGraphicsWorker.reset();
        textureComputeWorker.reset();
        swapChain.reset();
        workloadGraphicsWorker.reset();
        presentGraphicsWorker.reset();
        shaderLibrary.reset();
        device.reset();
        renderInterface.reset();

        FileDialog::finish();
    }
    
    bool Application::loadConfiguration() {
        if (!appConfig.useConfigurationFile || userPaths.isEmpty()) {
            return false;
        }

        std::ifstream cfgStream(userPaths.configurationPath, std::ios_base::in);
        if (!cfgStream.is_open()) {
            return false;
        }

        bool readConfig = ConfigurationJSON::read(userConfig, cfgStream);
        userConfig.validate();
        return readConfig;
    }

    bool Application::saveConfiguration() {
        if (!appConfig.useConfigurationFile || userPaths.isEmpty() || !checkDirectoryCreated(userPaths.dataPath)) {
            return false;
        }

        std::ofstream cfgStream(userPaths.configurationPath, std::ios_base::out);
        if (!cfgStream.is_open()) {
            return false;
        }

        return ConfigurationJSON::write(userConfig, cfgStream);
    }

    bool Application::checkDirectoryCreated(const std::filesystem::path& path) {
        std::filesystem::path dirPath(path);
        return std::filesystem::is_directory(dirPath) || std::filesystem::create_directories(dirPath);
    }

    void Application::updateUserConfig(bool discardFBs) {
        sharedQueueResources->setUserConfig(userConfig, true);
    }

    void Application::updateEmulatorConfig() {
        sharedQueueResources->setEmulatorConfig(emulatorConfig);
    }

    void Application::updateEnhancementConfig() {
        sharedQueueResources->setEnhancementConfig(enhancementConfig);
    }

    void Application::setFullScreen(bool fullscreen) {
        appWindow->setFullScreen(fullscreen);
    }
};