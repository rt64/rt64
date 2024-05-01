//
// RT64
//

#pragma once

#define MAX_HIT_QUERIES 24

uint getHitBufferIndex(uint hitPos, uint2 pixelIdx, uint2 pixelDims) {
    return (hitPos * pixelDims.y + pixelIdx.y) * pixelDims.x + pixelIdx.x;
}