/**
 * @file FluidVorticity.CS.hlsl
 * @brief 2D 流体シミュレーション: 渦度計算と渦度閉じ込め (Vorticity Confinement)
 */
#include "FluidCommon.hlsli"

Texture2D<float2>   gInVelocity    : register(t0);
RWTexture2D<float2> gOutVelocity   : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)gResolution.x || id.y >= (uint)gResolution.y) return;
    int2 c = id.xy;

    int w = (int)gResolution.x;
    int h = (int)gResolution.y;

    float vL = gInVelocity[int2(max(0, c.x - 1), c.y)].y;
    float vR = gInVelocity[int2(min(w - 1, c.x + 1), c.y)].y;
    float uB = gInVelocity[int2(c.x, max(0, c.y - 1))].x;
    float uT = gInVelocity[int2(c.x, min(h - 1, c.y + 1))].x;

    // Curl (渦度): ∂v/∂x - ∂u/∂y
    float curl = 0.5 * ((vR - vL) - (uT - uB));

    float cL = abs(curl);
    float cR = abs(curl);
    float cB = abs(curl);
    float cT = abs(curl);

    float2 gradCurl = 0.5 * float2(cR - cL, cT - cB);
    float len = length(gradCurl);
    if (len > 0.0001) {
        float2 N = gradCurl / len;
        float2 force = float2(N.y, -N.x) * curl * gVorticityScale * gDeltaTime;
        gOutVelocity[c] = gInVelocity[c] + force;
    } else {
        gOutVelocity[c] = gInVelocity[c];
    }
}
