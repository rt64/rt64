//
// RT64
//

#include "rt64_gbi_s2dex.h"

#include "../include/rt64_extended_gbi.h"

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dex.h"
#include "rt64_gbi_rdp.h"

//#define LOG_BGRECT_METHODS
//#define LOG_LOADTXTR_METHODS

namespace RT64 {
    namespace GBI_S2DEX {
        void objRenderMode(State *state, DisplayList **dl) {
            state->rsp->setObjRenderMode((*dl)->w1);
        }

        void moveWord(State *state, DisplayList **dl) {
            switch ((*dl)->p0(0, 8)) {
            case G_MW_GENSTAT:
                assert(false);
                break;
            default:
                GBI_F3D::moveWord(state, dl);
                break;
            }
        }

        void rdpHalf0(State *state, DisplayList **dl) {
            uint8_t nextCode = (*dl + 1)->w0 >> 24;
            if (nextCode == S2DEX_G_SELECT_DL) {
                assert(false);
            }
            else if (nextCode == F3D_G_RDPHALF_1) {
                GBI_RDP::texrect(state, dl);
            }
        }

        void bg1CycTMEMLoadTile(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imagePtr, uint16_t imageSrcWsize, int16_t loadLines, int16_t tmemSH) {
            // TODO: Does it make sense that when using 32-bit images, the lrt is 0?
            const bool is32Bits = (scaleBg.imageSiz == G_IM_SIZ_32b);
            uint8_t textureImageSiz = is32Bits ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            uint16_t tileLrt = is32Bits ? 0 : ((loadLines << 2) - 1);
            rdp.setTextureImage(G_IM_FMT_RGBA, textureImageSiz, imageSrcWsize >> 1, imagePtr);
            rdp.loadTile(G_TX_LOADTILE, 0, 0, (tmemSH - 1) << 4, tileLrt);
        }
        
        void bg1CycTMEMSetAndLoadTile(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imagePtr, uint16_t imageSrcWsize, int16_t loadLines, uint16_t tmemSliceWmax, int16_t tmemAdrs, int16_t tmemSH) {
            const bool is32Bits = (scaleBg.imageSiz == G_IM_SIZ_32b);
            uint8_t setSiz = is32Bits ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            rdp.setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, setSiz, tmemSliceWmax, tmemAdrs, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            bg1CycTMEMLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, loadLines, tmemSH);
        }

        void bg1CycTMEMLoad(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imageTop, uint32_t &imagePtr, int16_t &imageRemain, uint16_t imageSrcWsize, uint16_t imagePtrX0, 
            int16_t tmemSrcLines, uint16_t tmemSliceWmax, int16_t drawLines, int16_t usesBilerp, int16_t flagSplit)
        {
            int16_t loadLines = drawLines + usesBilerp;
            int16_t iLoadable = imageRemain - flagSplit;

            // Load everything at once.
            if (iLoadable >= loadLines) {
                bg1CycTMEMLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, loadLines, tmemSliceWmax);
                imagePtr += imageSrcWsize * drawLines;
                imageRemain -= drawLines;
            }
            // Partitioned load.
            else {
                uint32_t imageTopSeg = imageTop & 0xFF000000;
                int16_t subSliceY2 = imageRemain;
                int16_t subSliceL2 = loadLines - subSliceY2;
                int16_t subSliceD2 = drawLines - subSliceY2;
                if (subSliceL2 > 0) {
                    uint32_t imagePtr2 = imageTop + imagePtrX0;
                    if (subSliceY2 & 0x1) {
                        imagePtr2 -= imageSrcWsize;
                        imagePtr2 = imageTopSeg | (imagePtr2 & 0x00FFFFFF);
                        subSliceY2--;
                        subSliceL2++;
                    }

                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr2, imageSrcWsize, subSliceL2, tmemSliceWmax, subSliceY2 * tmemSliceWmax, tmemSliceWmax);
                }

