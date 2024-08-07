//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#include "shared/rt64_other_mode.h"
#include "shared/rt64_render_flags.h"
#include "shared/rt64_render_params.h"

#ifdef HLSL_CPU
#include <string>
namespace interop {
#endif
    struct Blender {
        enum InputPM {
            PM_CC_OR_BLENDER = 0,
            PM_FRAMEBUFFER_COLOR = 1,
            PM_BLEND_COLOR = 2,
            PM_FOG_COLOR = 3
        };

        enum InputA {
            A_CC_ALPHA = 0,
            A_FOG_ALPHA = 1,
            A_SHADE_ALPHA = 2,
            A_ZERO = 3
        };

        enum InputB {
            B_ONE_MINUS_A = 0,
            B_FRAMEBUFFER_ALPHA = 1,
            B_ONE = 2,
            B_ZERO = 3
        };

        enum class Approximation {
            None,
            CombinerFramebuffer1MA_SquareMix,
            AnyFramebuffer1MA_MultiplyMix
        };

        static uint combineCycleCount(const OtherMode otherMode) {
            const uint cycleType = otherMode.cycleType();
            if (cycleType == G_CYC_2CYCLE) {
                return 2;
            }
            else if (cycleType == G_CYC_1CYCLE) {
                return 1;
            }
            else {
                return 0;
            }
        }
        
        static uint blendCycleCount(const OtherMode otherMode) {
            const uint ccCount = combineCycleCount(otherMode);
            if (otherMode.forceBlend()) {
                return ccCount;
            }
            else {
                return (ccCount > 0) ? (ccCount - 1) : 0;
            }
        }

        static InputPM decodeInputP(uint blenderInputs, bool secondCycle) {
            return secondCycle ? (InputPM)((blenderInputs >> 12) & 0x3) : (InputPM)((blenderInputs >> 14) & 0x3);
        }

        static InputPM decodeInputM(uint blenderInputs, bool secondCycle) {
            return secondCycle ? (InputPM)((blenderInputs >> 4) & 0x3) : (InputPM)((blenderInputs >> 6) & 0x3);
        }

        static InputA decodeInputA(uint blenderInputs, bool secondCycle) {
            return secondCycle ? (InputA)((blenderInputs >> 8) & 0x3) : (InputA)((blenderInputs >> 10) & 0x3);
        }

        static InputB decodeInputB(uint blenderInputs, bool secondCycle) {
            return secondCycle ? (InputB)((blenderInputs >> 0) & 0x3) : (InputB)((blenderInputs >> 2) & 0x3);
        }

        static bool usesInput(const OtherMode otherMode, InputA inputA) {
            const uint32_t cycles = blendCycleCount(otherMode);
            const uint blenderInputs = otherMode.blenderInputs();
            for (uint32_t c = 0; c < cycles; c++) {
                if (decodeInputA(blenderInputs, c > 0) == inputA) {
                    return true;
                }
            }

            return false;
        }

        static bool usesCombinerAlpha(const OtherMode otherMode) {
            return usesInput(otherMode, Blender::A_CC_ALPHA);
        }

        static bool usesAlphaBlendCycle(const OtherMode otherMode, bool secondCycle, bool allInputs) {
            const uint blenderInputs = otherMode.blenderInputs();
            const InputPM P = decodeInputP(blenderInputs, secondCycle);
            if (allInputs) {
                const InputA A = decodeInputA(blenderInputs, secondCycle);
                if ((P == PM_FRAMEBUFFER_COLOR) && (A != A_ZERO)) {
                    return true;
                }

                const InputPM M = decodeInputM(blenderInputs, secondCycle);
                const InputB B = decodeInputB(blenderInputs, secondCycle);
                if ((M == PM_FRAMEBUFFER_COLOR) && (B != B_ZERO)) {
                    return true;
                }
            }
            else if (P == PM_FRAMEBUFFER_COLOR) {
                return true;
            }

            return false;
        }
        
