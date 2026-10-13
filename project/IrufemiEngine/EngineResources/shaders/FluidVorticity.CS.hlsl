/**
 * @file FluidVorticity.CS.hlsl
 * @brief 2D 流体シミュレーション: 渦度計算と渦度閉じ込め (Vorticity Confinement)
 */
#include "FluidCommon.hlsli"

Texture2D<float2>   gInVelocity    : register(t0);
RWTexture2D<float2> gOutVelocity   : register(u0);

float ComputeCurl(int2 pos, int w, int h) {
    float vL = gInVelocity[int2(max(0, pos.x - 1), pos.y)].y;
    float vR = gInVelocity[int2(min(w - 1, pos.x + 1), pos.y)].y;
    float uB = gInVelocity[int2(pos.x, max(0, pos.y - 1))].x;
    float uT = gInVelocity[int2(pos.x, min(h - 1, pos.y + 1))].x;
    return 0.5 * ((vR - vL) - (uT - uB));
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)gResolution.x || id.y >= (uint)gResolution.y) return;
    int2 c = id.xy;

    int w = (int)gResolution.x;
    int h = (int)gResolution.y;

    // 自セルの渦度 (Curl)
    float curl = ComputeCurl(c, w, h);

    // 近傍4セルの渦度絶対値から勾配 (∇|ω|) を算出
    float cL = abs(ComputeCurl(int2(max(0, c.x - 1), c.y), w, h));
    float cR = abs(ComputeCurl(int2(min(w - 1, c.x + 1), c.y), w, h));
    float cB = abs(ComputeCurl(int2(c.x, max(0, c.y - 1)), w, h));
    float cT = abs(ComputeCurl(int2(c.x, min(h - 1, c.y + 1)), w, h));

    float2 gradCurl = 0.5 * float2(cR - cL, cT - cB);
    float len = length(gradCurl);
    if (len > 0.0001) {
        float2 N = gradCurl / len;
        // 渦度閉じ込め力: F_vc = (N × ω) * ε (2Dでは N.y, -N.x)
        float2 force = float2(N.y, -N.x) * curl * gVorticityScale * gDeltaTime;
        gOutVelocity[c] = gInVelocity[c] + force;
    } else {
        gOutVelocity[c] = gInVelocity[c];
    }
}
