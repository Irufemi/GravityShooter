/**
 * @file CosmicNebula.VS.hlsl
 * @brief 最奥深度 (Z=1.0) 全画面三角形頂点シェーダー
 * @details 3Dオブジェクト（自機・ガレキ）の描画ピクセルを上書きせず、
 *          背景空間のみに神秘的な星雲を描画するための最奥投影シェーダーです。
 */

#include "Fullscreen.hlsli"

static const uint kNumVertex = 3;
static const float4 kPositions[kNumVertex] = {
    {-1.0f,  1.0f, 1.0f, 1.0f}, // 左上
    { 3.0f,  1.0f, 1.0f, 1.0f}, // 右上
    {-1.0f, -3.0f, 1.0f, 1.0f}, // 左下
};
static const float2 kTexcoords[kNumVertex] = {
    {0.0f, 0.0f},
    {2.0f, 0.0f},
    {0.0f, 2.0f},
};

VertexShaderOutput main(uint vertexId : SV_VertexID) {
    VertexShaderOutput output;
    output.position = kPositions[vertexId];
    output.texcoord = kTexcoords[vertexId];
    return output;
}
