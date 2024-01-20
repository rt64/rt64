//
// RT64
//

#include "rt64_gbi_extended.h"

#include "../include/rt64_extended_gbi.h"

namespace RT64 {
    namespace GBI_EXTENDED {
        static GBIFunction Map[G_EX_MAX];
        static bool MapInitialized = false;

        void noOp(State *state, DisplayList **dl) {
            // Does nothing.
        }

        void print(State *state, DisplayList **dl) {
            // Not implemented.
        }

        void texrectV1(State *state, DisplayList **dl) {
            ExtendedAlignment extAlignment;
            const uint8_t tile = (*dl)->p1(0, 3);
            extAlignment.leftOrigin = (*dl)->p1(3, 12);
            extAlignment.rightOrigin = (*dl)->p1(15, 12);
            const bool flip = (*dl)->p1(7, 1);
            *dl = *dl + 1;

            const int16_t ulx = (*dl)->p0(16, 16);
            const int16_t uly = (*dl)->p0(0, 16);
            const int16_t lrx = (*dl)->p1(16, 16);
            const int16_t lry = (*dl)->p1(0, 16);
            *dl = *dl + 1;

            const int16_t uls = (*dl)->p0(16, 16);
            const int16_t ult = (*dl)->p0(0, 16);
            const int16_t dsdx = (*dl)->p1(16, 16);
            const int16_t dtdy = (*dl)->p1(0, 16);
            state->rdp->drawTexRect(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, flip, extAlignment);
        }

        void fillrectV1(State *state, DisplayList **dl) {
            ExtendedAlignment extAlignment;
            extAlignment.leftOrigin = (*dl)->p1(0, 12);
            extAlignment.rightOrigin = (*dl)->p1(12, 12);
            *dl = *dl + 1;

            const int16_t ulx = (*dl)->p0(16, 16);
            const int16_t uly = (*dl)->p0(0, 16);
            const int16_t lrx = (*dl)->p1(16, 16);
            const int16_t lry = (*dl)->p1(0, 16);
            state->rdp->fillRect(ulx, uly, lrx, lry, extAlignment);
        }

        void setViewportV1(State *state, DisplayList **dl) {
            const uint16_t ori = (*dl)->p1(0, 12);
            *dl = *dl + 1;

            state->rsp->setViewport((*dl)->w1, ori, 0, 0);
        }

        void setScissorV1(State *state, DisplayList **dl) {
            ExtendedAlignment extAlignment;
            const uint8_t mode = (*dl)->p1(0, 2);
            extAlignment.leftOrigin = (*dl)->p1(2, 12);
            extAlignment.rightOrigin = (*dl)->p1(14, 12);
            *dl = *dl + 1;

            const uint16_t ulx = (*dl)->p0(16, 16);
            const uint16_t uly = (*dl)->p0(0, 16);
            const uint16_t lrx = (*dl)->p1(16, 16);
            const uint16_t lry = (*dl)->p1(0, 16);
            state->rdp->setScissor(mode, ulx, uly, lrx, lry, extAlignment);
        }
        
        void setRectAlignV1(State *state, DisplayList **dl) {
            ExtendedAlignment extAlignment;
            extAlignment.leftOrigin = (*dl)->p1(0, 12);
            extAlignment.rightOrigin = (*dl)->p1(12, 12);
            *dl = *dl + 1;

            extAlignment.leftOffset = (int16_t)(*dl)->p0(16, 16);
            extAlignment.topOffset = (int16_t)(*dl)->p0(0, 16);
            extAlignment.rightOffset = (int16_t)(*dl)->p1(16, 16);
            extAlignment.bottomOffset = (int16_t)(*dl)->p1(0, 16);
            state->rdp->setRectAlign(extAlignment);
        }

