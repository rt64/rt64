//
// RT64
//

#include "rt64_gbi_f3ddkr.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_f3d.h"

namespace RT64 {
    namespace GBI_F3DDKR {
        void matrix(State *state, DisplayList **dl) {
            uint8_t index = uint8_t((*dl)->p0(16, 8));
            assert((index & (sizeof(FixedMatrix) - 1)) == 0 && "Index must be aligned to the matrix array.");

            index = index / sizeof(FixedMatrix);
            assert(index < 3 && "Index out of bounds.");

            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            memcpy(&state->rsp->DKR.matrices[index], state->fromRDRAM(rdramAddress), sizeof(FixedMatrix));
            state->rsp->DKR.matricesSegmentedAddresses[index] = (*dl)->w1;
            state->rsp->DKR.matricesRDRAMAddresses[index] = rdramAddress;
            state->rsp->DKR.matrixIndex = index;
            state->rsp->DKR.matrixChanged = true;
        }

        void vertex(State *state, DisplayList **dl) {
            uint32_t mi = state->rsp->DKR.matrixIndex;
            if (state->rsp->DKR.matrixChanged) {
                state->rsp->matrix(&state->rsp->DKR.matrices[mi], state->rsp->loadMask, state->rsp->DKR.matricesSegmentedAddresses[mi], state->rsp->DKR.matricesRDRAMAddresses[mi]);
                state->rsp->DKR.matrixChanged = false;
            }
            
            // TODO: Review append and billboard behavior.
            uint8_t vtxCount = uint8_t((*dl)->p0(19, 5)) + 1;
            uint8_t dmaOffset = uint8_t((*dl)->p0(16, 3)) & 0x6;
            uint8_t append = uint8_t((*dl)->p0(16, 1));
            if (append && state->rsp->DKR.billboard) {
                state->rsp->DKR.vertexCount = 1;
            }
            else if (!append) {
                state->rsp->DKR.vertexCount = 0;
            }

            uint32_t address = state->rsp->fromSegmentedMasked((*dl)->w1) + dmaOffset;
            uint8_t *RDRAM = state->RDRAM;
            uint32_t offset = 0;
            uint8_t dstIndex = state->rsp->DKR.vertexCount;
            for (uint32_t i = 0; i < vtxCount; i++) {
                RSP::Vertex &vertex = state->rsp->vertices[dstIndex + i];
                vertex.x = *(int16_t *)(&RDRAM[(address + 0) ^ 2]);
                vertex.y = *(int16_t *)(&RDRAM[(address + 2) ^ 2]);
                vertex.z = *(int16_t *)(&RDRAM[(address + 4) ^ 2]);
                vertex.color.r = RDRAM[(address + 6) ^ 3];
                vertex.color.g = RDRAM[(address + 7) ^ 3];
                vertex.color.b = RDRAM[(address + 8) ^ 3];
                vertex.color.a = RDRAM[(address + 9) ^ 3];
                address += 10;
            }

            state->rsp->setVertexCommon<true>(dstIndex, dstIndex + vtxCount);
            state->rsp->DKR.vertexCount += vtxCount;

            if (state->rsp->DKR.billboard) {
                uint32_t anchorIndex = state->rsp->indices[0];
                for (uint32_t i = 0; i < vtxCount; i++) {
                    state->rsp->DKR.billboardAnchors[dstIndex + i] = anchorIndex;
                }
            }
            else {
                for (uint32_t i = 0; i < vtxCount; i++) {
                    state->rsp->DKR.billboardAnchors[dstIndex + i] = UINT32_MAX;
                }
            }
        }

        void runDmaDl(State *state, DisplayList **dl) {
            uint8_t commandCount = (*dl)->p0(16, 8);
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            DisplayList *dlStart = reinterpret_cast<DisplayList *>(state->fromRDRAM(rdramAddress));
            state->ext.interpreter->runRDPLists(dlStart, dlStart + commandCount);
        }

        struct TriDKR {
            uint8_t v2;
            uint8_t v1;
            uint8_t v0;
            uint8_t backface;
            uint32_t ts0;
            uint32_t ts1;
            uint32_t ts2;
        };

