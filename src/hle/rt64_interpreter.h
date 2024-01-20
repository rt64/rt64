//
// RT64
//

#pragma once

#include "rt64_state.h"

#include "gbi/rt64_f3d.h"
#include "gbi/rt64_gbi.h"

namespace RT64 {
    struct Interpreter {
        State *state;
        GBIManager gbiManager;
        GBI *hleGBI;
        uint8_t extendedOpCode = 0;
        GBIFunction extendedFunction = nullptr;

        struct {
            uint32_t textAddress = 0;
            uint32_t dataAddress = 0;
        } UCode;

        Interpreter();
        void setup(State *state);
        void loadUCodeGBI(uint32_t textAddress, uint32_t dataAddress, bool resetFromTask);
        void processRDPLists(uint32_t dlStartAdddress, DisplayList* dlStart, DisplayList* dlEnd);
        void processDisplayLists(uint32_t dlStartAdddress, DisplayList *dlStart);
    };
};