        static bool usesAlphaBlend(const OtherMode otherMode) {
            const bool forceBlend = otherMode.forceBlend();
            const uint ccCount = combineCycleCount(otherMode);
            if ((ccCount >= 2) && usesAlphaBlendCycle(otherMode, true, forceBlend)) {
                return true;
            }

            if ((ccCount >= 1) && usesAlphaBlendCycle(otherMode, false, (ccCount >= 2) || forceBlend)) {
                return true;
            }

            return false;
        }

        static bool usesStandardFogCycle(uint blenderInputs, uint cycleIndex) {
            const bool secondCycle = (cycleIndex > 0);
            const InputPM P = decodeInputP(blenderInputs, secondCycle);
            const InputPM M = decodeInputM(blenderInputs, secondCycle);
            const InputA A = decodeInputA(blenderInputs, secondCycle);
            const InputB B = decodeInputB(blenderInputs, secondCycle);
            return (P == PM_FOG_COLOR) && (A == A_SHADE_ALPHA) && (M == PM_CC_OR_BLENDER) && (B == B_ONE_MINUS_A);
        }

        static bool usesStandardFogCycle(const OtherMode otherMode) {
            const uint cycles = blendCycleCount(otherMode);
            const uint blenderInputs = otherMode.blenderInputs();
            for (uint c = 0; c < cycles; c++) {
                if (usesStandardFogCycle(blenderInputs, c)) {
                    return true;
                }
            }

            return false;
        }

        static bool usesVisualizeCoverageCycle(uint blenderInputs, uint cycleIndex) {
            const bool secondCycle = (cycleIndex > 0);
            const InputPM M = decodeInputM(blenderInputs, secondCycle);
            const InputA A = decodeInputA(blenderInputs, secondCycle);
            const InputB B = decodeInputB(blenderInputs, secondCycle);
            return (A == A_ZERO) && (M == PM_BLEND_COLOR) && (B == B_FRAMEBUFFER_ALPHA);
        }

        static bool usesVisualizeCoverageCycle(const OtherMode otherMode) {
            const uint cycles = blendCycleCount(otherMode);
            const uint blenderInputs = otherMode.blenderInputs();
            for (uint c = 0; c < cycles; c++) {
                if (usesVisualizeCoverageCycle(blenderInputs, c)) {
                    return true;
                }
            }

            return false;
        }

#ifdef HLSL_CPU
        struct EmulationRequirements {
            struct Cycle {
                // If either input is zero, overflow isn't possible. The cycle is merely a passthrough of whatever input isn't zero.
                // Even in the case one of the inputs is the framebuffer in the first cycle, it merely replaces whatever is on the second cycle.
                bool passthrough;

                // Numerator overflow is possible if the cycle doesn't use 1MA in the second input. These require overflow
                // and normalization emulation, which is only impossible to emulate natively if at least one of the inputs is the framebuffer color.
                bool numeratorOverflow;

                // If this is a multi-cycle blender and the framebuffer is used in the first cycle, simple emulation can't be used.
                // However, there may be cases where the second cycle doesn't act in a way that makes it impossible to emulate natively.
                bool framebufferColor;
            };

            Cycle cycles[2];
            bool simpleEmulation;
            Approximation approximateEmulation;
        };

