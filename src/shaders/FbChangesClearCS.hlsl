//
// RT64
//

RWStructuredBuffer<uint> gOutputCount : register(u0);

[numthreads(1, 1, 1)]
void CSMain() {
    gOutputCount[0] = 0;
}