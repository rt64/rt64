//
// RT64
//

#include "rt64_gbi_rdp.h"

#include "../include/rt64_extended_gbi.h"

#include "rt64_f3d.h"

namespace RT64 {
    namespace GBI_RDP {
        void noOp(State *state, DisplayList **dl) {
            // Do nothing.
        }

        void setColorImage(State *state, DisplayList **dl) {
            const uint8_t fmt = (*dl)->p0(21, 3);
            const uint8_t siz = (*dl)->p0(19, 2);
            const uint16_t width = (*dl)->p0(0, 12) + 1;
            const uint32_t address = (*dl)->w1;
            state->rdp->setColorImage(fmt, siz, width, address);
        }

        void setDepthImage(State *state, DisplayList **dl) {
            const uint32_t address = (*dl)->w1;
            state->rdp->setDepthImage(address);
        }

        void setTextureImage(State *state, DisplayList **dl) {
            const uint8_t fmt = (*dl)->p0(21, 3);
            const uint8_t siz = (*dl)->p0(19, 2);
            const uint16_t width = (*dl)->p0(0, 12) + 1;
            const uint32_t address = (*dl)->w1;
            state->rdp->setTextureImage(fmt, siz, width, address);
        }

        void setCombine(State *state, DisplayList **dl) {
            const uint64_t combine = (static_cast<uint64_t>((*dl)->w1) << 32) | static_cast<uint64_t>((*dl)->w0);
            state->rdp->setCombine(combine);
        }

        void setTile(State *state, DisplayList **dl) {
            const uint8_t tile = (*dl)->p1(24, 3);
            const uint8_t fmt = (*dl)->p0(21, 3);
            const uint8_t siz = (*dl)->p0(19, 2);
            const uint16_t line = (*dl)->p0(9, 9);
            const uint16_t tmem = (*dl)->p0(0, 9);
            const uint8_t palette = (*dl)->p1(20, 4);
            const uint8_t cmt = (*dl)->p1(18, 2);
            const uint8_t cms = (*dl)->p1(8, 2);
            const uint8_t maskt = (*dl)->p1(14, 4);
            const uint8_t masks = (*dl)->p1(4, 4);
            const uint8_t shiftt = (*dl)->p1(10, 4);
            const uint8_t shifts = (*dl)->p1(0, 4);
            state->rdp->setTile(tile, fmt, siz, line, tmem, palette, cmt, cms, maskt, masks, shiftt, shifts);
        }

        void setTileSize(State *state, DisplayList **dl) {
            const uint8_t tile = (*dl)->p1(24, 3);
            const uint16_t uls = (*dl)->p0(12, 12);
            const uint16_t ult = (*dl)->p0(0, 12);
            const uint16_t lrs = (*dl)->p1(12, 12);
            const uint16_t lrt = (*dl)->p1(0, 12);
            state->rdp->setTileSize(tile, uls, ult, lrs, lrt);
        }

        void loadTile(State *state, DisplayList **dl) {
            const uint8_t tile = (*dl)->p1(24, 3);
            const uint16_t uls = (*dl)->p0(12, 12);
            const uint16_t ult = (*dl)->p0(0, 12);
            const uint16_t lrs = (*dl)->p1(12, 12);
            const uint16_t lrt = (*dl)->p1(0, 12);
            state->rdp->loadTile(tile, uls, ult, lrs, lrt);
        }

        void loadBlock(State *state, DisplayList **dl) {
            const uint8_t tile = (*dl)->p1(24, 3);
            const uint16_t uls = (*dl)->p0(12, 12);
            const uint16_t ult = (*dl)->p0(0, 12);
            const uint16_t lrs = (*dl)->p1(12, 12);
            const uint16_t dxt = (*dl)->p1(0, 12);
            state->rdp->loadBlock(tile, uls, ult, lrs, dxt);
        }

