/**
 * @file FluidDivergence.CS.hlsl
 * @brief 2D 流体シミュレーション: 発散計算 (Divergence: ∇・u)
 */
#include "FluidCommon.hlsli"

Texture2D<float2>   gInVelocity    : register(t0);
RWTexture2D<float>  gOutDivergence : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)gResolution.x || id.y >= (uint)gResolution.y) return;
    int2 c = id.xy;
    int w = (int)gResolution.x;
    int h = (int)gResolution.y;

    float uL = gInVelocity[int2(max(0, c.x - 1), c.y)].x;
    float uR = gInVelocity[int2(min(w - 1, c.x + 1), c.y)].x;
    float vB = gInVelocity[int2(c.x, max(0, c.y - 1))].y;
    float vT = gInVelocity[int2(c.x, min(h - 1, c.y + 1))].y;

    gOutDivergence[c] = 0.5 * ((uR - uL) + (vT - vB));
}