                if (flagSplit) {
                    uint32_t imagePtr1A = imagePtr + iLoadable * imageSrcWsize;
                    uint32_t imagePtr1B = imageTop;
                    int16_t subSliceY1 = iLoadable;
                    int16_t subSliceL1 = iLoadable & 1;
                    if (subSliceL1) {
                        imagePtr1A -= imageSrcWsize;
                        imagePtr1B -= imageSrcWsize;
                        imagePtr1B = imageTopSeg | (imagePtr1B & 0x00FFFFFF);
                        subSliceY1--;
                    }

                    subSliceL1++;

                    int16_t tmemSH_A = (imageSrcWsize - imagePtrX0) >> 3;
                    int16_t tmemSH_B = tmemSliceWmax - tmemSH_A;
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr1B, imageSrcWsize, subSliceL1, tmemSliceWmax, subSliceY1 * tmemSliceWmax + tmemSH_A, tmemSH_B);
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr1A, imageSrcWsize, subSliceL1, tmemSliceWmax, subSliceY1 * tmemSliceWmax, tmemSH_A);
                }

                if (iLoadable > 0) {
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, iLoadable, tmemSliceWmax, 0, tmemSliceWmax);
                }
                else {
                    uint8_t loadSiz = (scaleBg.imageSiz == G_IM_SIZ_32b) ? G_IM_SIZ_32b : G_IM_SIZ_16b;
                    rdp.setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, loadSiz, tmemSliceWmax, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                }

                imageRemain -= drawLines;
                if (imageRemain > 0) {
                    imagePtr += imageSrcWsize * drawLines;
                }
                else {
                    imageRemain = tmemSrcLines - subSliceD2;
                    imagePtr = imageTop + subSliceD2 * imageSrcWsize + imagePtrX0;
                }
            }
        }

        void bg1Cyc(State *state, DisplayList **dl) {
#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bg1Cyc::start(0x%08X)", (*dl)->w1);
#       endif

            RDP *rdp = state->rdp.get();
            RSP *rsp = state->rsp.get();
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            // TODO load this into the S2D struct buffer for a more accurate implementation in case there's ever command state bleed.
            const uObjBg *bgObject = reinterpret_cast<const uObjBg *>(state->fromRDRAM(rdramAddress));
            const uObjBg_t &bg = bgObject->bg;
            const uObjScaleBg_t &scaleBg = bgObject->scaleBg;

            // Validate the image loading method is one of the two supported methods. It's worth noting 
            // that despite the load block method being documented, it doesn't actually work in the retail
            // microcode and it should therefore be treated as load tile as well.
            assert((bg.imageLoad == S2DEX_G_BGLT_LOADBLOCK) || (bg.imageLoad == S2DEX_G_BGLT_LOADTILE));

            // The scale should at least be 1.
            uint16_t scaleW = std::max(scaleBg.scaleW, uint16_t(1));
            uint16_t scaleH = std::max(scaleBg.scaleH, uint16_t(1));

            // Max frame area determined from the image size and the scale.
            int32_t frameWmax = (int32_t(scaleBg.imageW) << 10) / scaleW;
            int32_t frameHmax = (int32_t(scaleBg.imageH) << 10) / scaleH;
            frameWmax = (frameWmax - 1) & ~0x3;
            frameHmax = (frameHmax - 1) & ~0x3;

            // Clamp the frame's width to the max frame area.
            int16_t frameW = scaleBg.frameW;
            int16_t frameH = scaleBg.frameH;
            frameWmax = std::max(scaleBg.frameW - frameWmax, 0);
            frameHmax = std::max(scaleBg.frameH - frameHmax, 0);
            frameW -= frameWmax;
            frameH -= frameHmax;
            
            int16_t frameX0 = scaleBg.frameX;
            int16_t frameY0 = scaleBg.frameY;
            const bool imageFlipS = scaleBg.imageFlip & S2DEX_G_BG_FLAG_FLIPS;
            if (imageFlipS) {
                frameX0 += frameWmax;
            }

            // TODO: This scissor state probably needs to be pulled from the one tracked
            // by the RSP instead of the RDP, which is not currently implemented yet.
            const FixedRect &scissorRect = rdp->scissorRectStack[rdp->scissorStackSize - 1];
            int32_t scissorX0 = scissorRect.ulx;
            int32_t scissorY0 = scissorRect.uly;
            int32_t scissorX1 = scissorRect.lrx;
            int32_t scissorY1 = scissorRect.lry;
            int16_t pixX0 = int16_t(std::max(scissorX0 - frameX0, 0));
            int16_t pixY0 = int16_t(std::max(scissorY0 - frameY0, 0));
            int16_t pixX1 = int16_t(std::max(frameW - scissorX1 + frameX0, 0));
            int16_t pixY1 = int16_t(std::max(frameH - scissorY1 + frameY0, 0));

            // Cut out the part outside of the current scissor.
            frameW = frameW - (pixX0 + pixX1);
            frameH = frameH - (pixY0 + pixY1);
            frameX0 = frameX0 + pixX0;
            frameY0 = frameY0 + pixY0;

            // The frame is no longer valid, don't draw anything.
            if ((frameW <= 0) || (frameH <= 0)) {
                return;
            }

            // Compute the frame size after being clamped by the scissor.
            int16_t frameX1 = frameX0 + frameW;
            int16_t framePtrY0 = frameY0 >> 2;
            int16_t frameRemain = frameH >> 2;
            int16_t imageSrcW = scaleBg.imageW << 3;
            int16_t imageSrcH = scaleBg.imageH << 3;
            
            // Find the corresponding range in the image for the frame.
            // The image size needs to be extended when using bilerp.
            int16_t usesBilerp = (rsp->objRenderMode & S2DEX_G_OBJRM_BILERP) ? 1 : 0;

            // Some games will incorrectly set this flag despite not actually using bilerp during drawing. The side effect is that it'll end
            // up using a method of loading textures that makes it almost impossible for the emulation to detect tiles that can be upscaled due
            // to the various shenanigans the microcode has to do to load an image that is bigger than what it really is.
            // 
            // This enhancement falls under the category of a developer intended fix because there's a clear mismatch between the end result
            // and the loading method used to achieve it, as evidenced by the current state of the RDP. The end result is it makes the loading
            // logic much simpler and allows the emulation to upscale the tiles properly.
            //
            const bool rdpUsesBilerp = (rdp->otherMode.textFilt() == G_TF_BILERP);
            const bool bilerpFixEnabled = state->ext.enhancementConfig->s2dex.fixBilerpMismatch;
            if (usesBilerp && !rdpUsesBilerp && bilerpFixEnabled) {
                usesBilerp = 0;
            }

            int16_t imageW = (frameW * scaleW) >> 7;
            int16_t imageSliceW = imageW + (usesBilerp * 32);
            int16_t imageX0;
            if (imageFlipS) {
                imageX0 = scaleBg.imageX + ((pixX1 * scaleW) >> 7);
            }
            else {
                imageX0 = scaleBg.imageX + ((pixX0 * scaleW) >> 7);
            }

            int16_t imageY0 = scaleBg.imageY + (pixY0 * scaleH >> 7);
            int32_t imageYorig = scaleBg.imageYorig;

            // Keep scrolling down the image one row at a time if the 
            // left of the image is greater than the source's width.
            while (imageX0 >= imageSrcW) {
                imageX0 -= imageSrcW;
                imageY0 += 32;
                imageYorig += 32;
            }

            // Loop around when the top of the image is greater than
            // the source's height.
            while (imageY0 >= imageSrcH) {
                imageY0 -= imageSrcH;
                imageYorig -= imageSrcH;
            }

            // The TMEM loads will need to be split if the image range covers the entire image's width.
            int16_t flagSplit = (imageX0 + imageSliceW >= imageSrcW);

            // How many lines can be loaded into TMEM per draw.
            int16_t tmemSrcLines = imageSrcH >> 5;

            // Determine the amount of TMEM that can be used based on the format and size of the image.
            // We limit the amount of TMEM that can be used to half for CI images since the upper region
            // needs to be used for the palette.
            int16_t tmemSize = (scaleBg.imageFmt == G_IM_FMT_CI) ? 256 : 512;
            int16_t tmemShift = (0x200 >> scaleBg.imageSiz);
            int16_t tmemMask = (tmemShift - 1);
            int32_t imageSliceWmax;
            if (scaleBg.imageSiz == G_IM_SIZ_32b) {
                tmemSize = 480;
                imageSliceWmax = 0x2800;
            }
            else {
                imageSliceWmax = (scaleBg.frameW * scaleW) >> 7;
                imageSliceWmax = std::min(imageSliceWmax + (usesBilerp << 5), int32_t(imageSrcW));
            }

            // Get the amount of lines that can be loaded into TMEM at once.
            uint16_t tmemSliceWmax = (imageSliceWmax + tmemMask) / tmemShift + 1;
            int16_t tmemSliceLines = tmemSize / tmemSliceWmax;
            int16_t imageSliceLines = tmemSliceLines - usesBilerp;
            int32_t frameSliceLines = (imageSliceLines << 20) / scaleH;

            // Figure out which line to start with from the image coordinates.
            int32_t imageLYoffset = (int32_t(imageY0) - imageYorig) << 5;
            if (imageLYoffset < 0) {
                imageLYoffset -= (scaleH - 1);
            }

            int32_t frameLYoffset = (imageLYoffset / scaleH) << 10;

            // Determine which slice number corresponds to this offset.
            int32_t imageNumSlice;
            if (frameLYoffset >= 0) {
                imageNumSlice = frameLYoffset / frameSliceLines;
            }
            else {
                imageNumSlice = (frameLYoffset - frameSliceLines + 1) / frameSliceLines;
            }

            // How much of the first drawn rectangle will be hidden at the top of the frame.
            int32_t frameLSliceL0 = frameSliceLines * imageNumSlice;
            int32_t frameLYslice = frameLSliceL0 & ~0x3FF;
            int32_t frameLHidden = frameLYoffset - frameLYslice;
            int32_t imageLHidden = (frameLHidden >> 10) * scaleH;
            frameLSliceL0 = (frameLSliceL0 & 0x3FF) + frameSliceLines - frameLHidden;

            // Image parameters for the slice.
            uint16_t imageT = (imageLHidden >> 5) & 0x1F;
            imageLHidden >>= 10;
            int16_t imageISliceL0 = imageSliceLines - imageLHidden;
            int16_t imageIY0 = imageSliceLines * imageNumSlice + (imageYorig & ~0x1F) / 32 + imageLHidden;
            uint16_t imageHLowered = (scaleBg.imageH >> 2);
            if (imageIY0 < 0) {
                imageIY0 += imageHLowered;
            }

            if (imageIY0 >= imageHLowered) {
                imageIY0 -= imageHLowered;
            }

            const uint32_t imageAddress = state->rsp->fromSegmented(scaleBg.imageAddress);
            uint16_t imageSrcWsize = (imageSrcW / tmemShift) << 3;
            uint16_t imagePtrX0 = (imageX0 / tmemShift) << 3;
            uint32_t imagePtr = imageAddress + imageSrcWsize * imageIY0 + imagePtrX0;
            uint16_t imageS = imageX0 & tmemMask;
            if (imageFlipS) {
                imageS = -(imageS + imageW);
            }

            // RDP Commands constant throughout the image.
            uint8_t loadSiz = (scaleBg.imageSiz == G_IM_SIZ_32b) ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            rdp->setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, loadSiz, tmemSliceWmax, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            rdp->setTile(G_TX_RENDERTILE, scaleBg.imageFmt, scaleBg.imageSiz, tmemSliceWmax, 0, uint8_t(scaleBg.imagePal), G_TX_WRAP | G_TX_MIRROR, G_TX_WRAP | G_TX_MIRROR, 0xF, 0xF, 0, 0);

            // Attempt to use a single tile instead to draw the entire rect at once if there's a framebuffer copy available.
            // If it fails to find a possible tile copy, fall back to the regular approach.
            // FIXME: The rest of the code should still run to account for state bleeding without drawing any rectangles.
            if (state->ext.enhancementConfig->s2dex.framebufferFastPath) {
                int16_t uls = imageS;
                int16_t ult = imageT;
                int16_t lrs = uls + scaleBg.imageW;
                int16_t lrt = ult + scaleBg.imageH;
                rdp->setTextureImage(G_IM_FMT_RGBA, loadSiz, imageSrcWsize >> 1, imagePtr);
                rdp->setTileSize(G_TX_RENDERTILE, uls, ult, lrs, lrt);

                uint64_t replacementHash = 0;
                bool replacementCheck = rdp->loadTileReplacementCheck(G_TX_LOADTILE, uls, ult, lrs, lrt, scaleBg.imageSiz, scaleBg.imageFmt, scaleBg.imageLoad, scaleBg.imagePal, replacementHash);
                bool singleTileMode = replacementCheck || rdp->loadTileCopyCheck(G_TX_LOADTILE, uls, ult, lrs, lrt);
                state->startSpriteCommand(replacementHash);
                if (singleTileMode) {
                    int32_t uly = frameY0 & ~0x3;
                    rdp->setTileReplacementHash(G_TX_RENDERTILE, replacementHash);
                    rdp->loadTile(G_TX_LOADTILE, uls, ult, lrs, lrt);
                    rdp->drawTexRect(frameX0, uly, frameX1, uly + frameH, G_TX_RENDERTILE, uls, ult, scaleW, scaleH, false);
                    rdp->clearTileReplacementHash(G_TX_RENDERTILE);
                    state->endSpriteCommand();
                    return;
                }
            }

            rdp->setTileSize(G_TX_RENDERTILE, 0, 0, 0, 0);
            
            // Draw the image.
            int16_t imageRemain = tmemSrcLines - imageIY0;
            int16_t imageSliceH = imageISliceL0;
            int32_t frameSliceCount = frameLSliceL0;
            while (frameRemain > 0) {
                int16_t frameSliceH = frameSliceCount >> 10;
                if (frameSliceH <= 0) {
                    imageRemain -= imageSliceH;
                    if (imageRemain > 0) {
                        imagePtr += imageSrcWsize * imageSliceH;
                    }
                    else {
                        imagePtr = imageAddress - (imageRemain * imageSrcWsize) + imagePtrX0;
                        imageRemain += tmemSrcLines;
                    }
                }
                else {
                    frameSliceCount &= 0x3FF;
                    frameRemain -= frameSliceH;

                    // Final slice.
                    if (frameRemain < 0) {
                        frameSliceH += frameRemain;
                        imageSliceH += ((frameRemain * scaleH) >> 10) + 1;
                        imageSliceH = std::min(imageSliceH, imageSliceLines);
                    }

                    bg1CycTMEMLoad(*rdp, scaleBg, imageAddress, imagePtr, imageRemain, imageSrcWsize, imagePtrX0, tmemSrcLines, tmemSliceWmax, imageSliceH, usesBilerp, flagSplit);

                    int16_t framePtrY1 = framePtrY0 + frameSliceH;
                    rdp->drawTexRect(frameX0, framePtrY0 << 2, frameX1, framePtrY1 << 2, G_TX_RENDERTILE, imageS, imageT, scaleW, scaleH, false);
                    framePtrY0 = framePtrY1;
                }

                frameSliceCount += frameSliceLines;
                imageSliceH = imageSliceLines;
                imageT = 0;
            }

            state->endSpriteCommand();

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bg1Cyc::end(0x%08X)", (*dl)->w1);
#       endif
        }
        
        void bgCopy(State *state, DisplayList **dl) {
            // TODO: Reimplement more accurately to match the microcode.

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bgCopy::start(0x%08X)", (*dl)->w1);
#       endif
            
            RDP *rdp = state->rdp.get();
            RSP* rsp = state->rsp.get();
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            // TODO load this into the S2D struct buffer for a more accurate implementation in case there's ever command state bleed.
            const uObjBg *bgObject = reinterpret_cast<const uObjBg *>(state->fromRDRAM(rdramAddress));
            const uObjBg_t &bg = bgObject->bg;
            assert(bg.imageLoad == S2DEX_G_BGLT_LOADTILE); // TODO: Only the load tile version is implemented.
            
            const uint16_t TMEMAddress = 0;
            const uint8_t lineShiftOffset = (bg.imageSiz == G_IM_SIZ_32b) ? 1 : 0;
            const uint16_t bgLine = bg.imageW >> (2 + bg.imageSiz - lineShiftOffset);
            const uint16_t dsdx = 4 << 10;
            const uint16_t dsdy = 4 << 8;
            const uint16_t lrSubstract = 4;
            const uint16_t bgRectLrs = bg.imageW - lrSubstract;
            const uint32_t savedOtherModeH = rdp->otherMode.H;
            rsp->setTextureImage(bg.imageFmt, bg.imageSiz, bg.imageW >> 2, state->rsp->fromSegmented(bg.imageAddress));
            rdp->setTile(G_TX_LOADTILE, bg.imageFmt, bg.imageSiz, bgLine, TMEMAddress, static_cast<uint8_t>(bg.imagePal), 0, 0, 0, 0, 0, 0);
            rdp->setTile(G_TX_RENDERTILE, bg.imageFmt, bg.imageSiz, bgLine, TMEMAddress, static_cast<uint8_t>(bg.imagePal), 0, 0, 0, 0, 0, 0);

            // Attempt to use a single tile instead to draw the entire rect at once if there's a framebuffer copy available.
            // If it fails to find a possible tile copy, fall back to the regular approach.
            // FIXME: The rest of the code should still run to account for state bleeding without drawing any rectangles.
            uint16_t bgRectLrt = bg.imageH - lrSubstract;
            uint64_t replacementHash = 0;
            if (state->ext.enhancementConfig->s2dex.framebufferFastPath) {
                bool replacementCheck = rdp->loadTileReplacementCheck(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt, bg.imageSiz, bg.imageFmt, bg.imageLoad, bg.imagePal, replacementHash);
                bool singleTileMode = replacementCheck || rdp->loadTileCopyCheck(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt);
                state->startSpriteCommand(replacementHash);
                if (singleTileMode) {
                    rdp->setTileReplacementHash(G_TX_RENDERTILE, replacementHash);
                    rdp->setTileSize(G_TX_RENDERTILE, 0, 0, bgRectLrs, bgRectLrt);
                    rdp->loadTile(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt);
                    rdp->drawTexRect(bg.frameX, bg.frameY, bg.frameX + bg.frameW - lrSubstract, bg.frameY + bg.frameH - lrSubstract, G_TX_RENDERTILE, 0, 0, dsdx, dsdy, false);
                    rdp->clearTileReplacementHash(G_TX_RENDERTILE);
                    state->endSpriteCommand();
                    return;
                }
            }

            const uint32_t Division = (bg.imageFmt == G_IM_FMT_CI) ? 2 : 1;
            const uint16_t bgRectCount = bg.imageH / (bg.tmemH / Division); // TODO: Check for remaining pixels for the last Texrect.
            bgRectLrt = (bg.tmemH / Division) - lrSubstract;
            rdp->setTileSize(G_TX_RENDERTILE, 0, 0, bgRectLrs, bgRectLrt);

            for (uint16_t r = 0; r < bgRectCount; r++) {
                const uint16_t bgRectUlt = r * (bg.tmemH / Division);
                rdp->loadTile(G_TX_LOADTILE, 0, bgRectUlt, bgRectLrs, bgRectUlt + bgRectLrt);
                rdp->drawTexRect(bg.frameX, bg.frameY + bgRectUlt, bg.frameX + bg.frameW - lrSubstract, bg.frameY + bgRectUlt + bgRectLrt, G_TX_RENDERTILE, 0, 0, dsdx, dsdy, false);
            }

            state->endSpriteCommand();

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bgCopy::end(0x%08X)", (*dl)->w1);
#       endif
        }

        uint32_t extractBits(uint32_t word, uint8_t pos, uint8_t bits) {
            return ((word >> pos) & ((0x01 << bits) - 1));
        }

        void readS2DStruct(State *state, uint32_t ptr, uint32_t loadSize) {
            // Convert the segmented obj pointer
            uint32_t rdramAddress = state->rsp->fromSegmentedMasked(ptr);
            // Mask the address as the RSP DMA hardware would
            rdramAddress &= RSP_DMA_MASK;
            // Truncate the load size as the ucode does
            loadSize &= 0xFF;
            // Load the struct from RDRAM into the S2D struct buffer
            memcpy(state->rsp->S2D.struct_buffer.data(), state->fromRDRAM(rdramAddress), loadSize);
        }

        void doLoadTxtr(State *state, const uObjTxtr *obj) {
            // Not sure what this does yet so it's just left as the original register name.
            // Maybe used for tracking what the most recent command was so it can insert syncs as needed?
            int32_t r1 = state->rsp->S2D.data_02AE;

            uint32_t sid = (uint8_t)(obj->sid);
            // Must be divisible by 4 and no more than 12 to work correctly.
            // Technically the RSP does support unaligned reads/writes, so perfect simulation would require ditching the divisibility
            // requirement and handling those cases correctly. Handling values above 12 is basically impossible because it would
            // depend on dmem overrun.
            assert((sid & 0b11) == 0 && sid <= 12);

            uint32_t mask = obj->mask;
            uint32_t flag = obj->flag;
            // Get the status for the given id.
            uint32_t status = state->rsp->S2D.statuses[sid / 4];

            // Skip the load if the masked status is equal to the provided flag.
            if ((status & mask) != flag) {
                // Update the status for the given id.
                state->rsp->S2D.statuses[sid / 4] = (status & ~mask) | (flag & mask);
                state->rsp->S2D.data_02AE = -127;

                if (r1 == -127) {
                    // RDP Tile Sync, not needed
                }

                uint32_t tsize = obj->val1;
                state->rdp->setTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, tsize + 1, state->rsp->fromSegmented(obj->image));

                // The most accurate way to implement this is to combine the calculated fields as the RSP does and then decode them as the RDP would.
                uint32_t type_mask = (int8_t)(obj->type >> 16); // The cast to signed is very important here, as sign extension is required.
                uint32_t w0_part1 = ((tsize + 1) & type_mask) << 7;
                uint32_t w0_part2 = ((uint8_t)(obj->type >> 8)) << 16;
                uint32_t settile_w0 = obj->tmem | w0_part1 | w0_part2; // combined settile w0
                uint32_t fmt = extractBits(settile_w0, 21, 3);
                uint32_t siz = extractBits(settile_w0, 19, 2);
                uint32_t line = extractBits(settile_w0, 9, 9);
                uint32_t tmem = extractBits(settile_w0, 0, 9);

                state->rdp->setTile(G_TX_LOADTILE, fmt, siz, line, tmem, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                if (r1 >= 0) {
                    // RDP Load Sync, not needed
                }

                // Same deal as above, but we need to decode differently depending on the command.
                uint32_t load_w1 = ((tsize << 14) | obj->val2) & 0x00FFFFFF;
                // These command ids are masked by 0x3F because the RDP ignores the top two bits.
                uint32_t load_command_id = obj->type & 0x3F;
                if (load_command_id == (G_LOADBLOCK & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    uint32_t dxt = extractBits(load_w1, 0, 12);
                    state->rdp->loadBlock(G_TX_LOADTILE, 0, 0, lrs, dxt);
                } else if (load_command_id == (G_LOADTILE & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    uint32_t lrt = extractBits(load_w1, 0, 12);
                    state->rdp->loadTile(G_TX_LOADTILE, 0, 0, lrs, lrt);
                } else if (load_command_id == (G_LOADTLUT & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    state->rdp->loadTLUT(G_TX_LOADTILE, 0, 0, lrs, 0);
                } else {
                    assert(false && "Invalid sprite load command");
                }
            }
        }

        void objLoadTxtr(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxtr::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxtr::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxSprite(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxSprite::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the sprite draw
            // TODO call doObjSprite(state, &obj->sprite) when it's implemented
            assert(false);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxSprite::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxRect(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRect::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the rect draw
            // TODO call doObjRectangle(state, &obj->sprite) when it's implemented
            assert(false);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRect::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxRectR(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRectR::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);

            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the rect r draw
            // TODO call doObjRectangleR(state, &obj->sprite) when it's implemented
            assert(false);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRectR::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void reset(State *state) {
            state->rsp->objRenderMode = 0x0;
        }

        void setup(GBI *gbi) {
            gbi->constants = {
                { F3DENUM::G_MTX_MODELVIEW, 0x00 },
                { F3DENUM::G_MTX_PROJECTION, 0x01 },
                { F3DENUM::G_MTX_MUL, 0x00 },
                { F3DENUM::G_MTX_LOAD, 0x02 },
                { F3DENUM::G_MTX_NOPUSH, 0x00 },
                { F3DENUM::G_MTX_PUSH, 0x04 },
                { F3DENUM::G_TEXTURE_ENABLE, 0x00000002 },
                { F3DENUM::G_SHADING_SMOOTH, 0x00000200 },
                { F3DENUM::G_CULL_FRONT, 0x00001000 },
                { F3DENUM::G_CULL_BACK, 0x00002000 },
                { F3DENUM::G_CULL_BOTH, 0x00003000 }
            };

            gbi->map[F3D_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[S2DEX_G_OBJ_RENDERMODE] = &objRenderMode;
            gbi->map[S2DEX_G_BG_1CYC] = &bg1Cyc;
            gbi->map[S2DEX_G_BG_COPY] = &bgCopy;
            gbi->map[S2DEX_G_OBJ_LOADTXTR] = &objLoadTxtr;
            gbi->map[S2DEX_G_OBJ_LDTX_SPRITE] = &objLoadTxSprite;
            gbi->map[S2DEX_G_OBJ_LDTX_RECT] = &objLoadTxRect;
            gbi->map[S2DEX_G_OBJ_LDTX_RECT_R] = &objLoadTxRectR;
            gbi->map[F3D_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3D_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[F3D_G_MOVEWORD] = &moveWord;
            gbi->map[F3D_G_SETOTHERMODE_H] = &GBI_F3D::setOtherModeH;
            gbi->map[F3D_G_SETOTHERMODE_L] = &GBI_F3D::setOtherModeL;
            gbi->map[S2DEX_G_RDPHALF_0] = &rdpHalf0;
            gbi->map[F3D_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3D_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3DEX_G_LOAD_UCODE] = &GBI_F3DEX::loadUCode;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;

            gbi->resetFromTask = &reset;
        }
    }
};