        void loadTLUT(State *state, DisplayList **dl) {
            const uint8_t tile = (*dl)->p1(24, 3);
            const uint16_t uls = (*dl)->p0(12, 12);
            const uint16_t ult = (*dl)->p0(0, 12);
            const uint16_t lrs = (*dl)->p1(12, 12);
            const uint16_t lrt = (*dl)->p1(0, 12);
            state->rdp->loadTLUT(tile, uls, ult, lrs, lrt);
        }

        void setEnvColor(State *state, DisplayList **dl) {
            const uint32_t color = (*dl)->w1;
            state->rdp->setEnvColor(color);
        }

        void setPrimColor(State *state, DisplayList **dl) {
            // While the manual states that lodMin has 8 bits of precision, the RDP only uses 5 of them.
            const uint8_t lodFrac = (*dl)->p0(0, 8);
            const uint8_t lodMin = (*dl)->p0(8, 5); 
            const uint32_t color = (*dl)->w1;
            state->rdp->setPrimColor(lodFrac, lodMin, color);
        }

        void setBlendColor(State *state, DisplayList **dl) {
            const uint32_t color = (*dl)->w1;
            state->rdp->setBlendColor(color);
        }

        void setFogColor(State *state, DisplayList **dl) {
            const uint32_t color = (*dl)->w1;
            state->rdp->setFogColor(color);
        }

        void setFillColor(State *state, DisplayList **dl) {
            const uint32_t color = (*dl)->w1;
            state->rdp->setFillColor(color);
        }

        void setOtherMode(State *state, DisplayList **dl) {
            const uint32_t high = (*dl)->p0(0, 24);
            const uint32_t low = (*dl)->w1;
            state->rdp->setOtherMode(high, low);
        }

        void setPrimDepth(State *state, DisplayList **dl) {
            const uint16_t z = (*dl)->p1(16, 16);
            const uint16_t dz = (*dl)->p1(0, 16);
            state->rdp->setPrimDepth(z, dz);
        }
        
        void setScissor(State *state, DisplayList **dl) {
            const uint8_t mode = (*dl)->p1(24, 2);
            const int32_t ulx = (*dl)->p0(12, 12);
            const int32_t uly = (*dl)->p0(0, 12);
            const int32_t lrx = (*dl)->p1(12, 12);
            const int32_t lry = (*dl)->p1(0, 12);
            state->rdp->setScissor(mode, ulx, uly, lrx, lry);
        }

        void setConvert(State *state, DisplayList **dl) {
            const int32_t k0 = (*dl)->p0(13, 9);
            const int32_t k1 = (*dl)->p0(4, 9);
            const int32_t k2 = ((*dl)->p0(0, 4) << 5) | (*dl)->p1(27, 5);
            const int32_t k3 = (*dl)->p1(18, 9);
            const int32_t k4 = (*dl)->p1(9, 9);
            const int32_t k5 = (*dl)->p1(0, 9);
            state->rdp->setConvert(k0, k1, k2, k3, k4, k5);
        }

        void setKeyR(State *state, DisplayList **dl) {
            const uint32_t cR = (*dl)->p1(8, 8);
            const uint32_t sR = (*dl)->p1(0, 8);
            const uint32_t wR = (*dl)->p1(16, 12);
            state->rdp->setKeyR(cR, sR, wR);
        }

        void setKeyGB(State *state, DisplayList **dl) {
            const uint32_t cG = (*dl)->p1(24, 8);
            const uint32_t sG = (*dl)->p1(16, 8);
            const uint32_t wG = (*dl)->p0(12, 12);
            const uint32_t cB = (*dl)->p1(8, 8);
            const uint32_t sB = (*dl)->p1(0, 8);
            const uint32_t wB = (*dl)->p0(0, 12);
            state->rdp->setKeyGB(cG, sG, wG, cB, sB, wB);
        }

