#include "Text.hlsli"
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

SamplerState gSampler : register(s0);

#include "Bindless.hlsli"



// 3つの値の中央値を求める関数 (MSDFの基本数式)
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV Transform (通常はテクスチャ全体を使うが、アトラスから一部を切り取るUVはコンポーネント側で頂点UVとして計算する)
    float32_t4 uvw = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t2 uv = uvw.xy;

    // アトラステクスチャからRGB(MSDF)をサンプリング
    float32_t3 sampleColor = gTextures[gMaterial.textureIndex].Sample(gSampler, uv).rgb;

    // アトラス解像度を動的取得 (ゼロ除算ガード)
    float2 texSize;
    gTextures[gMaterial.textureIndex].GetDimensions(texSize.x, texSize.y);
    texSize = max(texSize, float2(1.0f, 1.0f));

    // MSDF生成時の距離場スプレッド (FontManager::PX_RANGE = 4.0)
    const float pxRange = 4.0f;
    float2 unitRange = float2(pxRange, pxRange) / texSize;
    float2 screenTexSize = 1.0f / max(fwidth(uv), float2(1e-5f, 1e-5f));
    float screenPxRange = max(0.5f * dot(unitRange, screenTexSize), 1.0f);

    // 距離場の中央値から画面ピクセル空間の距離を計算
    float sd = median(sampleColor.r, sampleColor.g, sampleColor.b);
    float screenPxDistance = screenPxRange * (sd - 0.5f);

    // 画面ピクセル空間で正確に1ピクセルのアンチエイリアス境界を生成
    float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);

    // マテリアルの色と頂点カラー(input.color)を合成
    float32_t4 baseColor = gMaterial.color * input.color;

    // 文字の色にMSDFで求めたアルファ値を乗算する
    output.color = float32_t4(baseColor.rgb, baseColor.a * opacity);

    return output;
}
