/**
 * @file FluidProject.CS.hlsl
 * @brief 2D 流体シミュレーション: 圧力勾配減算・非圧縮化投影 (Projection)
 */
#include "FluidCommon.hlsli"

Texture2D<float2>   gInVelocity  : register(t0);
Texture2D<float>    gInPressure  : register(t1);
RWTexture2D<float2> gOutVelocity : register(u0);

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

    float2 gradP = 0.5 * float2(pR - pL, pT - pB);
    gOutVelocity[c] = gInVelocity[c] - gradP;
}