        void texrect(State *state, DisplayList **dl) {
            const int32_t ulx = (*dl)->p1(12, 12);
            const int32_t uly = (*dl)->p1(0, 12);
            const int32_t lrx = (*dl)->p0(12, 12);
            const int32_t lry = (*dl)->p0(0, 12);
            const uint8_t tile = (*dl)->p1(24, 3);
            *dl = *dl + 1;

            const int16_t uls = (*dl)->p1(16, 16);
            const int16_t ult = (*dl)->p1(0, 16);
            *dl = *dl + 1;
            
            const int16_t dsdx = (*dl)->p1(16, 16);
            const int16_t dtdy = (*dl)->p1(0, 16);
            state->rdp->drawTexRect(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, false);
        }
        
        void texrectFlip(State *state, DisplayList **dl) {
            const int32_t ulx = (*dl)->p1(12, 12);
            const int32_t uly = (*dl)->p1(0, 12);
            const int32_t lrx = (*dl)->p0(12, 12);
            const int32_t lry = (*dl)->p0(0, 12);
            const uint8_t tile = (*dl)->p1(24, 3);
            *dl = *dl + 1;

            const int16_t uls = (*dl)->p1(16, 16);
            const int16_t ult = (*dl)->p1(0, 16);
            *dl = *dl + 1;

            const int16_t dsdx = (*dl)->p1(16, 16);
            const int16_t dtdy = (*dl)->p1(0, 16);
            
            state->rdp->drawTexRect(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, true);
        }

        // TODO replace the normal rect commands with these and pass a temporary RDP buffer when rdphalf_2 is run in the RSP
        void texrectLLE(State* state, DisplayList** dl) {
            const int32_t ulx = (*dl)[0].p1(12, 12);
            const int32_t uly = (*dl)[0].p1(0, 12);
            const int32_t lrx = (*dl)[0].p0(12, 12);
            const int32_t lry = (*dl)[0].p0(0, 12);
            const uint8_t tile = (*dl)[0].p1(24, 3);

            const int16_t uls = (*dl)[1].p0(16, 16);
            const int16_t ult = (*dl)[1].p0(0, 16);
            const int16_t dsdx = (*dl)[1].p1(16, 16);
            const int16_t dtdy = (*dl)[1].p1(0, 16);
            state->rdp->drawTexRect(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, false);
        }

        void texrectFlipLLE(State* state, DisplayList** dl) {
            const int32_t ulx = (*dl)[0].p1(12, 12);
            const int32_t uly = (*dl)[0].p1(0, 12);
            const int32_t lrx = (*dl)[0].p0(12, 12);
            const int32_t lry = (*dl)[0].p0(0, 12);
            const uint8_t tile = (*dl)[0].p1(24, 3);

            const int16_t uls = (*dl)[1].p0(16, 16);
            const int16_t ult = (*dl)[1].p0(0, 16);
            const int16_t dsdx = (*dl)[1].p1(16, 16);
            const int16_t dtdy = (*dl)[1].p1(0, 16);
            state->rdp->drawTexRect(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, true);
        }

        void fillRect(State *state, DisplayList **dl) {
            const int32_t ulx = (*dl)->p1(12, 12);
            const int32_t uly = (*dl)->p1(0, 12);
            const int32_t lrx = (*dl)->p0(12, 12);
            const int32_t lry = (*dl)->p0(0, 12);
            state->rdp->fillRect(ulx, uly, lrx, lry);
        }

        void loadSync(State *state, DisplayList **dl) {
            // Do nothing.
        }

        void pipeSync(State *state, DisplayList **dl) {
            // Do nothing.
        }

        void tileSync(State *state, DisplayList **dl) {
            // Do nothing.
        }

        void fullSync(State *state, DisplayList **dl) {
            RT64_LOG_PRINTF("GBI_RDP::fullSync()");
            state->fullSync();
            state->dpInterrupt();
        }

