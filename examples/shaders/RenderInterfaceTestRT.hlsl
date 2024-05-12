//
// RT64
//

struct Attributes {
    float2 bary;
};

struct Payload {
    float4 color;
};

struct BufferParams {
    float3 rgbMultiplier;
    uint3 pad;
};

RaytracingAccelerationStructure gBVH : register(t0);
RWTexture2D<float4> gOutput : register(u1);
StructuredBuffer<BufferParams> gBufferParams : register(t2);

[shader("raygeneration")]
void ColorRayGen() {
    RayDesc ray;
    ray.Origin = float3(float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy), 0.0f);
    ray.Direction = float3(0.0f, 0.0f, 1.0f);
    ray.TMin = 1e-6f;
    ray.TMax = 1e6f;
    
    Payload payload;
    payload.color = float4(0.0f, 0.5f, 0.0f, 1.0f);
    TraceRay(gBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    gOutput[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void ColorClosestHit(inout Payload payload, in Attributes attr) {
    payload.color = float4(1.0f, attr.bary.xy, 1.0f) * float4(gBufferParams[0].rgbMultiplier, 1.0f);
}

[shader("miss")]
void ColorMiss(inout Payload payload) {
    payload.color = float4(0.25f, 0.25f, 0.5f, 1.0f) * float4(gBufferParams[1].rgbMultiplier, 1.0f);
}