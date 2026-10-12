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

// ふんわりとした雲塊のボリューム（Billowy Cumulus）
float billowNoise(float2 p) {
    float f = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        float n = noise(p);
        float b = 1.0 - abs(n * 2.0 - 1.0);
        f += a * b;
        p *= 2.05;
        a *= 0.5;
    }
    return f;
}

// ふんわりとした雲塊の高度場（Height Field / Billowy Cumulus ＆ 生きた大気の対流）
float EvaluateCloudHeight(float2 p, float breathTime) {
    // 独立した2つの乱流フロー（Dual Turbulent Flow）で雲が実際にうねり・対流する
    float2 flow1 = p * 1.5 + float2(sin(breathTime * 0.5) * 0.35, -breathTime * 0.18);
    float2 flow2 = p * 3.2 + float2(-cos(breathTime * 0.6) * 0.30, breathTime * 0.15);

    float b1 = billowNoise(flow1);
    float b2 = billowNoise(flow2);

    // 主ガスのコブに沿って歪みながら流動する光条フィラメント
    float2 flowF = p * 2.2 + float2(cos(breathTime * 0.4) * 0.25, -breathTime * 0.22) + b1 * 0.25;
    float f1 = ridgedNoise(flowF);

    // 雲の大きな波がゆったりと押し寄せる立体的うねり（Swell）
    float swell = sin(p.x * 2.0 + breathTime * 1.4) * 0.12;
    float h = b1 * 0.50 + b2 * 0.25 + f1 * 0.25 + swell;
    return smoothstep(0.14, 0.84, h);
}

float4 main(VertexOutput input) : SV_Target {
    float3 surfacePos = input.worldPosition;
    float3 cameraPos = gPerFrame.cameraWorldPosition;
    float3 toCam = cameraPos - surfacePos;
    float distToSurface = length(toCam);
    float3 viewDir = normalize(toCam); // サーフェスからカメラへの視線ベクトル
    float3 rayDir = normalize(surfacePos - cameraPos); // カメラからサーフェスへの入射視線

    // タイトル (CosmicNebula.PS.hlsl) のカラーパレットを完全継承
    float3 colDeepSpace = gNebulaSea.colDeepSpace.rgb;  // 深宇宙の漆黒（深淵）
    float3 colTeal      = gNebulaSea.colTeal.rgb;       // 澄んだディープティール
    float3 colCyan      = gNebulaSea.colCyan.rgb;       // 鮮烈なエレクトリックシアン
    float3 colWhite     = gNebulaSea.colWhite.rgb;      // 純白ハイライト光条
    float3 colAmethyst  = gNebulaSea.colAmethyst.rgb;   // 幻想的なアメジストパープル
    float3 horizonSky   = gNebulaSea.colHorizonSky.rgb;  // 地平線スカイドーム色

    // --- 1. 時間・移流パラメータの設計（Decima/UE5アーキテクチャ準拠） ---
    // 自機前進速度に埋もれない、力強い対流サイクル
    float breathTime = gNebulaSea.time * 0.75;
    // 奥の重力特異点（+Z）に向かって星雲が吸い込まれる基底重力移流
    float inflowTime = gNebulaSea.time * 0.15;

    // --- 2. 視差遮蔽マッピング（POM / Parallax Occlusion Mapping） ---
    // メッシュ上面は Y=0.0f（環境物の足元）に厳格固定したまま、内部への視差を計算
    float2 baseUV = surfacePos.xz * 0.0045;
    // 自機進行方向（+Z）と同調する奥への重力流入
    baseUV.y += -inflowTime * 0.20;

    float2 viewParallaxDir = rayDir.xz / max(-rayDir.y, 0.18);

    // 視差サンプリング
    float h0 = EvaluateCloudHeight(baseUV, breathTime);
    float2 parallaxUV = baseUV - viewParallaxDir * ((1.0 - h0) * 0.012);
    float cloudHeight = EvaluateCloudHeight(parallaxUV, breathTime);

    // --- 3. 雲塊の立体ライティング ＆ 生きた光条シマー ---
    float cloudPeak = pow(cloudHeight, 2.2);

    // 局所勾配による微小ハイライト
    float eps = 0.005;
    float hR = EvaluateCloudHeight(parallaxUV + float2(eps, 0.0), breathTime);
    float hU = EvaluateCloudHeight(parallaxUV + float2(0.0, eps), breathTime);
    float3 cloudNormal = normalize(float3((cloudHeight - hR) * 4.0, 1.0, (cloudHeight - hU) * 4.0));
    float NdotL = saturate(dot(cloudNormal, normalize(float3(0.0, 0.65, 0.76))));

    // 光条フィラメントの生きたシマー（光が波のように尾根を高速に駆け抜けるパルス）
    float shimmer = sin(surfacePos.z * 0.020 - breathTime * 3.0 + cloudHeight * 4.5) * 0.35 + 0.85;

    // --- 4. 4本目準拠の最高純度色彩（アメジスト × ティール × 漆黒 × シアン） ---
    // 有機的に交錯するパープル＆ティールマーブル（対流時間で緩やかに変色）
    float purpleBias = saturate(sin(surfacePos.x * 0.0020 + surfacePos.z * 0.0010 + breathTime * 0.25) * 0.5 + 0.5);
    float3 baseGasColor = lerp(colTeal, colAmethyst, purpleBias * 0.65);

    // 谷底は深宇宙の漆黒（深淵）へしっかり沈み、雲の盛り上がりに向かって主ガスが鮮やかに発光
    float3 finalColor = lerp(colDeepSpace, baseGasColor, cloudHeight);

    // 雲の頭頂部・波頭でのエレクトリックシアン ＆ 純白ハイライト（距離減衰を付与し、遠方の過剰飽和・白飛びを完全防止）
    float distFade = 1.0 - smoothstep(150.0, 1200.0, distToSurface);
    float peakGlow = pow(cloudPeak, 2.2) * shimmer;
    float edgeHighlight = pow(cloudPeak, 5.0) * pow(NdotL, 2.5) * shimmer;
    finalColor += colCyan * (peakGlow * 0.50 * (0.35 + 0.65 * distFade));
    finalColor += colWhite * (edgeHighlight * 0.45 * distFade);

    // --- 5. 3D空間フレネル大気散乱 ＆ 遠方大気フォグ同調 ---
    // スカイドーム大気（ラベンダー）と呼応するオーロラ・ヴェールの揺らぎ
    float auroraWave = sin(surfacePos.x * 0.0015 + breathTime * 0.45) * 0.15 + 0.85;
    float NdotV = saturate(dot(float3(0, 1, 0), viewDir));
    float fresnel = pow(1.0 - NdotV, 4.0);
    finalColor += lerp(colTeal, colAmethyst, purpleBias) * (fresnel * 0.28 * auroraWave);

    // 地平線奥のスカイドームへふんわり溶け込ませる
    float fogT = saturate((distToSurface - gNebulaSea.fadeStartDistance) /
                          max(gNebulaSea.fadeEndDistance - gNebulaSea.fadeStartDistance, 1.0));
    finalColor = lerp(finalColor, horizonSky, smoothstep(0.0, 1.0, fogT));

    // 完全不透明（1.0f）で出力
    return float4(finalColor, 1.0f);
}