        void setViewportAlignV1(State *state, DisplayList **dl) {
            const uint16_t ori = (*dl)->p1(0, 12);
            *dl = *dl + 1;

            const int16_t x = (*dl)->p0(16, 16);
            const int16_t y = (*dl)->p0(0, 16);
            state->rsp->setViewportAlign(ori, x, y);
        }
        
        void setScissorAlignV1(State *state, DisplayList **dl) {
            ExtendedAlignment extAlignment;
            extAlignment.leftOrigin = (*dl)->p1(0, 12);
            extAlignment.rightOrigin = (*dl)->p1(12, 12);
            *dl = *dl + 1;

            extAlignment.leftOffset = (int16_t)(*dl)->p0(16, 16);
            extAlignment.topOffset = (int16_t)(*dl)->p0(0, 16);
            extAlignment.rightOffset = (int16_t)(*dl)->p1(16, 16);
            extAlignment.bottomOffset = (int16_t)(*dl)->p1(0, 16);
            *dl = *dl + 1;

            extAlignment.leftBound = (*dl)->p0(16, 16);
            extAlignment.topBound = (*dl)->p0(0, 16);
            extAlignment.rightBound = (*dl)->p1(16, 16);
            extAlignment.bottomBound = (*dl)->p1(0, 16);
            state->rdp->setScissorAlign(extAlignment);
        }

        void setRefreshRateV1(State *state, DisplayList **dl) {
            const uint16_t refreshRate = (*dl)->p1(0, 16);
            state->setRefreshRate(refreshRate);
        }

        void vertexZTestV1(State *state, DisplayList **dl) {
            const uint8_t vertexIndex = (*dl)->p1(0, 8);
            state->rsp->vertexTestZ(vertexIndex);
        }

        void endVertexZTestV1(State *state, DisplayList **dl) {
            state->rsp->endVertexTestZ();
        }

        void matrixGroupV1(State *state, DisplayList **dl) {
            const uint32_t id = (*dl)->w1;
            *dl = *dl + 1;
            const uint8_t push = (*dl)->p0(0, 1);
            const uint8_t proj = (*dl)->p0(1, 1);
            const uint8_t mode = (*dl)->p0(2, 1);
            const uint8_t pos = (*dl)->p0(3, 2);
            const uint8_t rot = (*dl)->p0(5, 2);
            const uint8_t scale = (*dl)->p0(7, 2);
            const uint8_t skew = (*dl)->p0(9, 2);
            const uint8_t persp = (*dl)->p0(11, 2);
            const uint8_t vert = (*dl)->p0(13, 2);
            const uint8_t tile = (*dl)->p0(15, 2);
            const uint8_t order = (*dl)->p0(17, 2);
            
            state->rsp->matrixId(id, push, proj, mode, pos, rot, scale, skew, persp, vert, tile, order);
        }

        void popMatrixGroupV1(State *state, DisplayList **dl) {
            const uint8_t popCount = (*dl)->p1(0, 8);
            state->rsp->popMatrixId(popCount);
        }

        void forceUpscale2DV1(State *state, DisplayList **dl) {
            const uint8_t force = (*dl)->p1(0, 1);
            state->rdp->forceUpscale2D(force);
        }

        void forceTrueBilerpV1(State *state, DisplayList **dl) {
            const uint8_t mode = (*dl)->p1(0, 2);
            state->rdp->forceTrueBilerp(mode);
        }

        void forceScaleLODV1(State *state, DisplayList **dl) {
            const uint8_t force = (*dl)->p1(0, 1);
            state->rdp->forceScaleLOD(force);
        }
        
        void forceBranchV1(State *state, DisplayList **dl) {
            const uint8_t force = (*dl)->p1(0, 1);
            state->rsp->forceBranch(force);
        }

        void setRenderToRAMV1(State *state, DisplayList **dl) {
            const uint8_t render = (*dl)->p1(0, 1);
            state->setRenderToRAM(render);
        }

