#pragma once

#include <cassert>
#include <stdint.h>

#include "m64p_common.h"
#include "m64p_plugin.h"
#include "m64p_types.h"
#include "m64p_vidext.h"

// mupen64plus function pointers for the video extension functions
extern ptr_VidExt_InitWithRenderMode CoreVideo_InitWithRenderMode;
extern ptr_VidExt_Quit CoreVideo_Quit;
extern ptr_VidExt_SetCaption CoreVideo_SetCaption;
extern ptr_VidExt_ToggleFullScreen CoreVideo_ToggleFullScreen;
extern ptr_VidExt_ResizeWindow CoreVideo_ResizeWindow;
extern ptr_VidExt_VK_GetSurface CoreVideo_VK_GetSurface;
extern ptr_VidExt_VK_GetInstanceExtensions CoreVideo_VK_GetInstanceExtensions;
extern ptr_VidExt_SetVideoMode CoreVideo_SetVideoMode;

// TODO: move this to applicationwindow?
extern int window_width;
extern int window_height;

#ifdef _WIN32
namespace pj64 {
    // Prevent PJ64's definitions from conflicting with the implementations
#   define InitiateGFX InitiateGFXPJ64
#   define RomOpen RomOpenPJ64
#   include "project64/Video.h"
#   undef InitiateGFX
#   undef RomOpen
};
#endif

#include "hle/rt64_application.h"

#define DP_STATUS_XBUS_DMA          0x01
#define DP_STATUS_FREEZE            0x02

typedef GFX_INFO MupenGraphicsInfo;
#ifdef _WIN32
typedef pj64::GFX_INFO Project64GraphicsInfo;
#endif

struct PluginGraphicsInfo {
    union {
        MupenGraphicsInfo mupen64plus;
#ifdef _WIN32
        Project64GraphicsInfo project64;
#endif
    };
};

namespace RT64 {
    enum class APIType {
        Native,
        Mupen64Plus,
        Project64,
    };

    struct APIContainer {
        std::unique_ptr<Application> app;
        APIType apiType;
    };

    extern APIContainer API;

    template<typename T>
    void InitiateGFXCore(Application::Core &appCore, T info) {
        appCore.HEADER = info.HEADER;
        appCore.RDRAM = info.RDRAM;
        appCore.DMEM = info.DMEM;
        appCore.IMEM = info.IMEM;
        appCore.MI_INTR_REG = info.MI_INTR_REG;
        appCore.DPC_START_REG = info.DPC_START_REG;
        appCore.DPC_END_REG = info.DPC_END_REG;
        appCore.DPC_CURRENT_REG = info.DPC_CURRENT_REG;
        appCore.DPC_STATUS_REG = info.DPC_STATUS_REG;
        appCore.DPC_CLOCK_REG = info.DPC_CLOCK_REG;
        appCore.DPC_BUFBUSY_REG = info.DPC_BUFBUSY_REG;
        appCore.DPC_PIPEBUSY_REG = info.DPC_PIPEBUSY_REG;
        appCore.DPC_TMEM_REG = info.DPC_TMEM_REG;
        appCore.VI_STATUS_REG = info.VI_STATUS_REG;
        appCore.VI_ORIGIN_REG = info.VI_ORIGIN_REG;
        appCore.VI_WIDTH_REG = info.VI_WIDTH_REG;
        appCore.VI_INTR_REG = info.VI_INTR_REG;
        appCore.VI_V_CURRENT_LINE_REG = info.VI_V_CURRENT_LINE_REG;
        appCore.VI_TIMING_REG = info.VI_TIMING_REG;
        appCore.VI_V_SYNC_REG = info.VI_V_SYNC_REG;
        appCore.VI_H_SYNC_REG = info.VI_H_SYNC_REG;
        appCore.VI_LEAP_REG = info.VI_LEAP_REG;
        appCore.VI_H_START_REG = info.VI_H_START_REG;
        appCore.VI_V_START_REG = info.VI_V_START_REG;
        appCore.VI_V_BURST_REG = info.VI_V_BURST_REG;
        appCore.VI_X_SCALE_REG = info.VI_X_SCALE_REG;
        appCore.VI_Y_SCALE_REG = info.VI_Y_SCALE_REG;
        appCore.checkInterrupts = info.CheckInterrupts;
    }
};