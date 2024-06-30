//
// RT64
//

#pragma once

#include <sstream>
#include <filesystem>

#include "common/rt64_common.h"
#include "common/rt64_emulator_configuration.h"
#include "common/rt64_enhancement_configuration.h"
#include "common/rt64_elapsed_timer.h"
#include "common/rt64_profiling_timer.h"
#include "common/rt64_user_paths.h"
#include "shared/rt64_point_light.h"
#include "preset/rt64_preset_draw_call.h"
#include "preset/rt64_preset_light.h"
#include "preset/rt64_preset_material.h"
#include "preset/rt64_preset_scene.h"
#include "render/rt64_render_worker.h"
#include "render/rt64_shader_library.h"
#include "rhi/rt64_render_interface.h"

#include "rt64_application_window.h"
#include "rt64_interpreter.h"
#include "rt64_shared_queue_resources.h"

#if RT_ENABLED
#   include "render/rt64_raytracing_shader_cache.h"
#endif

#if SCRIPT_ENABLED
#   include "script/rt64_script.h"
#endif

namespace RT64 {
    struct ApplicationConfiguration {
        std::filesystem::path appId = "rt64";
        std::filesystem::path dataPath;
        bool detectDataPath = true;
        bool useConfigurationFile = true;
    };

    struct Application : public ApplicationWindow::Listener {
        enum class SetupResult {
            Success,
            DynamicLibrariesNotFound,
            InvalidGraphicsAPI,
            GraphicsAPINotFound,
            GraphicsDeviceNotFound
        };

        struct Core {
            RenderWindow window;
            uint8_t *HEADER;
            uint8_t *RDRAM;
            uint8_t *DMEM;
            uint8_t *IMEM;
            uint32_t *MI_INTR_REG;
            uint32_t *DPC_START_REG;
            uint32_t *DPC_END_REG;
            uint32_t *DPC_CURRENT_REG;
            uint32_t *DPC_STATUS_REG;
            uint32_t *DPC_CLOCK_REG;
            uint32_t *DPC_BUFBUSY_REG;
            uint32_t *DPC_PIPEBUSY_REG;
            uint32_t *DPC_TMEM_REG;
            uint32_t *VI_STATUS_REG;
            uint32_t *VI_ORIGIN_REG;
            uint32_t *VI_WIDTH_REG;
            uint32_t *VI_INTR_REG;
            uint32_t *VI_V_CURRENT_LINE_REG;
            uint32_t *VI_TIMING_REG;
            uint32_t *VI_V_SYNC_REG;
            uint32_t *VI_H_SYNC_REG;
            uint32_t *VI_LEAP_REG;
            uint32_t *VI_H_START_REG;
            uint32_t *VI_V_START_REG;
            uint32_t *VI_V_BURST_REG;
            uint32_t *VI_X_SCALE_REG;
            uint32_t *VI_Y_SCALE_REG;

            void (*checkInterrupts)();

            VI decodeVI() const;
        };

        struct {
            hlslpp::float4x4 projMatrix;
            hlslpp::float4x4 viewMatrix;
            hlslpp::float4x4 invViewMatrix;
            float fovRadians;
            float nearPlane;
            float farPlane;
        } previousFrame;

        Core core;
        UserConfiguration userConfig;
        EmulatorConfiguration emulatorConfig;
        EnhancementConfiguration enhancementConfig;
        ApplicationConfiguration appConfig;
        UserConfiguration::GraphicsAPI createdGraphicsAPI;
        bool freeCamClearQueued;
        UserPaths userPaths;
        std::unique_ptr<Interpreter> interpreter;
        std::unique_ptr<State> state;
        std::unique_ptr<ApplicationWindow> appWindow;
        std::unique_ptr<RenderDevice> device;
        std::unique_ptr<RenderSwapChain> swapChain;
        std::unique_ptr<RenderWorker> framebufferGraphicsWorker;
        std::unique_ptr<BufferUploader> drawDataUploader;
        std::unique_ptr<BufferUploader> transformsUploader;
        std::unique_ptr<BufferUploader> tilesUploader;
        std::unique_ptr<BufferUploader> workloadExtrasUploader;
        std::unique_ptr<BufferUploader> workloadVelocityUploader;
        std::unique_ptr<BufferUploader> workloadTilesUploader;
        std::unique_ptr<RenderWorker> textureComputeWorker;
        std::unique_ptr<RenderWorker> workloadGraphicsWorker;
        std::unique_ptr<RenderWorker> presentGraphicsWorker;
        std::unique_ptr<RasterShaderCache> rasterShaderCache;
        std::unique_ptr<TextureCache> textureCache;
        std::unique_ptr<ShaderLibrary> shaderLibrary;
        std::unique_ptr<WorkloadQueue> workloadQueue;
        std::unique_ptr<PresentQueue> presentQueue;
        std::unique_ptr<SharedQueueResources> sharedQueueResources;
        std::unique_ptr<RenderInterface> renderInterface;
        std::atomic<bool> debuggerPaused;
        uint64_t frameCounter;
        uint32_t threadsAvailable;
        ProfilingTimer dlApiProfiler = ProfilingTimer(120);
        ProfilingTimer screenApiProfiler = ProfilingTimer(120);
        bool wineDetected;

#   if RT_ENABLED
        RaytracingConfiguration rtConfig;
        std::unique_ptr<RaytracingShaderCache> rtShaderCache;
        Texture blueNoiseTexture;
#   endif

#   if SCRIPT_ENABLED
        ScriptLibrary scriptLibrary;
        std::unique_ptr<ScriptAPI> scriptAPI;
        std::unique_ptr<ScriptBase> currentScript;
#   endif

        Application(const Core &core, const ApplicationConfiguration &appConfig);
        ~Application();
        SetupResult setup(uint32_t threadId);
        void processDisplayLists(uint8_t *memory, uint32_t dlStartAddress, uint32_t dlEndAddress, bool isHLE);
        void updateScreen();
        bool loadOfflineShaderCache(std::istream &stream);
        void destroyShaderCache();
        void updateMultisampling();
        void end();
        bool loadConfiguration();
        bool saveConfiguration();
        bool checkDirectoryCreated(const std::filesystem::path &path);
#   ifdef _WIN32
        bool windowMessageFilter(unsigned int message, WPARAM wParam, LPARAM lParam) override;
        bool usesWindowMessageFilter() override;
#   endif
        void updateUserConfig(bool discardFBs);
        void updateEmulatorConfig();
        void updateEnhancementConfig();
        void setFullScreen(bool fullscreen);
    };
};