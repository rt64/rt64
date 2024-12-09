//
// RT64
//

struct Constants {
    float Value;
};

[[vk::push_constant]] ConstantBuffer<Constants> gConstants : register(b0);
RWBuffer<float> gOutput : register(u1);

[numthreads(1, 1, 1)]
void CSMain(uint coord : SV_DispatchThreadID) {
    gOutput[0] = sqrt(gConstants.Value);
}