        void getTrianglePointers(DisplayList *triangleData, size_t arrayLength, DisplayList *&trianglesEnd, std::vector<const DisplayList *> &triPointers, bool &overrun) {
            DisplayList *curCommand = triangleData;
            const DisplayList *endCommand = triangleData + arrayLength;
            triPointers.clear();
            overrun = false;

            // Check commands until we hit the end of the buffer or until we hit a command that isn't a triangle.
            while (curCommand < endCommand) {
                uint32_t commandId = (curCommand->w0 >> 24) & 0x3F;

                // Stop counting if this isn't a triangle command.
                if (commandId < (uint32_t)RDPTriangle::Base || commandId >(uint32_t)RDPTriangle::MaxValue) {
                    break;
                }

                // Determine the triangle type.
                bool shaded = (commandId & (uint32_t)RDPTriangle::Shaded) != 0;
                bool textured = (commandId & (uint32_t)RDPTriangle::Textured) != 0;
                bool hasDepth = (commandId & (uint32_t)RDPTriangle::Depth) != 0;

                // Determine the length of this triangle command.
                size_t commandLength = triangleBaseWords;

                if (shaded) {
                    commandLength += triangleShadeWords;
                }

                if (textured) {
                    commandLength += triangleTexWords;
                }

                if (hasDepth) {
                    commandLength += triangleDepthWords;
                }

                // Check if this triangle runs past the end of the buffer.
                if (curCommand + commandLength > endCommand) {
                    overrun = true;
                    break;
                }

                // Advance the command pointer by the length of this command.
                triPointers.push_back(curCommand);
                curCommand += commandLength;

                // TODO do more than 1 triangle
                break;
            }

            trianglesEnd = curCommand;
        }

        template <typename T>
        void setMinSize(std::vector<T>& vec, size_t min_size) {
            if (vec.size() < min_size) {
                vec.resize(min_size + min_size / 2);
            }
        }

