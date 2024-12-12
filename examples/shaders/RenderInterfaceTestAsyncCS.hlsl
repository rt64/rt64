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
RWStructuredBuffer<CustomStruct> gStructuredBase : register(u2);
RWStructuredBuffer<CustomStruct> gStructuredOffset : register(u3);
RWByteAddressBuffer gByteAddress : register(u4);

[numthreads(1, 1, 1)]
void CSMain(uint coord : SV_DispatchThreadID) {
    // Test formatted buffer (read/write)
    float sqrtValue = sqrt(gConstants.Value);
    gOutput[0] = sqrtValue;

    // Test structured buffer base view (read/write)
    CustomStruct data = gStructuredBase[0];
    gOutput[1] = data.point3D.x + data.point3D.y + data.point3D.z + 
                 data.size2D.x + data.size2D.y;

    // Increment all values by 1 for next frame
    data.point3D += 1.0;
    data.size2D += 1.0;
    gStructuredBase[0] = data;

    // Test structured buffer offset view
    CustomStruct offsetData = gStructuredOffset[0]; // test will offset view
    gOutput[2] = offsetData.point3D.x + offsetData.point3D.y + offsetData.point3D.z + 
                 offsetData.size2D.x + offsetData.size2D.y;

    // Increment all values by 1 for next frame
    offsetData.point3D += 1.0;
    offsetData.size2D += 1.0;
    gStructuredOffset[0] = offsetData;

    // Test byte address buffer (read/write)
    float rawValue = asfloat(gByteAddress.Load(16));
    gOutput[3] = rawValue;
    
    // Write incremented value back
    gByteAddress.Store(16, asuint(rawValue + 1.0));
}