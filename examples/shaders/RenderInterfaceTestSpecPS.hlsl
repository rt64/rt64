
[[vk::constant_id(0)]] const uint useRed = 0;

float4 PSMain() : SV_TARGET {
    return useRed ? float4(1.0, 0.0, 0.0, 1.0)   // Red if true
                  : float4(0.0, 1.0, 0.0, 1.0);   // Green if false
}