        DisplayList* decodeTriangles(State* state, DisplayList* triangleData, size_t array_length) {
            DisplayList* trianglesEnd;
            bool overrun;

            const bool texturePerspective = (state->rdp->otherMode.textPersp() == G_TP_PERSP);
            std::vector<const DisplayList *> &triangles = state->rdp->triPointerBuffer;
            getTrianglePointers(triangleData, array_length, trianglesEnd, triangles, overrun);
            size_t vertexCount = triangles.size() * 3;

            std::vector<interop::float4> &posWorkBuffer = state->rdp->triPosWorkBuffer;
            std::vector<interop::float4> &colorWorkBuffer = state->rdp->triColWorkBuffer;
            std::vector<interop::float2> &texcoordWorkBuffer = state->rdp->triTcWorkBuffer;
            setMinSize(posWorkBuffer, vertexCount);
            setMinSize(colorWorkBuffer, vertexCount);
            setMinSize(texcoordWorkBuffer, vertexCount);

            size_t workBufferIndex = 0;
            uint32_t tile = -1;
            uint32_t level = -1;

            for (const DisplayList* curData : triangles) {
                uint32_t commandId = (curData[0].w0 >> 24) & 0x3F;
                // TODO flush when tile and level change
                tile = (curData[0].w0 >> 16) & 0x7;
                level = (curData[0].w0 >> 19) & 0x7;

                if (commandId < (uint32_t)RDPTriangle::Base || commandId >(uint32_t)RDPTriangle::MaxValue) {
                    RT64_LOG_PRINTF("Not a triangle! Got command 0x%02X\n", commandId);
                }

                bool hasDepth = (commandId & (uint32_t)RDPTriangle::Depth) != 0;
                bool textured = (commandId & (uint32_t)RDPTriangle::Textured) != 0;
                bool shaded = (commandId & (uint32_t)RDPTriangle::Shaded) != 0;

                // int flip = (curData[0].w0 >> 23) & 1;
                // Extract the coords and slopes
                int16_t ylFixed = (curData[0].w0 & 0x0000FFFF) << 2 >> 2;
                int16_t ymFixed = (curData[0].w1 >> 16) << 2 >> 2;
                int16_t yhFixed = (curData[0].w1 & 0x0000FFFF) << 2 >> 2;

                int32_t xlFixed = curData[1].w0;
                int32_t dxldyFixed = curData[1].w1;
                int32_t xhFixed = curData[2].w0;
                int32_t dxhdyFixed = curData[2].w1;
                int32_t xmFixed = curData[3].w0;
                int32_t dxmdyFixed = curData[3].w1;

                // Convert from fixed to floating point
                float yl = ylFixed / 4.0f;
                float ym = ymFixed / 4.0f;
                float yh = yhFixed / 4.0f;

                float y1 = yh;
                float y2 = yl;

                float xl = xlFixed / 65536.0f;
                float xm = xmFixed / 65536.0f;
                float xh = xhFixed / 65536.0f;

                float dxldy = dxldyFixed / 65536.0f;
                float dxmdy = dxmdyFixed / 65536.0f;
                float dxhdy = dxhdyFixed / 65536.0f;

                // Round yh down (which translates to up on the screen) to the nearest scanline to get the y-coordinate of XM and XH
                float yFloor = floorf(yh);

                // Calculate the x intercepts of the H and M line
                float hIntercept = xh - dxhdy * yFloor;
                float mIntercept = xm - dxmdy * yFloor;

                // Using the equation of the H line, calculate the x coordinate on H at y=yh
                float x1 = dxhdy * y1 + hIntercept;

                // Using the equation of the H line, calculate the x coordinate on H at y=yl
                float x2 = dxhdy * y2 + hIntercept;

                // Calculate the x intercept of the L line
                float l_intercept = x2 - dxldy * y2;

                // Get the third vertex from the inputs
                float x3 = xl;
                float y3 = ym;

                posWorkBuffer[workBufferIndex + 0].x = x1;
                posWorkBuffer[workBufferIndex + 0].y = y1;
                posWorkBuffer[workBufferIndex + 1].x = x2;
                posWorkBuffer[workBufferIndex + 1].y = y2;
                posWorkBuffer[workBufferIndex + 2].x = x3;
                posWorkBuffer[workBufferIndex + 2].y = y3;

                curData += triangleBaseWords;

                // Calculate y distance from each vertex to the start of the major edge which is used for other coefficients.
                float dy_1 = y1 - yFloor;
                float dy_2 = y2 - yFloor;
                float dy_3 = y3 - yFloor;

                // Get the x coordinate of the point at y=y3 on the opposite edge of the third vertex.
                float x3_opposite = dxhdy * y3 + hIntercept;

                // Calculate x distance from the point opposite vertex 3 to vertex 3 itself.
                float dx_3 = x3 - x3_opposite;

                if (shaded) {
                    hlslpp::int4 baseColorFixed{
                        ((curData[0].w0 >> 16) << 16) | (curData[2].w0 >> 16),
                        ((curData[0].w0 & 0xFFFF) << 16) | (curData[2].w0 & 0xFFFF),
                        ((curData[0].w1 >> 16) << 16) | (curData[2].w1 >> 16),
                        ((curData[0].w1 & 0xFFFF) << 16) | (curData[2].w1 & 0xFFFF)
                    };

                    hlslpp::int4 colorDxFixed{
                        ((curData[1].w0 >> 16) << 16) | (curData[3].w0 >> 16),
                        ((curData[1].w0 & 0xFFFF) << 16) | (curData[3].w0 & 0xFFFF),
                        ((curData[1].w1 >> 16) << 16) | (curData[3].w1 >> 16),
                        ((curData[1].w1 & 0xFFFF) << 16) | (curData[3].w1 & 0xFFFF),
                    };

                    hlslpp::int4 colorDeFixed{
                        ((curData[4].w0 >> 16) << 16) | (curData[6].w0 >> 16),
                        ((curData[4].w0 & 0xFFFF) << 16) | (curData[6].w0 & 0xFFFF),
                        ((curData[4].w1 >> 16) << 16) | (curData[6].w1 >> 16),
                        ((curData[4].w1 & 0xFFFF) << 16) | (curData[6].w1 & 0xFFFF),
                    };

                    // dy seems to only be used on edge pixels for anti aliasing purposes
                    // hlslpp::int4 colorDyFixed {
                    //     ((curData[5].w0 >> 16)    << 16) | (curData[7].w0 >> 16),
                    //     ((curData[5].w0 & 0xFFFF) << 16) | (curData[7].w0 & 0xFFFF),
                    //     ((curData[5].w1 >> 16)    << 16) | (curData[7].w1 >> 16),
                    //     ((curData[5].w1 & 0xFFFF) << 16) | (curData[7].w1 & 0xFFFF),
                    // };

                    hlslpp::float4 baseColor = hlslpp::float4(baseColorFixed) * (1.0f / 65536.0f);
                    hlslpp::float4 colorDx = hlslpp::float4(colorDxFixed) * (1.0f / 65536.0f);
                    hlslpp::float4 colorDe = hlslpp::float4(colorDeFixed) * (1.0f / 65536.0f);
                    // hlslpp::float4 colorDy   = hlslpp::float4(colorDyFixed)   * (1.0f / 65536.0f);

                    // Offset the base color (which seems to start at XH) by the distance between YH and the y-coord of XH.
                    hlslpp::float4 v1Color = baseColor + colorDe * dy_1;
                    // Use the edge derivative to calculate the color at the middle vertex from the color at the major vertex
                    hlslpp::float4 v2Color = baseColor + colorDe * dy_2;
                    // Use the edge derivative to calculate the color at the opposite side of the last vertex from the color at the second vertex
                    hlslpp::float4 v3OppositeColor = baseColor + colorDe * dy_3;
                    // Use the x derivative to calculate the color at the last vertex, starting from its opposite point
                    hlslpp::float4 v3Color = v3OppositeColor + colorDx * dx_3;

                    colorWorkBuffer[workBufferIndex + 0] = v1Color * (1/255.0f);
                    colorWorkBuffer[workBufferIndex + 1] = v2Color * (1/255.0f);
                    colorWorkBuffer[workBufferIndex + 2] = v3Color * (1/255.0f);

                    curData += triangleShadeWords;
                }
                // No shade coefficients
                else {
                    colorWorkBuffer[workBufferIndex + 0] = interop::float4(0.0f, 0.0f, 0.0f, 0.0f);
                    colorWorkBuffer[workBufferIndex + 1] = interop::float4(0.0f, 0.0f, 0.0f, 0.0f);
                    colorWorkBuffer[workBufferIndex + 2] = interop::float4(0.0f, 0.0f, 0.0f, 0.0f);
                }

                if (textured) {
                    hlslpp::int3 baseTexcoordFixed{
                        ((curData[0].w0 >> 16) << 16) | (curData[2].w0 >> 16),
                        ((curData[0].w0 & 0xFFFF) << 16) | (curData[2].w0 & 0xFFFF),
                        ((curData[0].w1 >> 16) << 16) | (curData[2].w1 >> 16)
                    };

                    hlslpp::int3 texcoordDxFixed{
                        ((curData[1].w0 >> 16) << 16) | (curData[3].w0 >> 16),
                        ((curData[1].w0 & 0xFFFF) << 16) | (curData[3].w0 & 0xFFFF),
                        ((curData[1].w1 >> 16) << 16) | (curData[3].w1 >> 16)
                    };

                    hlslpp::int3 texcoordDeFixed{
                        ((curData[4].w0 >> 16) << 16) | (curData[6].w0 >> 16),
                        ((curData[4].w0 & 0xFFFF) << 16) | (curData[6].w0 & 0xFFFF),
                        ((curData[4].w1 >> 16) << 16) | (curData[6].w1 >> 16)
                    };

                    // dy seems to only be used on edge pixels for anti aliasing purposes
                    hlslpp::int3 texcoordDyFixed{
                        ((curData[5].w0 >> 16) << 16) | (curData[7].w0 >> 16),
                        ((curData[5].w0 & 0xFFFF) << 16) | (curData[7].w0 & 0xFFFF),
                        ((curData[5].w1 >> 16) << 16) | (curData[7].w1 >> 16)
                    };

                    hlslpp::float3 baseTexcoord = hlslpp::float3(baseTexcoordFixed) * (1.0f / 65536.0f);
                    hlslpp::float3 texcoordDx = hlslpp::float3(texcoordDxFixed) * (1.0f / 65536.0f);
                    hlslpp::float3 texcoordDe = hlslpp::float3(texcoordDeFixed) * (1.0f / 65536.0f);
                    hlslpp::float3 texcoordDy = hlslpp::float3(texcoordDyFixed) * (1.0f / 65536.0f);

                    // Same calculations as shade color
                    float w_base = baseTexcoord[2];
                    float w1 = w_base + texcoordDe[2] * dy_1;
                    float w2 = w_base + texcoordDe[2] * dy_2;
                    float w3_opposite = w_base + texcoordDe[2] * dy_3;
                    float w3 = w3_opposite + texcoordDx[2] * dx_3;

                    // Repeat the calculations but with perspective correction
                    hlslpp::float2 v1_texcoord = baseTexcoord.xy + texcoordDe.xy * dy_1;
                    hlslpp::float2 v2_texcoord = baseTexcoord.xy + texcoordDe.xy * dy_2;
                    hlslpp::float2 v3_opposite_texcoord = baseTexcoord.xy + texcoordDe.xy * dy_3;
                    hlslpp::float2 v3_texcoord = v3_opposite_texcoord + texcoordDx.xy * dx_3;
                    if (texturePerspective) {
                        posWorkBuffer[workBufferIndex + 0].w = 65536000.0f / w1;
                        posWorkBuffer[workBufferIndex + 1].w = 65536000.0f / w2;
                        posWorkBuffer[workBufferIndex + 2].w = 65536000.0f / w3;
                        texcoordWorkBuffer[workBufferIndex + 0] = (v1_texcoord / w1) * 1024.0f;
                        texcoordWorkBuffer[workBufferIndex + 1] = (v2_texcoord / w2) * 1024.0f;
                        texcoordWorkBuffer[workBufferIndex + 2] = (v3_texcoord / w3) * 1024.0f;
                    }
                    else {
                        posWorkBuffer[workBufferIndex + 0].w = 1.0f;
                        posWorkBuffer[workBufferIndex + 1].w = 1.0f;
                        posWorkBuffer[workBufferIndex + 2].w = 1.0f;
                        texcoordWorkBuffer[workBufferIndex + 0] = (v1_texcoord * 1024.0f) / 16384.0f;
                        texcoordWorkBuffer[workBufferIndex + 1] = (v2_texcoord * 1024.0f) / 16384.0f;
                        texcoordWorkBuffer[workBufferIndex + 2] = (v3_texcoord * 1024.0f) / 16384.0f;
                    }

                    curData += triangleTexWords;
                }
                // No texture coefficients
                else {
                    posWorkBuffer[workBufferIndex + 0].w = 1.0f;
                    posWorkBuffer[workBufferIndex + 1].w = 1.0f;
                    posWorkBuffer[workBufferIndex + 2].w = 1.0f;
                    texcoordWorkBuffer[workBufferIndex + 0] = interop::float2(0.0f, 0.0f);
                    texcoordWorkBuffer[workBufferIndex + 1] = interop::float2(0.0f, 0.0f);
                    texcoordWorkBuffer[workBufferIndex + 2] = interop::float2(0.0f, 0.0f);
                }

                if (hasDepth) {
                    int baseDepthFixed = curData[0].w0;
                    int depthDxFixed = curData[0].w1;
                    int depthDeFixed = curData[1].w0;
                    // dy seems to only be used on edge pixels for anti aliasing purposes
                    int depthDyFixed = curData[1].w1;

                    float baseDepth = baseDepthFixed * (1.0f / 65536.0f / 32768.0f);
                    float depthDx = depthDxFixed * (1.0f / 65536.0f / 32768.0f);
                    float depthDe = depthDeFixed * (1.0f / 65536.0f / 32768.0f);
                    float depthDy = depthDyFixed * (1.0f / 65536.0f / 32768.0f);

                    // Same calculations as shade color and texcoords
                    float v1Depth = baseDepth + depthDe * dy_1;
                    float v2Depth = baseDepth + depthDe * dy_2;
                    float v3_opposite_depth = baseDepth + depthDe * dy_3;
                    float v3Depth = v3_opposite_depth + depthDx * dx_3;

                    posWorkBuffer[workBufferIndex + 0].z = v1Depth;
                    posWorkBuffer[workBufferIndex + 1].z = v2Depth;
                    posWorkBuffer[workBufferIndex + 2].z = v3Depth;

                    curData += triangleDepthWords;
                }
                // No depth coefficients
                else {
                    posWorkBuffer[workBufferIndex + 0].z = 0.0f;
                    posWorkBuffer[workBufferIndex + 1].z = 0.0f;
                    posWorkBuffer[workBufferIndex + 2].z = 0.0f;
                }

                // TODO do more than 1 tri at a time
                break;
            }

            state->rdp->drawTris(1, &posWorkBuffer.data()[0][0], &texcoordWorkBuffer.data()[0][0], &colorWorkBuffer[0][0], tile, level);

            return trianglesEnd;
        }

