//
// RT64
//

#include "rt64_gbi.h"

#include <cassert>
#include <cinttypes>

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dgolden.h"
#include "rt64_gbi_f3dpd.h"
#include "rt64_gbi_f3dex.h"
#include "rt64_gbi_f3dex2.h"
#include "rt64_gbi_f3dwave.h"
#include "rt64_gbi_f3dzex2.h"
#include "rt64_gbi_l3dex2.h"
#include "rt64_gbi_rdp.h"
#include "rt64_gbi_s2dex.h"
#include "rt64_gbi_s2dex2.h"

#define GBI_UNKNOWN_HASH_NUMB 0xFFFFFFFFFFFFFFFFULL

namespace RT64 {
    // DisplayList

    DisplayList::DisplayList() {
        w0 = w1 = 0;
    }

    uint32_t DisplayList::p0(uint8_t pos, uint8_t bits) const {
        return ((w0 >> pos) & ((0x01 << bits) - 1));
    }

    uint32_t DisplayList::p1(uint8_t pos, uint8_t bits) const {
        return ((w1 >> pos) & ((0x01 << bits) - 1));
    }

    // ****************************************************************************************
    // * Database of known GBI versions.                                                      *
    // ****************************************************************************************
    // 
    //                  Constant                      Identifier                                   UCode                    LowP    NoN     ReJ     MVP     Point
    // 
    const GBIInstance   F3D_SDK_E                 = { "2.0D, 04-01-96 (F3D SDK 2.0E)",             GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_SDK_F                 = { "2.0D, 04-01-96 (F3D SDK 2.0F)",             GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_SDK_UNKNOWN_G         = { "2.0G, 09-30-96 (F3D SDK Unknown)",          GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_SDK_UNKNOWN_H         = { "2.0H, 02-12-97 (F3D SDK Unknown)",          GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_FIFO_SDK_E            = { "2.0D, 04-01-96 (F3D.fifo SDK 2.0E)",        GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_FIFO_SDK_F            = { "2.0D, 04-01-96 (F3D.fifo SDK 2.0F)",        GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_NON_SDK_E             = { "2.0D, 04-01-96 (F3D.NoN SDK 2.0E)",         GBIUCode::F3D,         { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_NON_SDK_UNKNOWN_D     = { "2.0D, 04-01-96 (F3D.NoN SDK Unknown)",      GBIUCode::Unknown,     { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_NON_SDK_UNKNOWN_G     = { "2.0G, 09-30-96 (F3D.NoN SDK Unknown)",      GBIUCode::F3D,         { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_NON_FIFO_SDK_E        = { "2.0D, 04-01-96 (F3D.NoN.fifo SDK 2.0E)",    GBIUCode::F3D,         { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_BC                    = { "2.0D, 04-01-96 (F3D Blast Corps)",          GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_FB                    = { "2.0G, 09-30-96 (F3D Freaky Boy Proto)",     GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_SM64                  = { "2.0D, 04-01-96 (F3D.fifo SM64)",            GBIUCode::F3D,         { false,  false,  false,  false,  false } };
    const GBIInstance   F3D_SM64_FINAL            = { "2.0H, 02-12-97 (F3D.fifo SM64 SH/iQue)",    GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_WAYNE                 = { "2.0D, 04-01-96 (F3D WG 3D Hockey)",         GBIUCode::F3D,         { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_GOLDEN                = { "SW Version: 2.0G, 09-30-96 (GE007)",        GBIUCode::F3DGOLDEN,   { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3D_PD                    = { "SW Version: Unknown (PD)",                  GBIUCode::F3DPD,       { false,  false,  false,  false,  false } };
    const GBIInstance   F3D_WAVE                  = { "SW Version: 2.0D, 04-01-96 (Wave Race)",    GBIUCode::F3DWAVE,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_0_95                = { "F3DEX 0.95",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_0_95                = { "F3DLX 0.95",                                GBIUCode::F3DEX,       { true,   false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_0_96                = { "F3DEX 0.96",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLP_0_96_REJ            = { "F3DLP 0.96.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_1_00                = { "F3DEX 1.00",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_1_21                = { "F3DEX 1.21",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX_1_21_A              = { "F3DEX 1.21 (Variant)",                      GBIUCode::F3DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_21                = { "F3DLX 1.21",                                GBIUCode::F3DEX,       { true,   false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_21_REJ            = { "F3DLX 1.21.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } };
    const GBIInstance   F3DLP_1_21_REJ            = { "F3DLP 1.21.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_22                = { "F3DLX 1.22",                                GBIUCode::F3DEX,       { true,   false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_1_23                = { "F3DEX 1.23",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX_1_23_A              = { "F3DEX 1.23 (Variant)",                      GBIUCode::F3DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_23                = { "F3DLX 1.23",                                GBIUCode::F3DEX,       { true,   false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_23_A              = { "F3DLX 1.23 (Variant)",                      GBIUCode::F3DEX,       { true,   false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_23_REJ            = { "F3DLX 1.23.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_1_23_REJ_A          = { "F3DLX 1.23.Rej (Variant)",                  GBIUCode::F3DEX,       { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLP_1_23_REJ            = { "F3DLP 1.23.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DTEXA_1_23              = { "F3DTEX/A 1.23",                             GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_0_96            = { "F3DEX.NoN 0.96",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_00            = { "F3DEX.NoN 1.00",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_NON_1_00            = { "F3DLX.NoN 1.00",                            GBIUCode::F3DEX,       { true,   true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_21            = { "F3DEX.NoN 1.21",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_21_A          = { "F3DEX.NoN 1.21 (Variant)",                  GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_NON_1_21            = { "F3DLX.NoN 1.21",                            GBIUCode::F3DEX,       { true,   true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_22            = { "F3DEX.NoN 1.22",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX_NON_1_22_A          = { "F3DEX.NoN 1.22 (Variant)",                  GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_23            = { "F3DEX.NoN 1.23",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX_NON_1_23_A          = { "F3DEX.NoN 1.23 (Variant)",                  GBIUCode::F3DEX,       { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_NON_1_23            = { "F3DLX.NoN 1.23",                            GBIUCode::F3DEX,       { true,   true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX_NON_1_23_A          = { "F3DLX.NoN 1.23 (Variant)",                  GBIUCode::F3DEX,       { true,   true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_03          = { "F3DEX2.fifo 2.03",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_FIFO_2_03_REJ      = { "F3DLX2.fifo 2.03.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DFLX2_FIFO_2_03F_REJ    = { "F3DFLX2.fifo 2.03F.ReJ",                    GBIUCode::Unknown,     { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_04          = { "F3DEX2.fifo 2.04",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_04H         = { "F3DEX2.fifo 2.04H",                         GBIUCode::F3DEX2,      { false,  false,  false,  true,   false } };
    const GBIInstance   F3DEX2_FIFO_2_05          = { "F3DEX2.fifo 2.05",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_05_REJ      = { "F3DEX2.fifo 2.05.Rej",                      GBIUCode::F3DEX2,      { false,  false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_FIFO_2_05_REJ      = { "F3DLX2.fifo 2.05.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DAM2_FIFO_2_05          = { "F3DAM2.fifo 2.05",                          GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_06          = { "F3DEX2.fifo 2.06",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_06_REJ      = { "F3DEX2.fifo 2.06.Rej",                      GBIUCode::F3DEX2,      { false,  false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_FIFO_2_06_REJ      = { "F3DLX2.fifo 2.06.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_07          = { "F3DEX2.fifo 2.07",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_08          = { "F3DEX2.fifo 2.08",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_08_REJ      = { "F3DEX2.fifo 2.08.Rej",                      GBIUCode::F3DEX2,      { false,  false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_FIFO_2_08_REJ      = { "F3DLX2.fifo 2.08.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_08_CRUISN   = { "F3DEX2.fifo 2.08 (Cruis'n Exotica)",        GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_FIFO_2_08_PL       = { "F3DEX2.fifo 2.08 (POSLIGHT)",               GBIUCode::F3DEX2,      { false,  false,  false,  false,  true  } };
    const GBIInstance   F3DEX2_XBUS_2_05          = { "F3DEX2.xbus 2.05",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_XBUS_2_07          = { "F3DEX2.xbus 2.07",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_XBUS_2_07_REJ      = { "F3DLX2.xbus 2.07.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_XBUS_2_08          = { "F3DEX2.xbus 2.08",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DLX2_XBUS_2_08_REJ      = { "F3DLX2.xbus 2.08.Rej",                      GBIUCode::F3DEX2,      { true,   false,  true,   false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_03      = { "F3DEX2.NoN.fifo 2.03",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_04      = { "F3DEX2.NoN.fifo 2.04",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_05      = { "F3DEX2.NoN.fifo 2.05",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX2_NON_FIFO_ACCLAIM   = { "F3DEX2.NoN.fifo 2.05 (Acclaim)",            GBIUCode::Unknown,     { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_06      = { "F3DEX2.NoN.fifo 2.06",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_07      = { "F3DEX2.NoN.fifo 2.07",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_FIFO_2_08      = { "F3DEX2.NoN.fifo 2.08",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX2_NON_FIFO_2_08H     = { "F3DEX2.NoN.fifo 2.08H",                     GBIUCode::F3DEX2,      { false,  true,   false,  true,   false } };
    const GBIInstance   F3DEX2_NON_XBUS_2_06      = { "F3DEX2.NoN.xbus 2.06",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DEX2_NON_XBUS_2_08      = { "F3DEX2.NoN.xbus 2.08",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } }; // Needs confirmation.
    const GBIInstance   F3DZEX2_NON_FIFO_2_06H    = { "F3DZEX2.NoN.fifo 2.06H",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  false } };
    const GBIInstance   F3DZEX2_NON_FIFO_2_08I    = { "F3DZEX2.NoN.fifo 2.08I",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  true  } };
    const GBIInstance   F3DZEX2_NON_FIFO_2_08J    = { "F3DZEX2.NoN.fifo 2.08J",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  true  } };
    const GBIInstance   S2D                       = { "2.0H, 02-12-97 (S2D)",                      GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2D_A                     = { "2.0H, 02-12-97 (S2D Variant)",              GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2D_FIFO                  = { "2.0H, 02-12-97 (S2D.fifo)",                 GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2D_FIFO_A                = { "2.0H, 02-12-97 (S2D.fifo Variant)",         GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX_1_03                = { "S2DEX 1.03",                                GBIUCode::S2DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX_1_05                = { "S2DEX 1.05",                                GBIUCode::S2DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX_1_06                = { "S2DEX 1.06",                                GBIUCode::S2DEX,       { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX_1_07                = { "S2DEX 1.07",                                GBIUCode::S2DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_04          = { "S2DEX2.fifo 2.04",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_05          = { "S2DEX2.fifo 2.05",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_05_SAFE     = { "S2DEX2.fifo 2.05 [Safe]",                   GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_06          = { "S2DEX2.fifo 2.06",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX2_FIFO_2_07          = { "S2DEX2.fifo 2.07",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX2_FIFO_2_08          = { "S2DEX2.fifo 2.08",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_XBUS_2_06          = { "S2DEX2.xbus 2.06",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   S2DEX2_XBUS_2_08          = { "S2DEX2.xbus 2.08",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3D_BC                    = { "2.0D, 04-01-96 (L3D Blast Corps)",          GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX_1_00                = { "L3DEX 1.00",                                GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX_1_21                = { "L3DEX 1.21",                                GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX_1_23                = { "L3DEX 1.23",                                GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX_1_23_A              = { "L3DEX 1.23 (Variant)",                      GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX2_FIFO_2_05          = { "L3DEX2.fifo 2.05",                          GBIUCode::L3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX2_FIFO_ACCLAIM       = { "L3DEX2.fifo 2.05 (Acclaim)",                GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   L3DEX2_FIFO_2_08          = { "L3DEX2.fifo 2.08",                          GBIUCode::L3DEX2,      { false,  false,  false,  false,  false } }; // Needs confirmation.
    const GBIInstance   ZSORTP_0_33               = { "ZSortp 0.33",                               GBIUCode::Unknown,     { false,  false,  false,  false,  false } }; // Needs confirmation.
    
    // ****************************************************************************************
    // * Database of known UCode text and data segments.                                      *
    // ****************************************************************************************
    // 
    //                  Length      Hash                    Known instances               
    //     
    static std::array<GBISegment, 94> textSegments = {
            GBISegment{ 0x1408,     0x9C0926F5E466BE70ULL,  { &F3D_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x1400,     0x34EAA6E921BCF1B2ULL,  { &F3D_SDK_F, &F3D_SDK_UNKNOWN_G, &F3D_SDK_UNKNOWN_H } }, // Needs confirmation.
            GBISegment{ 0x1408,     0x3E05E9BBE814C700ULL,  { &F3D_FIFO_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x1428,     0x3343EA81180A60EAULL,  { &F3D_NON_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x13F8,     0xDF4FFEDA859B3E81ULL,  { &F3D_NON_SDK_UNKNOWN_D } }, // Needs confirmation.
            GBISegment{ 0x1418,     0x8E49F8C85F8D80CBULL,  { &F3D_NON_SDK_UNKNOWN_G } }, // Needs confirmation.
            GBISegment{ 0x1428,     0x048153ADD9701E33ULL,  { &F3D_NON_FIFO_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x1418,     0x4039FC02BC24A2F3ULL,  { &F3D_BC } }, // Needs confirmation.
            GBISegment{ 0x1408,     0xF50165C013FCB8A2ULL,  { &F3D_SM64 } },
            GBISegment{ 0x13F8,     0x7D8EB8BDCAE7DF81ULL,  { &F3D_SM64_FINAL, &F3D_FIFO_SDK_F, &F3D_FB } }, // Needs confirmation.
            GBISegment{ 0x1400,     0x2963554DB650E3B2ULL,  { &F3D_WAYNE } }, // Needs confirmation.
            GBISegment{ 0x1418,     0xAEBF9966DD0486DDULL,  { &F3D_PD } },
            GBISegment{ 0x1420,     0xEDA47A4C2B7E69F8ULL,  { &F3D_GOLDEN } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x547A4F6CEDE3C737ULL,  { &F3D_WAVE } }, // Needs confirmation.
            GBISegment{ 0x13E0,     0x28825EEF49EE29CFULL,  { &F3DEX_0_95 } }, // Needs confirmation.
            GBISegment{ 0x1410,     0xF1CC01CCC3607D27ULL,  { &F3DLX_0_95 } }, // Needs confirmation.
            GBISegment{ 0x13A0,     0x2DAE911B08F94FCFULL,  { &F3DEX_0_96, &F3DEX_1_00 } }, // Needs confirmation.
            GBISegment{ 0x1230,     0x5762888C48511FB7ULL,  { &F3DLP_0_96_REJ } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x9A7772037D709388ULL,  { &F3DEX_1_21, &F3DEX_1_23_A } },
            GBISegment{ 0x1430,     0x2BDA7A08A7E967D7ULL,  { &F3DEX_1_21_A } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x631C3DEB122D3D90ULL,  { &F3DLX_1_21, &F3DLX_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x13D0,     0x1BEA638E869B0195ULL,  { &F3DLX_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x13C8,     0x57AF272D794B1706ULL,  { &F3DLP_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x777E1616FEE67AEDULL,  { &F3DLX_1_22 } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xAC03DE5B7B1E710FULL,  { &F3DEX_1_23 } },
            GBISegment{ 0x1430,     0x158A39AE852C6A07ULL,  { &F3DLX_1_23 } }, // Needs confirmation.
            GBISegment{ 0x13D0,     0xDE1E6BCF9EBFBAE9ULL,  { &F3DLX_1_23_REJ } }, // Needs confirmation.
            GBISegment{ 0x13D0,     0xBDC34FD53910D3CCULL,  { &F3DLX_1_23_REJ_A } }, // Needs confirmation.
            GBISegment{ 0x13C8,     0x9CA29A9ADD6CFC0BULL,  { &F3DLP_1_23_REJ } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xEE7CC90B5F7AB1D9ULL,  { &F3DTEXA_1_23 } }, // Needs confirmation.
            GBISegment{ 0x13C0,     0x600945CCAE7BFEC8ULL,  { &F3DEX_NON_0_96, &F3DEX_NON_1_00 } }, // Needs confirmation.
            GBISegment{ 0x13E8,     0x5FB61B550CB0D50AULL,  { &F3DLX_NON_1_00 } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x454B7C0482C64F7FULL,  { &F3DEX_NON_1_21, &F3DEX_NON_1_22, &F3DEX_NON_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xB9D3CD38EEE33417ULL,  { &F3DEX_NON_1_21_A } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xB5535AFA4144CE0BULL,  { &F3DLX_NON_1_21, &F3DLX_NON_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x14B0,     0xEC5D1D98AEB0EFF6ULL,  { &F3DEX_NON_1_22_A } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xA0132D3A6104A582ULL,  { &F3DEX_NON_1_23 } }, // Needs confirmation.
            GBISegment{ 0x1430,     0x81182AB598274EE9ULL,  { &F3DLX_NON_1_23 } }, // Needs confirmation.
            GBISegment{ 0x1370,     0xB15C4DE0C3534F47ULL,  { &F3DEX2_FIFO_2_03 } }, // Needs confirmation.
            GBISegment{ 0x1188,     0x4AEEB7B9E57E2D2EULL,  { &F3DLX2_FIFO_2_03_REJ } }, // Needs confirmation.
            GBISegment{ 0x1188,     0x2E8975DFB266FFA1ULL,  { &F3DFLX2_FIFO_2_03F_REJ } }, // Needs confirmation.
            GBISegment{ 0x1390,     0xF2931E7E69049A7AULL,  { &F3DEX2_FIFO_2_04 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x15C2462591E78D2BULL,  { &F3DEX2_FIFO_2_04H } }, // Needs confirmation.
            GBISegment{ 0x1390,     0xBA192DFA28437D3DULL,  { &F3DEX2_FIFO_2_05 } },
            GBISegment{ 0x1188,     0x462D273E7C98CAC7ULL,  { &F3DEX2_FIFO_2_05_REJ, &F3DEX2_FIFO_2_06_REJ } }, // Needs confirmation.
            GBISegment{ 0x1188,     0x87A4C67F1885E875ULL,  { &F3DLX2_FIFO_2_05_REJ, &F3DLX2_FIFO_2_06_REJ } }, // Needs confirmation.
            GBISegment{ 0x1188,     0x6F800A00CF591778ULL,  { &F3DAM2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x0B8FE363267ADCB4ULL,  { &F3DEX2_FIFO_2_06 } },
            GBISegment{ 0x1390,     0x8C1C9814E75E1B4BULL,  { &F3DEX2_FIFO_2_07 } },
            GBISegment{ 0x1390,     0xCF55FAE288BFE48DULL,  { &F3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1188,     0xB9FBB8E71F72B596ULL,  { &F3DEX2_FIFO_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x1188,     0x5235C026EC14B4FBULL,  { &F3DLX2_FIFO_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x1378,     0xC2AE06B691610132ULL,  { &F3DEX2_FIFO_2_08_CRUISN } }, // Needs confirmation.
            GBISegment{ 0x1630,     0x4588323F6F7E7720ULL,  { &F3DEX2_FIFO_2_08_PL } },
            GBISegment{ 0x13A0,     0xD734CFD78F6CCD45ULL,  { &F3DEX2_XBUS_2_05 } }, // Needs confirmation.
            GBISegment{ 0x13A0,     0x0A4F40D3A58CB674ULL,  { &F3DEX2_XBUS_2_07 } }, // Needs confirmation.
            GBISegment{ 0x1198,     0x3B23DFF75831391CULL,  { &F3DLX2_XBUS_2_07_REJ } }, // Needs confirmation.
            GBISegment{ 0x13A0,     0xA7B96A2FC8C94E60ULL,  { &F3DEX2_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1198,     0xD5E0AABE069BA75CULL,  { &F3DLX2_XBUS_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x1370,     0xCFCF197526613F82ULL,  { &F3DEX2_NON_FIFO_2_03 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0xDF624201BC21895EULL,  { &F3DEX2_NON_FIFO_2_04 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x0856C0CA45B9ABC4ULL,  { &F3DEX2_NON_FIFO_2_05 } },
            GBISegment{ 0x1390,     0x07287E3B3682EFBBULL,  { &F3DEX2_NON_FIFO_ACCLAIM } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x150739719E00ADD0ULL,  { &F3DEX2_NON_FIFO_2_06 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0xA4686FA3C97AD3CCULL,  { &F3DEX2_NON_FIFO_2_07 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x4C12DAE0534D7135ULL,  { &F3DEX2_NON_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x35D374BE816BC7DAULL,  { &F3DEX2_NON_FIFO_2_08H } }, // Needs confirmation.
            GBISegment{ 0x13A0,     0xBA58701614D1A902ULL,  { &F3DEX2_NON_XBUS_2_06 } }, // Needs confirmation.
            GBISegment{ 0x13A0,     0x9BF34794D785E01BULL,  { &F3DEX2_NON_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x1A24186AD41D2568ULL,  { &F3DZEX2_NON_FIFO_2_06H } },
            GBISegment{ 0x1630,     0xF5EE0949F308CFE3ULL,  { &F3DZEX2_NON_FIFO_2_08I } },
            GBISegment{ 0x1630,     0x7502444D3DDBD4BFULL,  { &F3DZEX2_NON_FIFO_2_08J } },
            GBISegment{ 0xEA0,      0x76CC0F7EDCB9D6ADULL,  { &S2D } }, // Needs confirmation.
            GBISegment{ 0xEA0,      0x129C21833F31999DULL,  { &S2D_A } }, // Needs confirmation.
            GBISegment{ 0xED0,      0xE4910220BE1D53D7ULL,  { &S2D_FIFO } }, // Needs confirmation.
            GBISegment{ 0xED0,      0x56E22D6B0FE5BDEEULL,  { &S2D_FIFO_A } }, // Needs confirmation.
            GBISegment{ 0x17E0,     0x5034E426AA0213E3ULL,  { &S2DEX_1_03 } }, // Needs confirmation.
            GBISegment{ 0x17E0,     0x24E4DE8F087EEF34ULL,  { &S2DEX_1_05 } }, // Needs confirmation.
            GBISegment{ 0x17E0,     0xE3AABE6D3AA1DC84ULL,  { &S2DEX_1_06 } }, // Needs confirmation.
            GBISegment{ 0x17E0,     0x874A5915C0C4C8A8ULL,  { &S2DEX_1_07 } },
            GBISegment{ 0x18C0,     0xB524D27BED87DE3AULL,  { &S2DEX2_FIFO_2_04 } },
            GBISegment{ 0x18C0,     0x7F6DEA6A33FF67BDULL,  { &S2DEX2_FIFO_2_05, &S2DEX2_FIFO_2_06, &S2DEX2_FIFO_2_07 } },
            GBISegment{ 0x18C0,     0x252C09A4BBB2F9D3ULL,  { &S2DEX2_FIFO_2_05_SAFE } },
            GBISegment{ 0x18C0,     0x9300F34F3B438634ULL,  { &S2DEX2_FIFO_2_08 } },
            GBISegment{ 0x18D0,     0x1B84EB8DEF3DC2E7ULL,  { &S2DEX2_XBUS_2_06 } }, // Needs confirmation.
            GBISegment{ 0x18D0,     0xDD4DDB087A3CAC5EULL,  { &S2DEX2_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0xFA8,      0xCD640EC520D9F1E0ULL,  { &L3D_BC } }, // Needs confirmation.
            GBISegment{ 0xF68,      0x75A9F04BCF222D6AULL,  { &L3DEX_1_00 } }, // Needs confirmation.
            GBISegment{ 0xFF0,      0xCFAEE316F8AF900CULL,  { &L3DEX_1_21, &L3DEX_1_23_A } }, // Needs confirmation.
            GBISegment{ 0xFF0,      0xC8D5E51F62D4F740ULL,  { &L3DEX_1_23 } }, // Needs confirmation.
            GBISegment{ 0x1190,     0xA2F9906594260FD4ULL,  { &L3DEX2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x1190,     0x019210E4F59B27C1ULL,  { &L3DEX2_FIFO_ACCLAIM } }, // Needs confirmation.
            GBISegment{ 0x1190,     0x60B6BA671EE6F1F6ULL,  { &L3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x10B0,     0xE8028E4BC6529E6EULL,  { &ZSORTP_0_33 } }, // Needs confirmation.
    };

    static std::array<GBISegment, 105> dataSegments = {
            GBISegment{ 0x800,      0xEEB10D73400213B3ULL,  { &F3D_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x800,      0x49651E384B48F694ULL,  { &F3D_SDK_F } }, // Needs confirmation.
            GBISegment{ 0x800,      0x1A736198F90E81C5ULL,  { &F3D_SDK_UNKNOWN_G } }, // Needs confirmation.
            GBISegment{ 0x800,      0x1798191D01F4D743ULL,  { &F3D_NON_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x800,      0x40C193AB87AD57AFULL,  { &F3D_NON_SDK_UNKNOWN_D } }, // Needs confirmation.
            GBISegment{ 0x800,      0xECD447C4539036A6ULL,  { &F3D_NON_SDK_UNKNOWN_G } }, // Needs confirmation.
            GBISegment{ 0x800,      0xE6332A7A1278B05DULL,  { &F3D_NON_FIFO_SDK_E } }, // Needs confirmation.
            GBISegment{ 0x800,      0x6D784F182608475DULL,  { &F3D_SDK_UNKNOWN_H } }, // Needs confirmation.
            GBISegment{ 0x800,      0xD17ED94BB63DC244ULL,  { &F3D_BC } }, // Needs confirmation.
            GBISegment{ 0x800,      0x89E9CDC032C3F810ULL,  { &F3D_FB } }, // Needs confirmation.
            GBISegment{ 0x800,      0x276AC049785A7E70ULL,  { &F3D_SM64, &F3D_FIFO_SDK_E } },
            GBISegment{ 0x800,      0x880FCE853CE3E422ULL,  { &F3D_SM64_FINAL } }, // Needs confirmation.
            GBISegment{ 0x800,      0xF6D6112068370B46ULL,  { &F3D_FIFO_SDK_F } }, // Needs confirmation.
            GBISegment{ 0x800,      0x53AF11FA4205F4E7ULL,  { &F3D_WAYNE } }, // Needs confirmation.
            GBISegment{ 0x800,      0x72AD2373CEC74AA7ULL,  { &F3D_PD } },
            GBISegment{ 0x800,      0xB2152361A81ED3B0ULL,  { &F3D_GOLDEN } }, // Needs confirmation.
            GBISegment{ 0x800,      0x3E7A693EA9A18E45ULL,  { &F3D_WAVE } }, // Needs confirmation.
            GBISegment{ 0x800,      0x4BB61D72241EFD23ULL,  { &F3DEX_0_95 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x58641378A6D4FF0CULL,  { &F3DLX_0_95 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x1D53DE103B933358ULL,  { &F3DEX_0_96 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x478F5508A96A070BULL,  { &F3DLP_0_96_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0xCE617F2A645340F4ULL,  { &F3DEX_1_00 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x4B5FDED20C137EC1ULL,  { &F3DEX_1_21, &F3DEX_1_21_A } },
            GBISegment{ 0x800,      0xCCF255DEB0FAA63DULL,  { &F3DLX_1_21 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x484C6940F5072C39ULL,  { &F3DLX_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0x914E659B39FAE202ULL,  { &F3DLP_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0x360E69EA3E9EB6B7ULL,  { &F3DLX_1_22 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x312E7A6E9F144186ULL,  { &F3DEX_NON_1_22_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0x3828B4F75B0A0E6AULL,  { &F3DEX_1_23 } },
            GBISegment{ 0x800,      0x4A9D9C1BBFFEA48DULL,  { &F3DEX_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0xEDDB46181FE78DE8ULL,  { &F3DLX_1_23 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x2773FDFC5E62C0B0ULL,  { &F3DLX_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0x6BD0FEA8C97D3442ULL,  { &F3DLX_1_23_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0x2A042AE973C2E3ABULL,  { &F3DLX_1_23_REJ_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0x50D70B247E16A681ULL,  { &F3DLP_1_23_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0x6BF03BD84C267741ULL,  { &F3DTEXA_1_23 } }, // Needs confirmation.
            GBISegment{ 0x800,      0xC3131EA8EB614FE3ULL,  { &F3DEX_NON_0_96 } }, // Needs confirmation.
            GBISegment{ 0x800,      0xD19BE2945518BDA8ULL,  { &F3DEX_NON_1_00 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x43B301361EBFD848ULL,  { &F3DLX_NON_1_00 } }, // Needs confirmation.
            GBISegment{ 0x800,      0xEACD63EAECAF00B5ULL,  { &F3DEX_NON_1_21, &F3DEX_NON_1_21_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0xA5E20037AB033BE2ULL,  { &F3DLX_NON_1_21 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x2A0468F401EEBDFAULL,  { &F3DEX_NON_1_22 } },
            GBISegment{ 0x800,      0xEC283C51058DB925ULL,  { &F3DEX_NON_1_23 } }, // Needs confirmation.
            GBISegment{ 0x800,      0xCCB420ED1CC65F3FULL,  { &F3DEX_NON_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x800,      0x9C7395BAB35E1E49ULL,  { &F3DLX_NON_1_23, } }, // Needs confirmation.
            GBISegment{ 0x800,      0x73678EC81B2D0D1FULL,  { &F3DLX_NON_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x420,      0x3BF767E62B44ABC6ULL,  { &F3DEX2_FIFO_2_03 } }, // Needs confirmation.
            GBISegment{ 0x410,      0xE967951D7760E73CULL,  { &F3DLX2_FIFO_2_03_REJ } }, // Needs confirmation.
            GBISegment{ 0x3B0,      0x33FF4026CF6458F5ULL,  { &F3DFLX2_FIFO_2_03F_REJ } }, // Needs confirmation.
            GBISegment{ 0x420,      0xB2C65790C7A8D338ULL,  { &F3DEX2_FIFO_2_04 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x4484B6D3398C3B6CULL,  { &F3DEX2_FIFO_2_04H } }, // Needs confirmation.
            GBISegment{ 0x420,      0xF71B0A57D680F2B3ULL,  { &F3DEX2_FIFO_2_05 } },
            GBISegment{ 0x410,      0x9AB5B5144A3BD300ULL,  { &F3DEX2_FIFO_2_05_REJ } }, // Needs confirmation.
            GBISegment{ 0x410,      0x7BF113F398B8301CULL,  { &F3DLX2_FIFO_2_05_REJ } }, // Needs confirmation.
            GBISegment{ 0x430,      0xD7D5F81345FC5310ULL,  { &F3DAM2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xD4619CEDD1213E15ULL,  { &F3DEX2_FIFO_2_06 } },
            GBISegment{ 0x410,      0xD3135F48A08C1982ULL,  { &F3DEX2_FIFO_2_06_REJ } }, // Needs confirmation.
            GBISegment{ 0x410,      0x47BA278DD7DB79EBULL,  { &F3DLX2_FIFO_2_06_REJ } }, // Needs confirmation.
            GBISegment{ 0x420,      0xF8649121FAB40A06ULL,  { &F3DEX2_FIFO_2_07 } },
            GBISegment{ 0x420,      0x3D4CAB9C82AD3772ULL,  { &F3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x410,      0x14F9D11C0F58AFB5ULL,  { &F3DEX2_FIFO_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x410,      0x25A00931C3C2A573ULL,  { &F3DLX2_FIFO_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x420,      0xD381B0139965EE5CULL,  { &F3DEX2_FIFO_2_08_CRUISN } }, // Needs confirmation.
            GBISegment{ 0x420,      0xB411ADC06FAA9D83ULL,  { &F3DEX2_FIFO_2_08_PL } },
            GBISegment{ 0x420,      0x58BA836DE5C3B968ULL,  { &F3DEX2_XBUS_2_05 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x1D0B2B02F7C09D84ULL,  { &F3DEX2_XBUS_2_07 } }, // Needs confirmation.
            GBISegment{ 0x410,      0xCE0DAA8259433956ULL,  { &F3DLX2_XBUS_2_07_REJ } }, // Needs confirmation.
            GBISegment{ 0x420,      0x5FB335C8CDF86F48ULL,  { &F3DEX2_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0x410,      0x2869DC3116DEA560ULL,  { &F3DLX2_XBUS_2_08_REJ } }, // Needs confirmation.
            GBISegment{ 0x420,      0x38FF0FE9D7CFBD34ULL,  { &F3DEX2_NON_FIFO_2_03 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x2E3B70A49807BF89ULL,  { &F3DEX2_NON_FIFO_2_04 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xE57A61CA7770A4EAULL,  { &F3DEX2_NON_FIFO_2_05 } },
            GBISegment{ 0x420,      0xD62BDC3A200CBB35ULL,  { &F3DEX2_NON_FIFO_ACCLAIM } }, // Needs confirmation.
            GBISegment{ 0x420,      0xD415349186DC365CULL,  { &F3DEX2_NON_FIFO_2_06 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x806BEAF49F944E89ULL,  { &F3DEX2_NON_FIFO_2_07 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x3BE3FAD9073FEB78ULL,  { &F3DEX2_NON_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xE762274AB4B747CDULL,  { &F3DEX2_NON_FIFO_2_08H } }, // Needs confirmation.
            GBISegment{ 0x420,      0x3E29DC0570E8B835ULL,  { &F3DEX2_NON_XBUS_2_06 } }, // Needs confirmation.
            GBISegment{ 0x420,      0x8309F513B20C9040ULL,  { &F3DEX2_NON_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xE3E5C20BC750105EULL,  { &F3DZEX2_NON_FIFO_2_06H } },
            GBISegment{ 0x420,      0x002D7FA254ABD8E7ULL,  { &F3DZEX2_NON_FIFO_2_08I } },
            GBISegment{ 0x420,      0x6069A2803CB39E66ULL,  { &F3DZEX2_NON_FIFO_2_08J } },
            GBISegment{ 0xFC0,      0xDC8A6B7535C3EC67ULL,  { &S2D, &S2D_A } }, // Needs confirmation.
            GBISegment{ 0xFC0,      0x6255273A8C5887F3ULL,  { &S2D_FIFO, &S2D_FIFO_A } }, // Needs confirmation.
            GBISegment{ 0x3C0,      0x59EE4D449CA1F9AFULL,  { &S2DEX_1_03 } }, // Needs confirmation.
            GBISegment{ 0x3C0,      0x14A890EC7B06F2E9ULL,  { &S2DEX_1_05 } }, // Needs confirmation.
            GBISegment{ 0x3C0,      0x7A9811CF7E7FCCCFULL,  { &S2DEX_1_06 } }, // Needs confirmation.
            GBISegment{ 0x3C0,      0x2018F33CBC3E2818ULL,  { &S2DEX_1_07 } },
            GBISegment{ 0x390,      0xB3108E4928CB6B1DULL,  { &S2DEX2_FIFO_2_04 } },
            GBISegment{ 0x390,      0x47829093527F366BULL,  { &S2DEX2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x390,      0x01DE3936615B8C9CULL,  { &S2DEX2_FIFO_2_05_SAFE } },
            GBISegment{ 0x390,      0x507670688A6E0671ULL,  { &S2DEX2_FIFO_2_06 } }, // Needs confirmation.
            GBISegment{ 0x390,      0xB0FA15F7CFBB1978ULL,  { &S2DEX2_FIFO_2_07 } }, // Needs confirmation.
            GBISegment{ 0x390,      0x50EF0DFBD3A8CD0FULL,  { &S2DEX2_FIFO_2_08 } },
            GBISegment{ 0x390,      0x123C6688307483B6ULL,  { &S2DEX2_XBUS_2_06 } }, // Needs confirmation.
            GBISegment{ 0x390,      0x4143792286C20CFFULL,  { &S2DEX2_XBUS_2_08 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x4B2D1A8C46895B0DULL,  { &L3D_BC } }, // Needs confirmation.
            GBISegment{ 0x800,      0xD15737A2A52F0B12ULL,  { &L3DEX_1_00 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x5C189B2C6243EB6DULL,  { &L3DEX_1_21 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x880EF9B9011CB11EULL,  { &L3DEX_1_23 } }, // Needs confirmation.
            GBISegment{ 0x800,      0x67315143B3DE852EULL,  { &L3DEX_1_23_A } }, // Needs confirmation.
            GBISegment{ 0x3F0,      0x58F659DEBA493C69ULL,  { &L3DEX2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x3F0,      0x772E4F4BD7F82DEFULL,  { &L3DEX2_FIFO_ACCLAIM } }, // Needs confirmation.
            GBISegment{ 0x3F0,      0xE261C98A63863DB4ULL,  { &L3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x400,      0x8F5D64011662220CULL,  { &ZSORTP_0_33 } }, // Needs confirmation.
    };

    static bool textAndDataSegmentsSorted = false;

    // GBIManager

    GBIManager::GBIManager() {
        GBI_EXTENDED::initialize();

        if (!textAndDataSegmentsSorted) {
            std::sort(textSegments.begin(), textSegments.end());
            std::sort(dataSegments.begin(), dataSegments.end());
            textAndDataSegmentsSorted = true;
        }
    }

    GBIManager::~GBIManager() {
        // Does nothing.
    }
    
    GBI *GBIManager::getGBIForRDP() {
        GBI &gbi = gbiCache[uint32_t(GBIUCode::RDP)];
        if (gbi.ucode == GBIUCode::Unknown) {
            gbi.ucode = GBIUCode::RDP;
            GBI_RDP::setup(&gbi, false);
        }

        return &gbi;
    }
    
    GBI *GBIManager::getGBIForUCode(uint8_t *RDRAM, uint32_t textAddress, uint32_t dataAddress) {
        assert(RDRAM != nullptr);

        int32_t textSegmentIndex = -1;
        uint8_t *rdramCursor = &RDRAM[textAddress];
        uint32_t rdramHashed = 0;
        uint64_t rdramHash = 0;
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);

        auto hashSegment = [&](uint32_t index, const GBISegment &gbiSegment, int32_t &resultIndex) {
            if (gbiSegment.hashLength > rdramHashed) {
                const uint32_t rdramToHash = gbiSegment.hashLength - rdramHashed;
                XXH3_64bits_update(&xxh3, rdramCursor, rdramToHash);
                rdramHash = XXH3_64bits_digest(&xxh3);
                rdramCursor += rdramToHash;
                rdramHashed += rdramToHash;
            }

            if (rdramHash == gbiSegment.hashValue) {
                resultIndex = int32_t(index);
            }

            // If we used an unknown value in the database, print the hash.
            if (gbiSegment.hashValue == GBI_UNKNOWN_HASH_NUMB) {
                fprintf(stdout, "HASH: 0x%016" PRIX64 "ULL SIZE: 0x%X\n", rdramHash, rdramHashed);
            }
        };

        for (uint32_t i = 0; i < textSegments.size(); i++) {
            hashSegment(i, textSegments[i], textSegmentIndex);
        }

        int32_t dataSegmentIndex = -1;
        XXH3_64bits_reset(&xxh3);
        rdramCursor = &RDRAM[dataAddress];
        rdramHashed = 0;
        rdramHash = 0;
        for (uint32_t i = 0; i < dataSegments.size(); i++) {
            hashSegment(i, dataSegments[i], dataSegmentIndex);
        }

        if (textSegmentIndex < 0 || dataSegmentIndex < 0) {
            fprintf(stderr, "Unable to find a matching GBI in the current database. This game is not supported in HLE.\n");
            deduceGBIInformation(RDRAM, textAddress, dataAddress);
            return nullptr;
        }

        // Search for the first intersection available between both segments.
        const GBISegment &textSegment = textSegments[textSegmentIndex];
        const GBISegment &dataSegment = dataSegments[dataSegmentIndex];
        const GBIInstance *matchingInstance = nullptr;
        for (const GBIInstance *textInstance : textSegment.instances) {
            for (const GBIInstance *dataInstance : dataSegment.instances) {
                if (textInstance == dataInstance) {
                    matchingInstance = dataInstance;
                    break;
                }
            }
        }

        if (matchingInstance == nullptr) {
            fprintf(stderr, "Unable to find a GBI that is shared between the text and data segment. Is the GBI database configured incorrectly?\n");
            return nullptr;
        }

        GBI &gbi = gbiCache[uint32_t(matchingInstance->ucode)];
        if (gbi.ucode == GBIUCode::Unknown) {
            gbi.ucode = matchingInstance->ucode;

            // Common RDP functions.
            GBI_RDP::setup(&gbi, true);

            switch (gbi.ucode) {
            case GBIUCode::RDP:
                break;
            case GBIUCode::F3D:
                GBI_F3D::setup(&gbi);
                break;
            case GBIUCode::F3DGOLDEN:
                GBI_F3DGOLDEN::setup(&gbi);
                break;
            case GBIUCode::F3DPD:
                GBI_F3DPD::setup(&gbi);
                break;
            case GBIUCode::F3DWAVE:
                GBI_F3DWAVE::setup(&gbi);
                break;
            case GBIUCode::F3DEX:
                GBI_F3DEX::setup(&gbi);
                break;
            case GBIUCode::F3DEX2:
                GBI_F3DEX2::setup(&gbi);
                break;
            case GBIUCode::F3DZEX2:
                GBI_F3DZEX2::setup(&gbi);
                break;
            case GBIUCode::S2DEX:
                GBI_S2DEX::setup(&gbi);
                break;
            case GBIUCode::S2DEX2:
                GBI_S2DEX2::setup(&gbi);
                break;
            case GBIUCode::L3DEX2:
                GBI_L3DEX2::setup(&gbi);
                break;
            default:
                assert(false && "Unknown UCode.");
                break;
            }
        }

        // Update the cached GBI to use the instance's flags.
        gbi.flags = matchingInstance->flags;

        return &gbi;
    }

    void GBIManager::deduceGBIInformation(uint8_t *RDRAM, uint32_t textAddress, uint32_t dataAddress) {
        const uint8_t rspPattern[] = "RSP";
        uint8_t dataSegmentBytes[0x800];
        uint8_t *dataSegmentBytesEnd = dataSegmentBytes + std::size(dataSegmentBytes);
        for (size_t i = 0; i < std::size(dataSegmentBytes); i++) {
            dataSegmentBytes[i] = RDRAM[(dataAddress + i) ^ 0x3];
        }

        const uint8_t *searchResult = std::search(dataSegmentBytes, dataSegmentBytesEnd, rspPattern, rspPattern + std::size(rspPattern) - 1);
        if (searchResult != dataSegmentBytesEnd) {
            uint32_t validChars = 0;
            while (searchResult[validChars] > 0x0A) {
                validChars++;
            }

            if (validChars > 0) {
                const uint8_t *searchResultEnd = searchResult + validChars;
                fprintf(stderr, "Detected name in data: %.*s\n", validChars, searchResult);

                const uint8_t f3dexPattern[] = "F3DEX";
                const uint8_t s2dexPattern[] = "S2DEX";
                const uint8_t twoPointPattern[] = "2.";
                const uint8_t *f3dexResult = std::search(searchResult, searchResultEnd, f3dexPattern, f3dexPattern + std::size(f3dexPattern) - 1);
                const uint8_t *twoPointResult = std::search(searchResult, searchResultEnd, twoPointPattern, twoPointPattern + std::size(twoPointPattern) - 1);
                if ((f3dexResult != searchResultEnd) && (twoPointResult != searchResultEnd)) {
                    const uint32_t lastOverlay = *reinterpret_cast<const uint32_t *>(&RDRAM[dataAddress + 0x410]);
                    const uint16_t lastOverlaySize = *reinterpret_cast<const uint16_t *>(&RDRAM[(dataAddress + 0x414) ^ 2]);
                    const uint16_t estimatedDataSize = *reinterpret_cast<const uint16_t *>(&RDRAM[(dataAddress + 0x380) ^ 2]);
                    const uint32_t textSize = lastOverlay + lastOverlaySize + 1;
                    fprintf(stderr, "Detected text size is 0x%X\n", textSize);
                    fprintf(stderr, "Estimated data size is 0x%X\n", estimatedDataSize);
                }

                const uint8_t *s2dexResult = std::search(searchResult, searchResultEnd, s2dexPattern, s2dexPattern + std::size(s2dexPattern) - 1);
                if ((s2dexResult != searchResultEnd) && (twoPointResult != searchResultEnd)) {
                    const uint32_t lastOverlay = *reinterpret_cast<const uint32_t *>(&RDRAM[dataAddress + 0x31C]);
                    const uint16_t lastOverlaySize = *reinterpret_cast<const uint16_t *>(&RDRAM[(dataAddress + 0x320) ^ 2]);
                    const uint32_t textSize = lastOverlay + lastOverlaySize + 1;
                    fprintf(stderr, "Detected text size is 0x%X\n", textSize);
                }
            }
        }
    }

    GBIFunction GBIManager::getExtendedFunction() const {
        return &GBI_EXTENDED::extendedOp;
    }
};