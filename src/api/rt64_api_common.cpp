//
// RT64
//

#include "m64p_common.h"
#include "m64p_plugin.h"
#include "m64p_types.h"

#include "rt64_api_common.h"

namespace RT64 {
    // Global container for the application.
    APIContainer API;
};

DLLEXPORT void CALL ProcessRDPList(void) {
    const RT64::Application::Core &core = RT64::API.app->core;
    uint32_t &dpcStartReg = *core.DPC_START_REG;
    uint32_t &dpcCurReg = *core.DPC_CURRENT_REG;
    uint32_t &dpcEndReg = *core.DPC_END_REG;
    uint32_t &dpcStatusReg = *core.DPC_STATUS_REG;
    bool xbus = dpcStatusReg & DP_STATUS_XBUS_DMA;
    uint8_t *memory = xbus ? core.DMEM : core.RDRAM;

#ifdef LOG_PLUGIN_API_CALLS
    RT64_LOG_PRINTF("ProcessRDPList() 0x%08X", dpcCurReg);
#endif

    dpcStatusReg &= ~DP_STATUS_FREEZE;
    RT64::API.app->processDisplayLists(memory, dpcCurReg, dpcEndReg, false);

    // Update registers to the end.
    dpcStartReg = dpcEndReg;
    dpcCurReg = dpcEndReg;
}

DLLEXPORT void CALL ProcessDList(void) {
    const RT64::Application::Core &core = RT64::API.app->core;
    const uint8_t *DMEM = core.DMEM;
    const OSTask_t *osTask = reinterpret_cast<const OSTask_t *>(&DMEM[0x0FC0]);
#ifdef LOG_PLUGIN_API_CALLS
    RT64_LOG_PRINTF("ProcessDList() 0x%08X", osTask->data_ptr);
#endif

    RT64::API.app->state->rsp->reset();
    RT64::API.app->interpreter->loadUCodeGBI(osTask->ucode, osTask->ucode_data, true);
    RT64::API.app->processDisplayLists(core.RDRAM, osTask->data_ptr, 0, true);
}

DLLEXPORT void CALL UpdateScreen(void) {
#ifdef LOG_PLUGIN_API_CALLS
    RT64_LOG_PRINTF("UpdateScreen()");
#endif
    RT64::API.app->updateScreen();
}

DLLEXPORT void CALL ChangeWindow(void) {
    const bool isPJ64 = (RT64::API.apiType == RT64::APIType::Project64);
    if (!isPJ64) {
        CoreVideo_ToggleFullScreen();
    } else {
        RT64::ApplicationWindow *appWindow = RT64::API.app->appWindow.get();
        appWindow->setFullScreen(!appWindow->fullScreen);
    }
}
