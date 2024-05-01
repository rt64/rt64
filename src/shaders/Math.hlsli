//
// RT64
//

#pragma once

#define EPSILON                             1e-6
#define M_PI                                3.14159265f
#define M_TWO_PI                            (M_PI * 2.0f)

int modulo(int x, int y) {
    if (y != 0) {
        return x - y * (int)(floor((float)(x) / (float)(y)));
    }
    else {
        return x;
    }
}

// Utility function to get a vector perpendicular to an input vector 
// (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}