        static EmulationRequirements checkEmulationRequirements(const OtherMode otherMode) {
            EmulationRequirements reqs = { };

            // Check the cycles for the emulation requirements.
            const uint blenderInputs = otherMode.blenderInputs();
            const uint blenderCycleCount = Blender::blendCycleCount(otherMode);
            for (uint c = 0; c < blenderCycleCount; c++) {
                const bool secondCycle = (c > 0);
                const InputPM P = decodeInputP(blenderInputs, secondCycle);
                const InputPM M = decodeInputM(blenderInputs, secondCycle);
                const InputA A = decodeInputA(blenderInputs, secondCycle);
                const InputB B = decodeInputB(blenderInputs, secondCycle);
                const bool anyInputIsZero = (A == A_ZERO) || (B == B_ZERO);
                const bool duplicateInput1MA = (P == M) && (B == B_ONE_MINUS_A);
                if (anyInputIsZero || duplicateInput1MA) {
                    reqs.cycles[c].passthrough = true;
                }
                else if (B != B_ONE_MINUS_A) {
                    reqs.cycles[c].numeratorOverflow = true;
                }

                if ((P == PM_FRAMEBUFFER_COLOR) || (M == PM_FRAMEBUFFER_COLOR)) {
                    reqs.cycles[c].framebufferColor = true;
                }
            }

            // Assume by default simple emulation is possible.
            reqs.simpleEmulation = true;

            // First cycle relies on numerator overflow and uses the framebuffer color.
            if (reqs.cycles[0].numeratorOverflow && reqs.cycles[0].framebufferColor) {
                reqs.simpleEmulation = false;
            }
            // Check for two-cycle cases.
            else if (blenderCycleCount == 2) {
                // First cycle uses framebuffer color and it's not a simple passthrough.
                if (reqs.cycles[0].framebufferColor && !reqs.cycles[0].passthrough) {
                    reqs.simpleEmulation = false;
                }
                // Second cycle relies on numerator overflow and uses the framebuffer color.
                else if (reqs.cycles[1].numeratorOverflow && reqs.cycles[1].framebufferColor) {
                    reqs.simpleEmulation = false;
                }
            }

            // Search for approximations if simple emulation isn't capable.
            if (!reqs.simpleEmulation) {
                if (blenderCycleCount == 2) {
                    const InputPM P0 = decodeInputP(blenderInputs, false);
                    const InputPM M0 = decodeInputM(blenderInputs, false);
                    const InputA A0 = decodeInputA(blenderInputs, false);
                    const InputB B0 = decodeInputB(blenderInputs, false);
                    const InputPM P1 = decodeInputP(blenderInputs, true);
                    const InputPM M1 = decodeInputM(blenderInputs, true);
                    const InputA A1 = decodeInputA(blenderInputs, true);
                    const InputB B1 = decodeInputB(blenderInputs, true);
                    if ((P0 == PM_CC_OR_BLENDER) && (M0 == PM_FRAMEBUFFER_COLOR) && (A0 == A_CC_ALPHA) && (B0 == B_ONE_MINUS_A) &&
                        (P1 == PM_CC_OR_BLENDER) && (M1 == PM_FRAMEBUFFER_COLOR) && (A1 == A_CC_ALPHA) && (B1 == B_ONE_MINUS_A)) 
                    {
                        reqs.approximateEmulation = Approximation::CombinerFramebuffer1MA_SquareMix;
                    }
                    else if ((P0 != PM_FRAMEBUFFER_COLOR) && (M0 == PM_FRAMEBUFFER_COLOR) && (B0 == B_ONE_MINUS_A) &&
                             (P1 == PM_CC_OR_BLENDER) && (M1 == PM_FRAMEBUFFER_COLOR) && (B1 == B_ONE_MINUS_A))
                    {
                        reqs.approximateEmulation = Approximation::AnyFramebuffer1MA_MultiplyMix;
                    }
                    else {
                        reqs.approximateEmulation = Approximation::None;
                    }
                }
            }

            return reqs;
        }

        static std::string inputToString(const InputPM i) {
            switch (i) {
            case PM_CC_OR_BLENDER:
                return "CC_OR_BLENDER";
            case PM_FRAMEBUFFER_COLOR:
                return "FRAMEBUFFER_COLOR";
            case PM_BLEND_COLOR:
                return "BLEND_COLOR";
            case PM_FOG_COLOR:
                return "FOG_COLOR";
            default:
                return "Unknown";
            }
        }

        static std::string inputToString(const InputA i) {
            switch (i) {
            case A_CC_ALPHA:
                return "CC_ALPHA";
            case A_FOG_ALPHA:
                return "FOG_ALPHA";
            case A_SHADE_ALPHA:
                return "SHADE_ALPHA";
            case A_ZERO:
                return "ZERO";
            default:
                return "Unknown";
            }
        }

        static std::string inputToString(const InputB i) {
            switch (i) {
            case B_ONE_MINUS_A:
                return "ONE_MINUS_A";
            case B_FRAMEBUFFER_ALPHA:
                return "FRAMEBUFFER_ALPHA";
            case B_ONE:
                return "ONE";
            case B_ZERO:
                return "ZERO";
            default:
                return "Unknown";
            }
        }
#else
        struct Inputs {
            float4 blendColor;
            float4 fogColor;
            float shadeAlpha;
        };

