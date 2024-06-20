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
    const GBIInstance   F3D_SM64                  = { "SW Version: 2.0D, 04-01-96 (SM64)",         GBIUCode::F3D,         { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX_1_21                = { "F3DEX 1.21",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   F3DLX_1_21_REJ            = { "F3DLX 1.21.Rej",                            GBIUCode::F3DEX,       { true,   false,  true,   false,  false } };
    const GBIInstance   F3DEX_1_23                = { "F3DEX 1.23",                                GBIUCode::F3DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX_NON_1_22            = { "F3DEX.NoN 1.22",                            GBIUCode::F3DEX,       { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_04H         = { "F3DEX2.fifo 2.04H",                         GBIUCode::F3DEX2,      { false,  false,  false,  true,   false } };
    const GBIInstance   F3DEX2_FIFO_2_05          = { "F3DEX2.fifo 2.05",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_07          = { "F3DEX2.fifo 2.07",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_08          = { "F3DEX2.fifo 2.08",                          GBIUCode::F3DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   F3DEX2_FIFO_2_08_PL       = { "F3DEX2.fifo 2.08 (POSLIGHT)",               GBIUCode::F3DEX2,      { false,  false,  false,  false,  true  } };
    const GBIInstance   F3DEX2_NON_FIFO_2_05      = { "F3DEX2.NoN.fifo 2.05",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX2_NON_FIFO_2_08      = { "F3DEX2.NoN.fifo 2.08",                      GBIUCode::F3DEX2,      { false,  true,   false,  false,  false } };
    const GBIInstance   F3DEX2_NON_FIFO_2_08H     = { "F3DEX2.NoN.fifo 2.08H",                     GBIUCode::F3DEX2,      { false,  true,   false,  true,   false } };
    const GBIInstance   F3DZEX2_NON_FIFO_2_06H    = { "F3DZEX2.NoN.fifo 2.06H",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  false } };
    const GBIInstance   F3DZEX2_NON_FIFO_2_08I    = { "F3DZEX2.NoN.fifo 2.08I",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  true  } };
    const GBIInstance   F3DZEX2_NON_FIFO_2_08J    = { "F3DZEX2.NoN.fifo 2.08J",                    GBIUCode::F3DZEX2,     { false,  true,   false,  false,  true  } };
    const GBIInstance   S2DEX_1_07                = { "S2DEX 1.07",                                GBIUCode::S2DEX,       { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_04          = { "S2DEX.fifo 2.04",                           GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_05          = { "S2DEX.fifo 2.05",                           GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_05_SAFE     = { "S2DEX.fifo 2.05 [Safe]",                    GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    const GBIInstance   S2DEX2_FIFO_2_08          = { "S2DEX2.fifo 2.08",                          GBIUCode::S2DEX2,      { false,  false,  false,  false,  false } };
    
    // ****************************************************************************************
    // * Database of known UCode text and data segments.                                      *
    // ****************************************************************************************
    // 
    //                  Length      Hash                    Known instances               
    //     
    static std::array<GBISegment, 21> textSegments = {
            GBISegment{ 0x1408,     0xF50165C013FCB8A2ULL,  { &F3D_SM64 } },
            GBISegment{ 0x1430,     0x9A7772037D709388ULL,  { &F3DEX_1_21 } },
            GBISegment{ 0x13D0,     0x1BEA638E869B0195ULL,  { &F3DLX_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x1430,     0xAC03DE5B7B1E710FULL,  { &F3DEX_1_23 } },
            GBISegment{ 0x1430,     0x454B7C0482C64F7FULL,  { &F3DEX_NON_1_22 } },
            GBISegment{ 0x1390,     0x15C2462591E78D2BULL,  { &F3DEX2_FIFO_2_04H } }, // Needs confirmation.
            GBISegment{ 0x1390,     0xBA192DFA28437D3DULL,  { &F3DEX2_FIFO_2_05 } },
            GBISegment{ 0x1390,     0x8C1C9814E75E1B4BULL,  { &F3DEX2_FIFO_2_07 } },
            GBISegment{ 0x1390,     0xCF55FAE288BFE48DULL,  { &F3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1630,     0x4588323F6F7E7720ULL,  { &F3DEX2_FIFO_2_08_PL } },
            GBISegment{ 0x1390,     0x0856C0CA45B9ABC4ULL,  { &F3DEX2_NON_FIFO_2_05 } },
            GBISegment{ 0x1390,     0x4C12DAE0534D7135ULL,  { &F3DEX2_NON_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x35D374BE816BC7DAULL,  { &F3DEX2_NON_FIFO_2_08H } }, // Needs confirmation.
            GBISegment{ 0x1390,     0x1A24186AD41D2568ULL,  { &F3DZEX2_NON_FIFO_2_06H } },
            GBISegment{ 0x1630,     0xF5EE0949F308CFE3ULL,  { &F3DZEX2_NON_FIFO_2_08I } },
            GBISegment{ 0x1630,     0x7502444D3DDBD4BFULL,  { &F3DZEX2_NON_FIFO_2_08J } },
            GBISegment{ 0x17E0,     0x874A5915C0C4C8A8ULL,  { &S2DEX_1_07 } },
            GBISegment{ 0x18C0,     0xB524D27BED87DE3AULL,  { &S2DEX2_FIFO_2_04 } },
            GBISegment{ 0x18C0,     0x7F6DEA6A33FF67BDULL,  { &S2DEX2_FIFO_2_05 } },
            GBISegment{ 0x18C0,     0x252C09A4BBB2F9D3ULL,  { &S2DEX2_FIFO_2_05_SAFE } },
            GBISegment{ 0x18C0,     0x9300F34F3B438634ULL,  { &S2DEX2_FIFO_2_08 } },
    };

    static std::array<GBISegment, 21> dataSegments = {
            GBISegment{ 0x800,      0x276AC049785A7E70ULL,  { &F3D_SM64 } },
            GBISegment{ 0x800,      0x4B5FDED20C137EC1ULL,  { &F3DEX_1_21 } },
            GBISegment{ 0x800,      0x3828B4F75B0A0E6AULL,  { &F3DEX_1_23 } },
            GBISegment{ 0x800,      0x484C6940F5072C39ULL,  { &F3DLX_1_21_REJ } }, // Needs confirmation.
            GBISegment{ 0x800,      0x2A0468F401EEBDFAULL,  { &F3DEX_NON_1_22 } },
            GBISegment{ 0x420,      0x4484B6D3398C3B6CULL,  { &F3DEX2_FIFO_2_04H } }, // Needs confirmation.
            GBISegment{ 0x420,      0xF71B0A57D680F2B3ULL,  { &F3DEX2_FIFO_2_05 } },
            GBISegment{ 0x420,      0xF8649121FAB40A06ULL,  { &F3DEX2_FIFO_2_07 } },
            GBISegment{ 0x420,      0x3D4CAB9C82AD3772ULL,  { &F3DEX2_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xB411ADC06FAA9D83ULL,  { &F3DEX2_FIFO_2_08_PL } },
            GBISegment{ 0x420,      0xE57A61CA7770A4EAULL,  { &F3DEX2_NON_FIFO_2_05 } },
            GBISegment{ 0x420,      0x3BE3FAD9073FEB78ULL,  { &F3DEX2_NON_FIFO_2_08 } }, // Needs confirmation.
            GBISegment{ 0x420,      0xE762274AB4B747CDULL,  { &F3DEX2_NON_FIFO_2_08H } }, // Needs confirmation.
            GBISegment{ 0x420,      0xE3E5C20BC750105EULL,  { &F3DZEX2_NON_FIFO_2_06H } },
            GBISegment{ 0x420,      0x002D7FA254ABD8E7ULL,  { &F3DZEX2_NON_FIFO_2_08I } },
            GBISegment{ 0x420,      0x6069A2803CB39E66ULL,  { &F3DZEX2_NON_FIFO_2_08J } },
            GBISegment{ 0x3C0,      0x2018F33CBC3E2818ULL,  { &S2DEX_1_07 } },
            GBISegment{ 0x390,      0xB3108E4928CB6B1DULL,  { &S2DEX2_FIFO_2_04 } },
            GBISegment{ 0x390,      0x47829093527F366BULL,  { &S2DEX2_FIFO_2_05 } }, // Needs confirmation.
            GBISegment{ 0x390,      0x01DE3936615B8C9CULL,  { &S2DEX2_FIFO_2_05_SAFE } },
            GBISegment{ 0x390,      0x50EF0DFBD3A8CD0FULL,  { &S2DEX2_FIFO_2_08 } },
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