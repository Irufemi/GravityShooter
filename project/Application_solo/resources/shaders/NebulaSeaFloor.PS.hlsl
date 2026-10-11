/**
 * @file NebulaSeaFloor.PS.hlsl
 * @brief 星雲ガス海（Nebula Sea Floor）3Dワールド空間 プロシージャル・ボリュメトリック ピクセルシェーダー
 * @details タイトル星雲 (CosmicNebula) の5色グラデーション（漆黒・ティール・シアン・白・アメジスト）を継承し、
 *          ワールドXZデュアル・フロー、3D Beer-Lambert 則による体積光吸収、柱とのソフト交差 (Depth Fade)、
 *          およびスカイドーム大気フォグ同調を完全な3D空間で計算します。
 */

#include "PerFrame.hlsli"
#include "GravityWarp.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct NebulaSeaParams {
    float time;
    float railSpeed;
    float surfaceY;
    float volumeDepth;
    float fadeStartDistance;
    float fadeEndDistance;
    float softIntersectionRange;
    float densityScale;
    float4 colDeepSpace;
    float4 colTeal;
    float4 colCyan;
    float4 colWhite;
    float4 colAmethyst;
    float4 colHorizonSky;
    float4 cameraSnapXZ;
};
ConstantBuffer<NebulaSeaParams> gNebulaSea : register(b6);

struct VertexOutput {
    float4 svPosition : SV_Position;
    float3 worldPosition : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 viewRay : VIEW_RAY;
};

// 絹のような光の筋（フィラメント・波頭）を生成する Ridged Multifractal ノイズ
float ridgedNoise(float2 p) {
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

// 雲海のボリューム密度をワールドXZ座標とサンプリング高度から計算
float EvaluateNebulaDensity(float2 xz, float sampleY, float time) {
    // レイヤー1: レール進行方向（奥から手前へ高速フロー）
    float2 flow1 = xz * 0.0035 + float2(0.0, -time * gNebulaSea.railSpeed * 0.015);
    // レイヤー2: 局所的な緩やかな乱流・うねり
    float2 flow2 = xz * 0.0080 + float2(time * 0.035, time * 0.020);

    float n1 = fBm(flow1);
    float n2 = ridgedNoise(flow2);

    float combinedNoise = n1 * 0.55 + n2 * 0.45;

    // Y=0.0f を上面とし、下層（volumeDepth）に向かって密度を変化させるプロファイル
    float depthT = saturate((gNebulaSea.surfaceY - sampleY) / max(gNebulaSea.volumeDepth, 1.0));
    float heightFade = smoothstep(0.0, 0.25, depthT) * (1.0 - smoothstep(0.85, 1.0, depthT));

    return saturate(combinedNoise * heightFade * gNebulaSea.densityScale);
}

float4 main(VertexOutput input) : SV_Target {
    float3 surfacePos = input.worldPosition;
    float3 cameraPos = gPerFrame.cameraWorldPosition;
    float3 rayDir = normalize(surfacePos - cameraPos);
    float distToSurface = length(surfacePos - cameraPos);

    // タイトル (CosmicNebula.PS.hlsl) のカラーパレットを完全継承
    float3 colDeepSpace = gNebulaSea.colDeepSpace.rgb;  // 深宇宙の漆黒
    float3 colTeal      = gNebulaSea.colTeal.rgb;       // エメラルド・ティール
    float3 colCyan      = gNebulaSea.colCyan.rgb;       // エレクトリックシアン
    float3 colWhite     = gNebulaSea.colWhite.rgb;      // 純白ハイライト光
    float3 colAmethyst  = gNebulaSea.colAmethyst.rgb;   // アメジストパープル
    float3 colDeepAbyss = colDeepSpace * 0.65;          // 最深部の静寂インディゴ

    float time = gNebulaSea.time * 0.35;

    // --- 3D ワールド空間 ボリュメトリック光吸収・散乱積分 (Beer-Lambert Model) ---
    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    // 視線が上面(Y=0)から内部へ潜り込む厚み（ステップサンプリング）
    const int kStepCount = 8;
    float maxMarchDepth = min(gNebulaSea.volumeDepth, 35.0);
    float stepDepth = maxMarchDepth / (float)kStepCount;

    // 視線レイが下向きに進む場合の垂直ステップ
    float rayDownRatio = max(-rayDir.y, 0.15); // 水平に近いグレージング角でも破綻しないようガード
    float sampleDistStep = stepDepth / rayDownRatio;

    for (int i = 0; i < kStepCount; ++i) {
        float currentDist = (float)i * sampleDistStep;
        float3 samplePos = surfacePos + rayDir * currentDist;

        if (samplePos.y > gNebulaSea.surfaceY) {
            continue;
        }

        float density = EvaluateNebulaDensity(samplePos.xz, samplePos.y, time);

        if (density > 0.01) {
            // 有機的なエメラルド〜パープル・シアンのグラデーション
            float purpleBias = saturate(sin(samplePos.x * 0.005 + time * 0.2) * 0.5 + 0.5);
            float3 baseGasColor = lerp(colTeal, colAmethyst, purpleBias * 0.45);

            // リッジピークでのシアン・白熱発光
            float3 litColor = baseGasColor;
            litColor += colCyan * (pow(density, 2.0) * 1.50);
            litColor += colWhite * (pow(density, 4.0) * 2.20);
            litColor += colCyan * (saturate(density - 0.40) * 0.12);

            // Beer-Lambert 則による吸収・散乱
            float stepAbsorb = exp(-density * 0.40 * stepDepth);
            float stepTrans = (1.0 - stepAbsorb);

            // 蓄積
            accumulatedColor += litColor * density * stepTrans * transmittance * 1.40;
            transmittance *= stepAbsorb;

            if (transmittance < 0.02) {
                break; // 遮蔽完了
            }
        }
    }

    // 雲の最深部（深みのある落ち着いたブルーグレーインディゴ）
    accumulatedColor += colDeepAbyss * transmittance;

    // --- 遠方大気フォグ同調（Atmospheric Fog Blending） ---
    // 地平線奥はスカイドームのラベンダー色の空へふんわりと霞んで溶け込ませる
    float fogT = saturate((distToSurface - gNebulaSea.fadeStartDistance) /
                          max(gNebulaSea.fadeEndDistance - gNebulaSea.fadeStartDistance, 1.0));
    float3 horizonSkyColor = gNebulaSea.colHorizonSky.rgb;
    accumulatedColor = lerp(accumulatedColor, horizonSkyColor, smoothstep(0.0, 1.0, fogT));

    // 完全不透明（1.0f）で出力
    return float4(accumulatedColor, 1.0f);
}
