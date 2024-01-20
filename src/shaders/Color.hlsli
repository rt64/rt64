//
// RT64
//

// Sourced from https://www.shadertoy.com/view/4dKcWK

static const float EPS = 1e-10;

float3 HUEtoRGB(in float hue) {
    // Hue [0..1] to RGB [0..1]
    // See http://www.chilliant.com/rgb2hsv.html
    float3 rgb = abs(hue * 6. - float3(3, 2, 4)) * float3(1, -1, -1) + float3(-1, 2, 2);
    return clamp(rgb, 0., 1.);
}

float3 RGBtoHCV(in float3 rgb) {
    // RGB [0..1] to Hue-Chroma-Value [0..1]
    // Based on work by Sam Hocevar and Emil Persson
    float4 p = (rgb.g < rgb.b) ? float4(rgb.bg, -1., 2. / 3.) : float4(rgb.gb, 0., -1. / 3.);
    float4 q = (rgb.r < p.x) ? float4(p.xyw, rgb.r) : float4(rgb.r, p.yzx);
    float c = q.x - min(q.w, q.y);
    float h = abs((q.w - q.y) / (6. * c + EPS) + q.z);
    return float3(h, c, q.x);
}

float3 HSLtoRGB(in float3 hsl) {
    // Hue-Saturation-Lightness [0..1] to RGB [0..1]
    float3 rgb = HUEtoRGB(hsl.x);
    float c = (1. - abs(2. * hsl.z - 1.)) * hsl.y;
    return (rgb - 0.5) * c + hsl.z;
}

float3 RGBtoHSL(in float3 rgb) {
    // RGB [0..1] to Hue-Saturation-Lightness [0..1]
    float3 hcv = RGBtoHCV(rgb);
    float z = hcv.z - hcv.y * 0.5;
    float s = hcv.y / (1. - abs(z * 2. - 1.) + EPS);
    return float3(hcv.x, s, z);
}

float3 ModRGBWithHSL(in float3 rgb, in float3 hslMod) {
    return saturate(HSLtoRGB(RGBtoHSL(rgb) + hslMod));
}

#define STRONG_GREEN_LUMINANCE

// Taken from https://64.github.io/tonemapping/
float RGBtoLuminance(float3 rgb) {
#ifdef STRONG_GREEN_LUMINANCE
    // ITU BT.601
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
#else
    // ITU BT.709
    return dot(rgb, float3(0.2127f, 0.7152f, 0.0722f));
#endif
}

float3 LinearToSrgb(in float3 lin) {
    return pow(lin, 1. / 2.2);
}

float3 SrgbToLinear(in float3 srgb) {
    return pow(srgb.rgb, 2.2);
}

float4 LinearToSrgb(in float4 lin) {
    return float4(LinearToSrgb(lin.rgb), lin.a);
}

float4 SrgbToLinear(in float4 srgb) {
    return float4(SrgbToLinear(srgb.rgb), srgb.a);
}