        void tri(State* state, DisplayList** dl) {
            // TODO pass the real length through
            *dl = decodeTriangles(state, *dl, 0x100);
        }

        void setup(GBI *gbi, bool isHLE) {
            // Mask the commands to 6 bits for LLE usage.
            unsigned int commandMask = isHLE ? 0xFF : 0x3F;
            gbi->map[G_NOOP & commandMask] = &noOp;
            gbi->map[G_SETCIMG & commandMask] = &setColorImage;
            gbi->map[G_SETZIMG & commandMask] = &setDepthImage;
            gbi->map[G_SETTIMG & commandMask] = &setTextureImage;
            gbi->map[G_SETCOMBINE & commandMask] = &setCombine;
            gbi->map[G_SETTILE & commandMask] = &setTile;
            gbi->map[G_SETTILESIZE & commandMask] = &setTileSize;
            gbi->map[G_LOADTILE & commandMask] = &loadTile;
            gbi->map[G_LOADBLOCK & commandMask] = &loadBlock;
            gbi->map[G_LOADTLUT & commandMask] = &loadTLUT;
            gbi->map[G_SETENVCOLOR & commandMask] = &setEnvColor;
            gbi->map[G_SETPRIMCOLOR & commandMask] = &setPrimColor;
            gbi->map[G_SETBLENDCOLOR & commandMask] = &setBlendColor;
            gbi->map[G_SETFOGCOLOR & commandMask] = &setFogColor;
            gbi->map[G_SETFILLCOLOR & commandMask] = &setFillColor;
            gbi->map[G_RDPSETOTHERMODE & commandMask] = &setOtherMode;
            gbi->map[G_SETPRIMDEPTH & commandMask] = &setPrimDepth;
            gbi->map[G_SETSCISSOR & commandMask] = &setScissor;
            gbi->map[G_SETCONVERT & commandMask] = &setConvert;
            gbi->map[G_SETKEYR & commandMask] = &setKeyR;
            gbi->map[G_SETKEYGB & commandMask] = &setKeyGB;
            gbi->map[G_TEXRECT & commandMask] = isHLE ? &texrect : &texrectLLE;
            gbi->map[G_TEXRECTFLIP & commandMask] = isHLE ? &texrectFlip : &texrectFlipLLE;
            gbi->map[G_FILLRECT & commandMask] = &fillRect;
            gbi->map[G_RDPLOADSYNC & commandMask] = &loadSync;
            gbi->map[G_RDPPIPESYNC & commandMask] = &pipeSync;
            gbi->map[G_RDPTILESYNC & commandMask] = &tileSync;
            gbi->map[G_RDPFULLSYNC & commandMask] = &fullSync;

            // Map the triangle commands in the LLE gbi.
            if (!isHLE) {
                // Register all 8 RDP tri command IDs to the generic tri handler.
                for (unsigned int commandId = G_RDPTRI_BASE; commandId < G_RDPTRI_BASE + 8; commandId++) {
                    gbi->map[commandId] = &tri;
                }
            }
        }
    }
};
