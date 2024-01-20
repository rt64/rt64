//
// RT64
//

#pragma once

#ifdef HLSL_CPU
#include <string>
#endif

#include "shared/rt64_hlsl.h"

#include "shared/rt64_other_mode.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct ColorCombiner {
        uint L;
        uint H;

        enum ColorInput {
            C_COMBINED,
            C_TEXEL0,
            C_TEXEL1,
            C_PRIMITIVE,
            C_SHADE,
            C_ENVIRONMENT,
            C_KEY_CENTER,
            C_KEY_SCALE,
            C_COMBINED_ALPHA,
            C_TEXEL0_ALPHA,
            C_TEXEL1_ALPHA,
            C_PRIMITIVE_ALPHA,
            C_SHADE_ALPHA,
            C_ENV_ALPHA,
            C_LOD_FRACTION,
            C_PRIM_LOD_FRAC,
            C_NOISE,
            C_K4,
            C_K5,
            C_ONE,
            C_ZERO
        };

        enum AlphaInput {
            A_COMBINED,
            A_TEXEL0,
            A_TEXEL1,
            A_PRIMITIVE,
            A_SHADE,
            A_ENVIRONMENT,
            A_LOD_FRACTION,
            A_PRIM_LOD_FRAC,
            A_ONE,
            A_ZERO
        };

        ColorInput colorInputCommon(uint index) constmethod {
            switch (index) {
            case 0:
                return C_COMBINED;
            case 1:
                return C_TEXEL0;
            case 2:
                return C_TEXEL1;
            case 3:
                return C_PRIMITIVE;
            case 4:
                return C_SHADE;
            case 5:
                return C_ENVIRONMENT;
            }

            // Should be unreachable.
            return C_ZERO;
        }

        ColorInput colorInputA(uint index) constmethod {
            if (index <= 5) {
                return colorInputCommon(index);
            }

            switch (index) {
            case 6:
                return C_ONE;
            case 7:
                return C_NOISE;
            default:
                return C_ZERO;
            }
        }

        ColorInput colorInputB(uint index) constmethod {
            if (index <= 5) {
                return colorInputCommon(index);
            }

            switch (index) {
            case 6:
                return C_KEY_CENTER;
            case 7:
                return C_K4;
            default:
                return C_ZERO;
            }
        }

        ColorInput colorInputC(uint index) constmethod {
            if (index <= 5) {
                return colorInputCommon(index);
            }

            switch (index) {
            case 6:
                return C_KEY_SCALE;
            case 7:
                return C_COMBINED_ALPHA;
            case 8:
                return C_TEXEL0_ALPHA;
            case 9:
                return C_TEXEL1_ALPHA;
            case 10:
                return C_PRIMITIVE_ALPHA;
            case 11:
                return C_SHADE_ALPHA;
            case 12:
                return C_ENV_ALPHA;
            case 13:
                return C_LOD_FRACTION;
            case 14:
                return C_PRIM_LOD_FRAC;
            case 15:
                return C_K5;
            default:
                return C_ZERO;
            }
        }

        ColorInput colorInputD(uint index) constmethod {
            if (index <= 5) {
                return colorInputCommon(index);
            }

            switch (index) {
            case 6:
                return C_ONE;
            default:
                return C_ZERO;
            }
        }

        AlphaInput alphaInputABD(uint index) constmethod {
            switch (index) {
            case 0:
                return A_COMBINED;
            case 1:
                return A_TEXEL0;
            case 2:
                return A_TEXEL1;
            case 3:
                return A_PRIMITIVE;
            case 4:
                return A_SHADE;
            case 5:
                return A_ENVIRONMENT;
            case 6:
                return A_ONE;
            default:
                return A_ZERO;
            }
        }

        AlphaInput alphaInputC(uint index) constmethod {
            switch (index) {
            case 0:
                return A_LOD_FRACTION;
            case 1:
                return A_TEXEL0;
            case 2:
                return A_TEXEL1;
            case 3:
                return A_PRIMITIVE;
            case 4:
                return A_SHADE;
            case 5:
                return A_ENVIRONMENT;
            case 6:
                return A_PRIM_LOD_FRAC;
            default:
                return A_ZERO;
            }
        }

        uint parseColorInputA(bool secondCycle) constmethod {
            return secondCycle ? (L >> 5U) & 0xFU : (L >> 20U) & 0xFU;
        }

        uint parseColorInputB(bool secondCycle) constmethod {
            return secondCycle ? (H >> 24U) & 0xFU : (H >> 28U) & 0xFU;
        }

        uint parseColorInputC(bool secondCycle) constmethod {
            return secondCycle ? (L >> 0U) & 0x1FU : (L >> 15U) & 0x1FU;
        }

        uint parseColorInputD(bool secondCycle) constmethod {
            return secondCycle ? (H >> 6U) & 0x7U : (H >> 15U) & 0x7U;
        }

        ColorInput decodeColorInput(uint index, bool secondCycle) constmethod {
            switch (index) {
            case 0:
                return colorInputA(parseColorInputA(secondCycle));
            case 1:
                return colorInputB(parseColorInputB(secondCycle));
            case 2:
                return colorInputC(parseColorInputC(secondCycle));
            case 3:
                return colorInputD(parseColorInputD(secondCycle));
            default:
                return C_ZERO;
            }
        }

        uint parseAlphaInputA(bool secondCycle) constmethod {
            return secondCycle ? (H >> 21) & 0x7 : (L >> 12) & 0x7;
        }

        uint parseAlphaInputB(bool secondCycle) constmethod {
            return secondCycle ? (H >> 3) & 0x7 : (H >> 12) & 0x7;
        }

        uint parseAlphaInputC(bool secondCycle) constmethod {
            return secondCycle ? (H >> 18) & 0x7 : (L >> 9) & 0x7;
        }

        uint parseAlphaInputD(bool secondCycle) constmethod {
            return secondCycle ? (H >> 0) & 0x7 : (H >> 9) & 0x7;
        }

        AlphaInput decodeAlphaInput(uint index, bool secondCycle) constmethod {
            switch (index) {
            case 0:
                return alphaInputABD(parseAlphaInputA(secondCycle));
            case 1:
                return alphaInputABD(parseAlphaInputB(secondCycle));
            case 2:
                return alphaInputC(parseAlphaInputC(secondCycle));
            case 3:
                return alphaInputABD(parseAlphaInputD(secondCycle));
            default:
                return A_ZERO;
            }
        }

        uint cycleCount(OtherMode otherMode) constmethod {
            const uint cycleType = otherMode.cycleType();
            switch (cycleType) {
            case G_CYC_2CYCLE:
                return 2;
            case G_CYC_1CYCLE:
                return 1;
            default:
                return 0;
            }
        }

