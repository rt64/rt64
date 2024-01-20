//
// RT64
//
// Luminance Histogram (clear pass) by Alex Tardif
// From http://www.alextardif.com/HistogramLuminance.html
//

#define HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION 8

RWByteAddressBuffer LuminanceHistogram : register(u0);

[numthreads(HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION, HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID) {
    LuminanceHistogram.Store(threadId.x * threadId.y, 0);
}