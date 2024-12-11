//
// RT64
//

struct CustomStruct {
    float3 point3D;
    float2 size2D;
};

struct Constants {
    float Value;
};

[[vk::push_constant]] ConstantBuffer<Constants> gConstants : register(b0);
RWBuffer<float> gOutput : register(u1);
RWStructuredBuffer<CustomStruct> gStructured : register(u2);
RWByteAddressBuffer gByteAddress : register(u3);

[numthreads(1, 1, 1)]
void CSMain(uint coord : SV_DispatchThreadID) {
    // Test formatted buffer (read/write)
    float sqrtValue = sqrt(gConstants.Value);
    gOutput[0] = sqrtValue;

    // Test structured buffer (reading and writing)
    CustomStruct data = gStructured[2];
    float3 scaledPoint = data.point3D * data.size2D.xyx;
    gOutput[1] = scaledPoint.x + scaledPoint.y + scaledPoint.z;
    
    // Write back modified data
    data.point3D *= 2.0;
    data.size2D += 1.0;
    gStructured[2] = data;

    // Test byte address buffer (reading and writing)
    float rawValue = asfloat(gByteAddress.Load(16));
    gOutput[2] = rawValue;
    
    // Write incremented value back
    gByteAddress.Store(16, asuint(rawValue + 1.0));
}