#ifdef HLSL_CPU
        bool usesTexture(const OtherMode otherMode, uint textureIndex, bool hardwareBugCheck) constmethod {
            const uint32_t cycleType = otherMode.cycleType();

            // The first tile is the only one used in copy mode.
            if ((cycleType == G_CYC_COPY)) {
                return (textureIndex == 0);
            }

            // TEXEL1 is not supported in one cycle mode and instead causes an issue. Most code paths should
            // consider the second texture isn't actually used, but the parameter allows to check regardless
            // in case the hardware bug where it shifts TEXEL0 instead should be emulated.
            if (!hardwareBugCheck && (cycleType == G_CYC_1CYCLE)) {
                if (textureIndex == 0) {
                    for (uint i = 0; i < 4; i++) {
                        ColorInput colorInput = decodeColorInput(i, true);
                        if ((colorInput == C_TEXEL0) ||
                            (colorInput == C_TEXEL1) ||
                            (colorInput == C_TEXEL0_ALPHA) ||
                            (colorInput == C_TEXEL1_ALPHA))
                        {
                            return true;
                        }
                    }

                    for (uint i = 0; i < 4; i++) {
                        AlphaInput alphaInput = decodeAlphaInput(i, true);
                        if ((alphaInput == A_TEXEL0) ||
                            (alphaInput == A_TEXEL1))
                        {
                            return true;
                        }
                    }
                }
                else {
                    return false;
                }
            }
            else {
                const uint count = cycleCount(otherMode);
                for (uint c = 0; c < count; c++) {
                    const bool secondCycle = (c > 0);

                    // Read from the second cycle in one-cycle mode.
                    if (count == 1) {
                        c = 1;
                    }

                    const ColorInput texColorInput = secondCycle ?
                        ((textureIndex == 0) ? C_TEXEL1 : C_TEXEL0) :
                        ((textureIndex == 0) ? C_TEXEL0 : C_TEXEL1);

                    const ColorInput texColorAlphaInput = secondCycle ?
                        ((textureIndex == 0) ? C_TEXEL1_ALPHA : C_TEXEL0_ALPHA) :
                        ((textureIndex == 0) ? C_TEXEL0_ALPHA : C_TEXEL1_ALPHA);

                    const AlphaInput texAlphaInput = secondCycle ?
                        ((textureIndex == 0) ? A_TEXEL1 : A_TEXEL0) :
                        ((textureIndex == 0) ? A_TEXEL0 : A_TEXEL1);

                    const bool decodeSecondCycle = (c > 0);
                    for (uint i = 0; i < 4; i++) {
                        ColorInput colorInput = decodeColorInput(i, decodeSecondCycle);
                        if ((colorInput == texColorInput) || (colorInput == texColorAlphaInput)) {
                            return true;
                        }
                    }

                    for (uint i = 0; i < 4; i++) {
                        AlphaInput alphaInput = decodeAlphaInput(i, decodeSecondCycle);
                        if (alphaInput == texAlphaInput) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        std::string colorInputText(ColorInput input) const {
            switch (input) {
            case C_COMBINED:
                return "C_COMBINED";
            case C_TEXEL0:
                return "C_TEXEL0";
            case C_TEXEL1:
                return "C_TEXEL1";
            case C_PRIMITIVE:
                return "C_PRIMITIVE";
            case C_SHADE:
                return "C_SHADE";
            case C_ENVIRONMENT:
                return "C_ENVIRONMENT";
            case C_KEY_CENTER:
                return "C_KEY_CENTER";
            case C_KEY_SCALE:
                return "C_KEY_SCALE";
            case C_COMBINED_ALPHA:
                return "C_COMBINED_ALPHA";
            case C_TEXEL0_ALPHA:
                return "C_TEXEL0_ALPHA";
            case C_TEXEL1_ALPHA:
                return "C_TEXEL1_ALPHA";
            case C_PRIMITIVE_ALPHA:
                return "C_PRIMITIVE_ALPHA";
            case C_SHADE_ALPHA:
                return "C_SHADE_ALPHA";
            case C_ENV_ALPHA:
                return "C_ENV_ALPHA";
            case C_LOD_FRACTION:
                return "C_LOD_FRACTION";
            case C_PRIM_LOD_FRAC:
                return "C_PRIM_LOD_FRAC";
            case C_NOISE:
                return "C_NOISE";
            case C_K4:
                return "C_K4";
            case C_K5:
                return "C_K5";
            case C_ONE:
                return "C_ONE";
            case C_ZERO:
                return "C_ZERO";
            default:
                return "C_UNKNOWN";
            }
        }

        std::string alphaInputText(AlphaInput input) const {
            switch (input) {
            case A_COMBINED:
                return "A_COMBINED";
            case A_TEXEL0:
                return "A_TEXEL0";
            case A_TEXEL1:
                return "A_TEXEL1";
            case A_PRIMITIVE:
                return "A_PRIMITIVE";
            case A_SHADE:
                return "A_SHADE";
            case A_ENVIRONMENT:
                return "A_ENVIRONMENT";
            case A_LOD_FRACTION:
                return "A_LOD_FRACTION";
            case A_PRIM_LOD_FRAC:
                return "A_PRIM_LOD_FRAC";
            case A_ONE:
                return "A_ONE";
            case A_ZERO:
                return "A_ZERO";
            default:
                return "A_UNKNOWN";
            }
        }

        std::string cycleColorText(const uint8_t cycle) const {
            const bool secondCycle = (cycle > 0);
            const uint A = parseColorInputA(secondCycle);
            const uint B = parseColorInputB(secondCycle);
            const uint C = parseColorInputC(secondCycle);
            const uint D = parseColorInputD(secondCycle);
            return "(" +
                colorInputText(colorInputA(A)) + "(" + std::to_string(A) + ") - " +
                colorInputText(colorInputB(B)) + "(" + std::to_string(B) + ")) * " +
                colorInputText(colorInputC(C)) + "(" + std::to_string(C) + ") + " +
                colorInputText(colorInputD(D)) + "(" + std::to_string(D) + ")";
        }

        std::string cycleAlphaText(const uint8_t cycle) const {
            const bool secondCycle = (cycle > 0);
            const uint A = parseAlphaInputA(secondCycle);
            const uint B = parseAlphaInputB(secondCycle);
            const uint C = parseAlphaInputC(secondCycle);
            const uint D = parseAlphaInputD(secondCycle);
            return "(" +
                alphaInputText(alphaInputABD(A)) + "(" + std::to_string(A) + ") - " +
                alphaInputText(alphaInputABD(B)) + "(" + std::to_string(B) + ")) * " +
                alphaInputText(alphaInputC(C)) + "(" + std::to_string(C) + ") + " +
                alphaInputText(alphaInputABD(D)) + "(" + std::to_string(D) + ")";
        }
#else
        struct Inputs {
            OtherMode otherMode;
            bool alphaOnly;
            float4 texVal0;
            float4 texVal1;
            float4 primColor;
            float4 shadeColor;
            float4 envColor;
            float3 keyCenter;
            float3 keyScale;
            float lodFraction;
            float primLodFrac;
            float noise;
            float K4;
            float K5;
        };

        float3 fromColorInput(Inputs inputs, bool secondCycle, ColorInput colorInput, float4 combinerColor) {
            switch (colorInput) {
            case C_COMBINED:
                return combinerColor.rgb;
            case C_TEXEL0:
                return secondCycle ? inputs.texVal1.rgb : inputs.texVal0.rgb;
            case C_TEXEL1:
                return secondCycle ? inputs.texVal0.rgb : inputs.texVal1.rgb;
            case C_PRIMITIVE:
                return inputs.primColor.rgb;
            case C_SHADE:
                return inputs.shadeColor.rgb;
            case C_ENVIRONMENT:
                return inputs.envColor.rgb;
            case C_KEY_CENTER:
                return inputs.keyCenter.rgb;
            case C_KEY_SCALE:
                return inputs.keyScale.rgb;
            case C_COMBINED_ALPHA:
                return combinerColor.a;
            case C_TEXEL0_ALPHA:
                return secondCycle ? inputs.texVal1.a : inputs.texVal0.a;
            case C_TEXEL1_ALPHA:
                return secondCycle ? inputs.texVal0.a : inputs.texVal1.a;
            case C_PRIMITIVE_ALPHA:
                return inputs.primColor.a;
            case C_SHADE_ALPHA:
                return inputs.shadeColor.a;
            case C_ENV_ALPHA:
                return inputs.envColor.a;
            case C_LOD_FRACTION:
                return inputs.lodFraction;
            case C_PRIM_LOD_FRAC:
                return inputs.primLodFrac;
            case C_NOISE:
                return inputs.noise;
            case C_K4:
                return inputs.K4;
            case C_K5:
                return inputs.K5;
            case C_ONE:
                return 1.0f;
            case C_ZERO:
            default:
                return 0.0f;
            }
        }

        float fromAlphaInput(Inputs inputs, bool secondCycle, AlphaInput alphaInput, float combinerAlpha) {
            switch (alphaInput) {
            case A_COMBINED:
                return combinerAlpha;
            case A_TEXEL0:
                return secondCycle ? inputs.texVal1.a : inputs.texVal0.a;
            case A_TEXEL1:
                return secondCycle ? inputs.texVal0.a : inputs.texVal1.a;
            case A_PRIMITIVE:
                return inputs.primColor.a;
            case A_SHADE:
                return inputs.shadeColor.a;
            case A_ENVIRONMENT:
                return inputs.envColor.a;
            case A_LOD_FRACTION:
                return inputs.lodFraction;
            case A_PRIM_LOD_FRAC:
                return inputs.primLodFrac;
            case A_ONE:
                return 1.0f;
            case A_ZERO:
            default:
                return 0.0f;
            }
        }

        void wrap(inout float i, const float Low, const float High) {
            const float Range = (High - Low);
            i += Range * step(i, Low);
            i -= Range * step(High, i);
        }

        void wrapInputC(inout float i) {
            const float Rounding = 1.0f / 255.0f;
            const float LowWrap = -1.0f - Rounding;
            const float HighWrap = 1.0f + Rounding;
            wrap(i, LowWrap, HighWrap);
        }

        void wrapInputABD(inout float i) {
            const float Rounding = 1.0f / 255.0f;
            const float LowWrap = -0.5f - Rounding;
            const float HighWrap = 1.5f + Rounding;
            wrap(i, LowWrap, HighWrap);
        }

        void wrapClamp(inout float i) {
            wrapInputABD(i);
            i = clamp(i, 0.0f, 1.0f);
        }

        void runCycle(Inputs inputs, uint cycle, bool twoCycle, inout float4 combinerColor) {
            const bool secondCycleInputs = (cycle == 1);
            const ColorInput CA = decodeColorInput(0, secondCycleInputs);
            const ColorInput CB = decodeColorInput(1, secondCycleInputs);
            const ColorInput CC = decodeColorInput(2, secondCycleInputs);
            const ColorInput CD = decodeColorInput(3, secondCycleInputs);
            const AlphaInput AA = decodeAlphaInput(0, secondCycleInputs);
            const AlphaInput AB = decodeAlphaInput(1, secondCycleInputs);
            const AlphaInput AC = decodeAlphaInput(2, secondCycleInputs);
            const AlphaInput AD = decodeAlphaInput(3, secondCycleInputs);
            const bool secondCycle = twoCycle && secondCycleInputs;

            // Simulate the wrap on the inputs of the second cycle.
            if (secondCycle) {
                if (AC == A_COMBINED) {
                    wrapInputC(combinerColor.a);
                }
                else {
                    wrapInputABD(combinerColor.a);
                }
            }

            if (!inputs.alphaOnly) {
                if (secondCycle) {
                    if (CC == C_COMBINED) {
                        wrapInputC(combinerColor.r);
                        wrapInputC(combinerColor.g);
                        wrapInputC(combinerColor.b);
                    }
                    else {
                        wrapInputABD(combinerColor.r);
                        wrapInputABD(combinerColor.g);
                        wrapInputABD(combinerColor.b);
                    }
                }

                combinerColor.rgb = (fromColorInput(inputs, secondCycle, CA, combinerColor) - fromColorInput(inputs, secondCycle, CB, combinerColor)) *
                    fromColorInput(inputs, secondCycle, CC, combinerColor) + fromColorInput(inputs, secondCycle, CD, combinerColor);
            }

            combinerColor.a = (fromAlphaInput(inputs, secondCycle, AA, combinerColor.a) - fromAlphaInput(inputs, secondCycle, AB, combinerColor.a)) *
                fromAlphaInput(inputs, secondCycle, AC, combinerColor.a) + fromAlphaInput(inputs, secondCycle, AD, combinerColor.a);
        }

        void run(Inputs inputs, out float4 combinerColor, out float alphaCompareValue) {
            combinerColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

            const uint cycleType = inputs.otherMode.cycleType();
            const bool twoCycle = (cycleType == G_CYC_2CYCLE);
            if (cycleType == G_CYC_COPY) {
                combinerColor = inputs.texVal0.rgba;
            }
            else {
                runCycle(inputs, twoCycle ? 0 : 1, twoCycle, combinerColor);
            }

            alphaCompareValue = combinerColor.a;

            if (cycleType == G_CYC_2CYCLE) {
                runCycle(inputs, 1, twoCycle, combinerColor);
            }

            wrapClamp(combinerColor.r);
            wrapClamp(combinerColor.g);
            wrapClamp(combinerColor.b);
            wrapClamp(combinerColor.a);
            wrapClamp(alphaCompareValue);
        }
#endif
    };
#ifdef HLSL_CPU
};
#endif