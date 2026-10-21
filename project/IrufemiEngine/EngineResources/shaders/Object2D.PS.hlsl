// - αカットアウト(閾値0.5)のみ
// - ライティング関連は削除

#include "Object2D.hlsli" // 2D用の入出力定義へと変更
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

#include "Bindless.hlsli"
#include "Noise.hlsli"

SamplerState gSampler : register(s0);

// 絹のような光の筋（フィラメント・波頭）を生成する Ridged Multifractal ノイズ（CosmicNebula準拠）
float ridgedNoiseVeil(float2 p) {
    float f = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        float n = noise(p);
        float r = 1.0 - abs(n * 2.0 - 1.0); // 鋭い光の峰（リッジ）
        r = r * r;
        f += a * r;
        p *= 2.15;
        a *= 0.5;
    }
    return f;
}

// 重力水面膜（Gravitational Veil Surface）内部視点プロシージャル計算
// タイトル星雲（CosmicNebula）の世界観を厳格に継承し、美しく透き通る靄が空間で優美に揺らめく表現
float4 RenderGravitationalVeil(float2 uv, float time)
{
    // アスペクト比補正（16:9 基準）
    float2 p = (uv - 0.5f) * float2(1.77778f, 1.0f);

    // タイトル星雲（CosmicNebula）完全準拠のカラーパレット
    const float3 colDeepSpace = float3(0.002f, 0.004f, 0.008f); // 漆黒の深宇宙
    const float3 colTeal      = float3(0.025f, 0.350f, 0.380f); // 落ち着いたディープティール
    const float3 colCyan      = float3(0.100f, 0.740f, 0.860f); // 鮮烈なエレクトリックシアン
    const float3 colWhite     = float3(0.850f, 0.950f, 1.000f); // 最鋭のハイライト光条
    const float3 colAmethyst  = float3(0.180f, 0.070f, 0.280f); // 神秘的な淡いアメジスト

    float3 finalColor = colDeepSpace;

    // --- 大スケールの緩やかな揺らめき（Sway: 一直線スクロールを排除し、風や水流でたゆたう動き） ---
    float swayX = sin(p.y * 1.4f + time * 0.45f) * 0.10f + cos(time * 0.25f) * 0.06f;
    float swayY = cos(p.x * 1.2f - time * 0.35f) * 0.08f + sin(time * 0.20f) * 0.05f;
    float2 pSway = p + float2(swayX, swayY);

    // --- 1. 遠景レイヤー（奥深くでゆったりと輝く淡い星雲とアメジストのニュアンス） ---
    float tFar = time * 0.08f;
    float2 pFar = pSway * 0.65f + float2(sin(tFar * 0.6f) * 0.12f, cos(tFar * 0.4f) * 0.08f);
    float nFar1 = noise(pFar * 1.2f + float2(tFar * 0.2f, 0.0f));
    float nFar2 = noise(pFar * 2.0f - float2(0.0f, tFar * 0.15f));
    float farGas = smoothstep(0.20f, 0.72f, nFar1 * 0.6f + nFar2 * 0.4f);
    float purpleMix = saturate(sin(pFar.x * 0.8f - pFar.y * 0.6f + tFar * 0.5f) * 0.5f + 0.5f);
    float3 farColor = lerp(colTeal * 0.6f, colAmethyst * 0.8f, purpleMix);
    finalColor += farColor * (farGas * 0.75f);

    // --- 2. 中景レイヤー（波打つ主星雲ヴェール ＆ 絹のような光条フィラメント） ---
    float tMid = time * 0.12f;
    float2 flowMid = float2(
        noise(pSway * 0.85f + float2(sin(tMid * 0.8f) * 0.15f, cos(tMid * 0.6f) * 0.12f)),
        noise(pSway * 0.85f + float2(-cos(tMid * 0.7f) * 0.14f, sin(tMid * 0.5f) * 0.15f) + 14.7f)
    );
    float2 warpMid = pSway + (flowMid - 0.5f) * 0.35f;

    float fMid1 = ridgedNoiseVeil(warpMid * 1.10f + float2(sin(tMid * 0.5f) * 0.10f, cos(tMid * 0.4f) * 0.08f));
    float fMid2 = ridgedNoiseVeil(warpMid * 2.20f - float2(cos(tMid * 0.4f) * 0.08f, fMid1 * 0.40f));
    float filamentMid = pow(saturate(fMid1 * 0.65f + fMid2 * 0.45f), 2.2f);

    // 生きた光条シマー（尾根を走るパルス）
    float shimmer = sin(warpMid.y * 5.0f - time * 1.2f + fMid1 * 3.2f) * 0.20f + 0.85f;
    // 2本の波打つ星雲帯が交差し、画面全体に優美なヴェールのたなびきを形成
    float band1 = sin(warpMid.x * 0.9f + warpMid.y * 0.7f + tMid * 0.6f) * 0.5f + 0.5f;
    float band2 = sin(warpMid.x * -0.7f + warpMid.y * 0.9f - tMid * 0.5f) * 0.5f + 0.5f;
    float gasMid = smoothstep(0.20f, 0.75f, filamentMid * 0.60f + (band1 * 0.6f + band2 * 0.4f) * 0.40f);

    finalColor += colTeal * (gasMid * 0.85f);
    finalColor += colCyan * (pow(filamentMid, 2.8f) * 1.10f * shimmer);
    finalColor += colWhite * (pow(filamentMid, 5.0f) * 1.25f * shimmer);

    // --- 3. 近景レイヤー（視界をかすめる細く透明なシルク光条リボン） ---
    float tNear = time * 0.18f;
    float2 pNear = pSway * 1.50f + float2(cos(tNear * 0.5f) * 0.15f, sin(tNear * 0.4f) * 0.12f);
    float fNear = ridgedNoiseVeil(pNear * 1.35f + float2(0.0f, -tNear * 0.15f));
    float ribbon = pow(saturate(fNear - 0.18f) / 0.82f, 3.2f);
    finalColor += colCyan * (ribbon * 0.50f);
    finalColor += colWhite * (pow(ribbon, 2.5f) * 0.70f);

    // --- 4. 周辺減光（Vignette）で外周を深淵に沈め、中央とUIの品格を両立 ---
    float vignette = 1.0f - smoothstep(0.65f, 1.60f, length(p));
    finalColor *= vignette;
    return float4(finalColor, 1.0f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;

    // UV 変換
	float32_t4 uvw = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t2 uv = uvw.xy;

    // customEffectType == 10: 重力水面膜（Gravitational Veil Surface）プロシージャル描画
    if (gMaterial.customEffectType == 10)
    {
        output.color = RenderGravitationalVeil(uv, gMaterial.customEffectParam);
        return output;
    }

    // hasTexture 時のみサンプル(帯域節約)
	float32_t4 texColor = (gMaterial.hasTexture != 0)
        ? gTextures[gMaterial.textureIndex].Sample(gSampler, uv)
        : float32_t4(1.0f, 1.0f, 1.0f, 1.0f);

    // ベースカラー
	float32_t4 baseColor = texColor * gMaterial.color * input.color;

	output.color = baseColor;
	return output;
}