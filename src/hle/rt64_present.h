//
// RT64
//

#pragma once

#include "gui/rt64_debugger_inspector.h"

#include "rt64_framebuffer_manager.h"
#include "rt64_vi.h"

namespace RT64 {
    struct DebuggerFramebuffer {
        uint32_t address = 0;
        bool view = false;
    };

    struct Present {
        VI screenVI;
        std::vector<uint8_t> storage;
        std::vector<FramebufferOperation> fbOperations;
        DebuggerFramebuffer debuggerFramebuffer;
        bool paused = false;
        uint64_t presentId = 0;
        uint64_t workloadId = 0;
    };
};