//
// RT64
//

#pragma once

#include <unordered_map>

#include "shared/rt64_f3d_defines.h"

enum class F3DENUM {
    G_MTX_MODELVIEW,
    G_MTX_PROJECTION,
    G_MTX_MUL,
    G_MTX_LOAD,
    G_MTX_NOPUSH,
    G_MTX_PUSH,
    G_TEXTURE_ENABLE,
    G_SHADING_SMOOTH,
    G_CULL_FRONT,
    G_CULL_BACK,
    G_CULL_BOTH
};

typedef struct {
    int16_t vscale[4];
    int16_t vtrans[4];
} Vp_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint32_t ucode_boot;
    uint32_t ucode_boot_size;
    uint32_t ucode;
    uint32_t ucode_size;
    uint32_t ucode_data;
    uint32_t ucode_data_size;
    uint32_t dram_stack;
    uint32_t dram_stack_size;
    uint32_t output_buff;
    uint32_t output_buff_size;
    uint32_t data_ptr;
    uint32_t data_size;
    uint32_t yield_data_ptr;
    uint32_t yield_data_size;
} OSTask_t;