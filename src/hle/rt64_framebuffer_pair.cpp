//
// RT64
//

#include "rt64_framebuffer_pair.h"

#include "shared/rt64_blender.h"

namespace RT64 {
    // FramebufferPair

    void FramebufferPair::reset() {
        fastPaths = {};
        flushReason = FlushReason::None;
        displayListAddress = 0;
        displayListCounter = 0;
        projectionCount = 0;
        projectionStart = 0;
        gameCallCount = 0;
        depthRead = false;
        depthWrite = false;
        syncRequired = false;
        fillRectOnly = true;
        ditherPatterns.fill(0);
        scissorRect.reset();
        startFbDiscards.clear();
        startFbOperations.clear();
        endFbOperations.clear();
        drawColorRect.reset();
        drawDepthRect.reset();
    }

    void FramebufferPair::addGameCall(const GameCall &gameCall) {
        assert(projectionCount > 0);
        auto &proj = projections[projectionCount - 1];
        proj.addGameCall(gameCall);
        depthRead = depthRead || gameCall.callDesc.otherMode.zCmp();
        depthWrite = depthWrite || gameCall.callDesc.otherMode.zUpd();
        fillRectOnly = fillRectOnly && (proj.type == Projection::Type::Rectangle) && (gameCall.callDesc.otherMode.cycleType() == G_CYC_FILL);
        gameCallCount++;

        // Track what type of color dither this call used.
        uint32_t ditherIndex = (gameCall.callDesc.otherMode.rgbDither() >> G_MDSFT_RGBDITHER) & 0x3;
        ditherPatterns[ditherIndex]++;
    }

    bool FramebufferPair::inProjection(uint32_t transformsIndex, Projection::Type type) const {
        if (projectionCount > 0) {
            const Projection &lastProj = projections[projectionCount - 1];
            if ((lastProj.transformsIndex == transformsIndex) && (lastProj.type == type)) {
                return true;
            }
        }

        return false;
    }

    int FramebufferPair::changeProjection(uint32_t transformsIndex, Projection::Type type) {
        adjustVector(projections, ++projectionCount);
        const uint32_t projectionIndex = projectionCount - 1;
        auto &projection = projections[projectionIndex];
        projection.reset();
        projection.transformsIndex = transformsIndex;
        projection.type = type;
        return projectionIndex;
    }

    bool FramebufferPair::isEmpty() const {
        return (gameCallCount == 0) && startFbOperations.empty() && endFbOperations.empty();
    }
    
    bool FramebufferPair::earlyPresentCandidate() const {
        // Some games might use screen framebuffers as temporary output for some operations that are clearly
        // not intended to be presented to the user. This function includes some cases commonly found in games.
        const bool singleGameCall = (gameCallCount == 1);
        
        if (singleGameCall) {
            for (uint32_t p = 0; p < projectionCount; p++) {
                const Projection &proj = projections[p];
                if (proj.gameCallCount == 0) {
                    continue;
                }

                const GameCall &call = proj.gameCalls[0];
                if (proj.type == Projection::Type::Rectangle) {
                    // The rect call covers the entire dimensions of the scissor.
                    bool fullScreenRect = call.callDesc.scissorRect.fullyInside(call.callDesc.rect);

                    // VISCVG: Some games will dump coverage by using a special blender that basically outputs coverage only.
                    // Coverage can be useful for emulating the VI anti-aliasing on the CPU, which is the case on the Zelda games
                    // or games with the same engine (e.g. Animal Forest).
                    // 
                    // It is very unlikely this will get presented to the user, so if the only relevant operation in the
                    // framebuffer pair is a rect operation that does this, it won't be valid for early presentation.
                    if (interop::Blender::usesVisualizeCoverageCycle(call.callDesc.otherMode)) {
                        return false;
                    }
                }
            }
        }

        return true;
    }
};