        inline void checkBillboardAnchor(State *state, std::vector<uint32_t> &billboardIndices, uint32_t vtxIndex) {
            uint32_t anchorIndex = state->rsp->DKR.billboardAnchors[vtxIndex];
            if (anchorIndex == UINT32_MAX) {
                return;
            }

            assert(anchorIndex != state->rsp->indices[vtxIndex]);
            billboardIndices.emplace_back(state->rsp->indices[vtxIndex]);
            billboardIndices.emplace_back(anchorIndex);
        }

        void triN(State *state, DisplayList **dl) {
            uint8_t triCount = uint8_t((*dl)->p0(20, 4)) + 1;
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            state->rsp->textureState.on = uint8_t((*dl)->p0(16, 1));

            const int workloadCursor = state->ext.workloadQueue->writeCursor;
            Workload &workload = state->ext.workloadQueue->workloads[workloadCursor];
            std::vector<uint32_t> &billboardIndices = workload.drawData.billboardIndices;
            TriDKR *tris = reinterpret_cast<TriDKR *>(state->fromRDRAM(rdramAddress));
            bool cullingEnabled = (state->rsp->geometryModeStack[state->rsp->geometryModeStackSize - 1] & state->rsp->cullFrontMask);
            for (uint32_t i = 0; i < triCount; i++) {
                bool triCull = (tris[i].backface & 0x40) == 0;
                if (triCull && !cullingEnabled) {
                    state->rsp->setGeometryMode(state->rsp->cullFrontMask);
                    cullingEnabled = true;
                }
                else if (!triCull && cullingEnabled) {
                    state->rsp->clearGeometryMode(state->rsp->cullFrontMask);
                    cullingEnabled = false;
                }

                state->rsp->modifyVertex(tris[i].v0, G_MWO_POINT_ST, tris[i].ts0);
                state->rsp->modifyVertex(tris[i].v1, G_MWO_POINT_ST, tris[i].ts1);
                state->rsp->modifyVertex(tris[i].v2, G_MWO_POINT_ST, tris[i].ts2);

                checkBillboardAnchor(state, billboardIndices, tris[i].v0);
                checkBillboardAnchor(state, billboardIndices, tris[i].v1);
                checkBillboardAnchor(state, billboardIndices, tris[i].v2);

                state->rsp->drawIndexedTri(tris[i].v2, tris[i].v1, tris[i].v0);
            }

            state->rsp->DKR.vertexCount = 0;
        }

        void moveWord(State *state, DisplayList **dl) {
            uint8_t type = (*dl)->p0(0, 8);
            switch (type) {
            case F3DDKR_G_MW_BILLBOARD:
                state->rsp->DKR.billboard = (*dl)->w1 & 0x01;
                break;
            case F3DDKR_G_MW_MVMATRIX:
                assert(((*dl)->w1 & (sizeof(FixedMatrix) - 1)) == 0 && "Index must be aligned to the matrix array.");
                state->rsp->DKR.matrixIndex = (*dl)->w1 / sizeof(FixedMatrix);
                state->rsp->DKR.matrixChanged = true;
                break;
            default:
                GBI_F3D::moveWord(state, dl);
                break;
            }
        }

        void reset(State *state) {
            GBI_F3D::reset(state);

            // Projection matrix is seemingly unused.
            state->rsp->viewMatrixStack[0] = hlslpp::float4x4::identity();
            state->rsp->projMatrixStack[0] = hlslpp::float4x4::identity();
            state->rsp->viewProjMatrixStack[0] = hlslpp::float4x4::identity();
        }

        void setup(GBI *gbi) {
            GBI_F3D::setup(gbi);

            gbi->map[F3D_G_MTX] = &matrix;
            gbi->map[F3D_G_VTX] = &vertex;
            gbi->map[F3DDKR_G_DMADL] = &runDmaDl;
            gbi->map[F3DDKR_G_TRIN] = &triN;
            gbi->map[F3D_G_MOVEWORD] = &moveWord;

            gbi->resetFromTask = &reset;
        }
    }
};