        void noOpHook(State *state, DisplayList **dl) {
            uint32_t magicNumber = (*dl)->p0(0, 24);
            if (magicNumber == RT64_HOOK_MAGIC_NUMBER) {
                uint32_t hookValue = (*dl)->p1(0, 28);
                uint32_t hookOp = (*dl)->p1(28, 4);
                switch (hookOp) {
                case RT64_HOOK_OP_GETVERSION: {
                    const uint32_t rdramAddress = state->rsp->fromSegmented(hookValue);
                    uint32_t *returnRDRAM = reinterpret_cast<uint32_t *>(state->fromRDRAM(rdramAddress));
                    *returnRDRAM = G_EX_VERSION;
                    break;
                }
                case RT64_HOOK_OP_ENABLE: {
                    uint8_t extendedOpCode = uint8_t(hookValue & 0xFF);
                    if ((extendedOpCode == 0) || (extendedOpCode == (*dl)->p0(24, 8))) {
                        assert(false && "Unimplemented invalid extended opcode.");
                        // TODO: Crash the emulator with an error message indicating RT64 does not allow this extended opcode to be registered.
                    }
                    
                    state->enableExtendedGBI(extendedOpCode);
                    break;
                }
                case RT64_HOOK_OP_DISABLE:
                    state->disableExtendedGBI();
                    break;
                case RT64_HOOK_OP_DL:
                case RT64_HOOK_OP_BRANCH: {
                    if (hookOp == RT64_HOOK_OP_DL) {
                        state->pushReturnAddress(*dl);
                    }

                    const uint32_t rdramAddress = state->rsp->fromSegmented(hookValue);
                    *dl = reinterpret_cast<DisplayList *>(state->fromRDRAM(rdramAddress)) - 1;
                    break;
                }
                default:
                    assert(false && "Unknown hook operation.");
                    break;
                }
            }
        }

        void extendedOp(State *state, DisplayList **dl) {
            assert(MapInitialized);

            uint32_t opCode = (*dl)->p0(0, 24);
            if (opCode < G_EX_MAX) {
                Map[opCode](state, dl);
            }
            else {
                assert(false && "Unimplemented unrecognized opcodes.");
                // TODO: Crash the emulator with an error message indicating RT64 does not detect the opCode and it might be outdated.
            }
        }

        void initialize() {
            Map[G_EX_NOOP] = &noOp;
            Map[G_EX_PRINT] = &print;
            Map[G_EX_TEXRECT_V1] = &texrectV1;
            Map[G_EX_FILLRECT_V1] = &fillrectV1;
            Map[G_EX_SETVIEWPORT_V1] = &setViewportV1;
            Map[G_EX_SETSCISSOR_V1] = &setScissorV1;
            Map[G_EX_SETRECTALIGN_V1] = &setRectAlignV1;
            Map[G_EX_SETVIEWPORTALIGN_V1] = &setViewportAlignV1;
            Map[G_EX_SETSCISSORALIGN_V1] = &setScissorAlignV1;
            Map[G_EX_SETREFRESHRATE_V1] = &setRefreshRateV1;
            Map[G_EX_VERTEXZTEST_V1] = &vertexZTestV1;
            Map[G_EX_ENDVERTEXZTEST_V1] = &endVertexZTestV1;
            Map[G_EX_MATRIXGROUP_V1] = &matrixGroupV1;
            Map[G_EX_POPMATRIXGROUP_V1] = &popMatrixGroupV1;
            Map[G_EX_FORCEUPSCALE2D_V1] = &forceUpscale2DV1;
            Map[G_EX_FORCETRUEBILERP_V1] = &forceTrueBilerpV1;
            Map[G_EX_FORCESCALELOD_V1] = &forceScaleLODV1;
            Map[G_EX_FORCEBRANCH_V1] = &forceBranchV1;
            Map[G_EX_SETRENDERTORAM_V1] = &setRenderToRAMV1;
            MapInitialized = true;
        }
    }
};