        static float3 fromInputPM(InputPM inputPM, Inputs inputs, float3 ccOrBlender) {
            switch (inputPM) {
            case PM_CC_OR_BLENDER:
                return ccOrBlender;
            case PM_BLEND_COLOR:
                return inputs.blendColor.rgb;
            case PM_FOG_COLOR:
                return inputs.fogColor.rgb;
            default:
                return float3(1.0f, 1.0f, 1.0f);
            }
        }

        static float fromInputA(InputA inputA, Inputs inputs, float combinerAlpha) {
            switch (inputA) {
            case A_CC_ALPHA:
                return combinerAlpha;
            case A_FOG_ALPHA:
                return inputs.fogColor.a;
            case A_SHADE_ALPHA:
                return inputs.shadeAlpha;
            case A_ZERO:
            default:
                return 0.0f;
            }
        }

        static float fromInputB(InputB inputB, float aMultiplier) {
            switch (inputB) {
            case B_ONE_MINUS_A:
                return (1.0f - aMultiplier);
                // Coverage is not emulated. We intentionally return full coverage whenever it's requested.
            case B_FRAMEBUFFER_ALPHA:
                return 1.0f;
            case B_ONE:
                return 1.0f;
            case B_ZERO:
            default:
                return 0.0f;
            }
        }

        static void runCycle(Inputs inputs, bool forceBlend, bool lastCycle, bool notFirstCycle, float4 combinerColor,
            InputPM P, InputPM M, InputA A, InputB B, inout bool passthroughEnabled, inout InputPM passthroughInput,
            inout float3 blenderColor, inout float finalAlpha)
        {
            // Output color is just the P input in this case.
            const bool replaceCcWithBlender = notFirstCycle && !passthroughEnabled;
            if (lastCycle && !forceBlend) {
                if (P == PM_FRAMEBUFFER_COLOR) {
                    finalAlpha = 0.0f;
                }
                else {
                    const float3 inputColor = replaceCcWithBlender ? blenderColor : combinerColor.rgb;
                    blenderColor = fromInputPM(P, inputs, inputColor);
                    finalAlpha = 1.0f;
                }

                return;
            }

            const bool anyInputIsZero = (A == A_ZERO) || (B == B_ZERO);
            const bool duplicateInput1MA = (P == M) && (B == B_ONE_MINUS_A);
            const bool passthrough = anyInputIsZero || duplicateInput1MA;
            const bool numeratorOverflow = !passthrough && (B != B_ONE_MINUS_A);
            bool framebufferColor = (P == PM_FRAMEBUFFER_COLOR) || (M == PM_FRAMEBUFFER_COLOR);
            if (passthroughEnabled) {
                if (P == PM_CC_OR_BLENDER) {
                    P = passthroughInput;
                    framebufferColor |= (passthroughInput == PM_FRAMEBUFFER_COLOR);
                }
                else if (M == PM_CC_OR_BLENDER) {
                    M = passthroughInput;
                    framebufferColor |= (passthroughInput == PM_FRAMEBUFFER_COLOR);
                }
            }

            if (passthrough) {
                if (!lastCycle) {
                    passthroughInput = (A == A_ZERO) ? M : P;
                    passthroughEnabled = true;
                }
                else if (framebufferColor) {
                    finalAlpha = 0.0f;
                }
                else {
                    const float3 inputColor = replaceCcWithBlender ? blenderColor : combinerColor.rgb;
                    blenderColor = fromInputPM((A == A_ZERO) ? M : P, inputs, inputColor.rgb);
                    finalAlpha = 1.0f;
                }
            }
            else if (framebufferColor) {
                const float3 inputColor = replaceCcWithBlender ? blenderColor : combinerColor.rgb;
                if (P == PM_FRAMEBUFFER_COLOR) {
                    blenderColor = fromInputPM(M, inputs, inputColor);
                    finalAlpha = (1.0f - fromInputA(A, inputs, combinerColor.a));
                }
                else if (M == PM_FRAMEBUFFER_COLOR) {
                    blenderColor = fromInputPM(P, inputs, inputColor);
                    finalAlpha = fromInputA(A, inputs, combinerColor.a);
                }
                else {
                    // Should be unreachable.
                }
            }
            else {
                const float3 inputColor = replaceCcWithBlender ? blenderColor : combinerColor.rgb;
                float aMultiplier = fromInputA(A, inputs, combinerColor.a);
                float bMultiplier = fromInputB(B, aMultiplier);

                // Simulate the overflow by using fmod.
                const float Overflow = 1.0f + 8.0f / 255.0f;
                float3 numerator = fmod(fromInputPM(P, inputs, inputColor.rgb) * aMultiplier + fromInputPM(M, inputs, inputColor.rgb) * bMultiplier, Overflow);
                blenderColor = numerator / max(aMultiplier + bMultiplier, 1.0f / 255.0f);
                finalAlpha = 1.0f;
            }
        }

