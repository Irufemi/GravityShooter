/**
 * @file FluidJacobi.CS.hlsl
 * @brief 2D 流体シミュレーション: 圧力ポアソン反復 (Jacobi: ∇²p = ∇・u)
 */
#include "FluidCommon.hlsli"

Texture2D<float>   gInPressure   : register(t0);
Texture2D<float>   gInDivergence : register(t1);
RWTexture2D<float> gOutPressure  : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)gResolution.x || id.y >= (uint)gResolution.y) return;
    int2 c = id.xy;
    int w = (int)gResolution.x;
    int h = (int)gResolution.y;

    float pL = gInPressure[int2(max(0, c.x - 1), c.y)];
    float pR = gInPressure[int2(min(w - 1, c.x + 1), c.y)];
    float pB = gInPressure[int2(c.x, max(0, c.y - 1))];
    float pT = gInPressure[int2(c.x, min(h - 1, c.y + 1))];
    float div = gInDivergence[c];

    // Jacobi反復: p = (pL + pR + pB + pT - div) / 4
    gOutPressure[c] = (pL + pR + pB + pT - div) * 0.25;
}