        static float4 run(const OtherMode otherMode, const RenderFlags renderFlags, const Inputs inputs, const float4 combinerColor, const bool overrideFog) {
            // Blender is bypassed in copy mode.
            const uint cycleType = otherMode.cycleType();
            if (cycleType == G_CYC_COPY) {
                return combinerColor;
            }

            // Decode all inputs.
            const uint blenderInputs = otherMode.blenderInputs();
            InputPM P0 = decodeInputP(blenderInputs, false);
            InputPM M0 = decodeInputM(blenderInputs, false);
            InputA A0 = decodeInputA(blenderInputs, false);
            InputB B0 = decodeInputB(blenderInputs, false);
            InputPM P1 = decodeInputP(blenderInputs, true);
            InputPM M1 = decodeInputM(blenderInputs, true);
            InputA A1 = decodeInputA(blenderInputs, true);
            InputB B1 = decodeInputB(blenderInputs, true);

            // Run blender approximations if they were specified.
            Approximation blenderApproximation = (Approximation)(renderBlenderApproximation(renderFlags));
            if (blenderApproximation != Approximation::None) {
                switch (blenderApproximation) {
                case Approximation::CombinerFramebuffer1MA_SquareMix:
                    return float4(combinerColor.rgb, pow(combinerColor.a, 2.0f));
                case Approximation::AnyFramebuffer1MA_MultiplyMix:
                    return float4(fromInputPM(P0, inputs, combinerColor.rgb), fromInputA(A0, inputs, combinerColor.a) * fromInputA(A1, inputs, combinerColor.a));
                default:
                    return float4(0.0f, 0.0f, 0.0f, 0.0f);
                }
            }

            const uint combinerCycles = combineCycleCount(otherMode);
            const uint blendCycles = blendCycleCount(otherMode);
            if (combinerCycles == 0) {
                return float4(combinerColor.rgb, 1.0f);
            }

            // Custom override of fog inputs by render flags if specified.
            if (overrideFog && (blendCycles >= 1) && usesStandardFogCycle(blenderInputs, 0)) {
                A0 = A_ZERO;
            }

            if (overrideFog && (blendCycles >= 2) && usesStandardFogCycle(blenderInputs, 1)) {
                A1 = A_ZERO;
            }

            float3 blenderColor = float3(0.0f, 0.0f, 0.0f);
            float finalAlpha = 0.0f;
            bool passthroughEnabled = false;
            const bool forceBlend = otherMode.forceBlend();
            InputPM passthroughInput;
            runCycle(
                inputs, forceBlend, (combinerCycles == 1), false, combinerColor, P0, M0, A0, B0,
                passthroughEnabled, passthroughInput, blenderColor, finalAlpha);


            if (combinerCycles > 1) {
                runCycle(
                    inputs, forceBlend, true, true, combinerColor, P1, M1, A1, B1,
                    passthroughEnabled, passthroughInput, blenderColor, finalAlpha);
            }

            return float4(blenderColor, finalAlpha);
        }
#endif
    };
#ifdef HLSL_CPU
};
#endif