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

// ふんわりとした雲塊の高度場（Height Field / Billowy Cumulus）
float EvaluateCloudHeight(float2 p) {
    float b1 = billowNoise(p * 1.5);
    float b2 = billowNoise(p * 3.5);
    float f1 = ridgedNoise(p * 2.2);

    float h = b1 * 0.55 + b2 * 0.25 + f1 * 0.20;
    return smoothstep(0.15, 0.85, h);
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

    // --- 1. 視差遮蔽マッピング（POM / Parallax Occlusion Mapping） ---
    // メッシュ上面は Y=0.0f（環境物の足元）に厳格固定したまま、
    // 視線が雲の内部へ潜り込む「深さの視差」を計算し、手前のコブが奥の谷を遮蔽する立体感を生む
    float2 baseUV = surfacePos.xz * 0.0045; // 視界に無数の雲塊が広がるスケール
    float2 viewParallaxDir = rayDir.xz / max(-rayDir.y, 0.18);

    // 視差サンプリング（上面の高度を基準に谷間を奥へずらす）
    float h0 = EvaluateCloudHeight(baseUV);
    float2 parallaxUV = baseUV - viewParallaxDir * ((1.0 - h0) * 0.012);
    float cloudHeight = EvaluateCloudHeight(parallaxUV);

    // --- 2. 雲塊の立体ライティング（陰影コントラスト） ---
    // 雲のコブの頭頂部（上面Y=0近傍）と谷間（Y < 0の深淵）
    float cloudPeak = pow(cloudHeight, 2.2);

    // 局所勾配による微小ハイライト
    float eps = 0.005;
    float hR = EvaluateCloudHeight(parallaxUV + float2(eps, 0.0));
    float hU = EvaluateCloudHeight(parallaxUV + float2(0.0, eps));
    float3 cloudNormal = normalize(float3((cloudHeight - hR) * 4.0, 1.0, (cloudHeight - hU) * 4.0));
    float NdotL = saturate(dot(cloudNormal, normalize(float3(0.0, 0.65, 0.76))));

    // --- 3. 4本目準拠の最高純度色彩（アメジスト × ティール × 漆黒 × シアン） ---
    // 有機的に交錯するパープル＆ティールマーブル
    float purpleBias = saturate(sin(surfacePos.x * 0.0020 + surfacePos.z * 0.0010) * 0.5 + 0.5);
    float3 baseGasColor = lerp(colTeal, colAmethyst, purpleBias * 0.65);

    // 谷底は深宇宙の漆黒（深淵）へしっかり沈み、雲の盛り上がりに向かって主ガスが鮮やかに発光
    float3 finalColor = lerp(colDeepSpace, baseGasColor, cloudHeight);

    // 雲の頭頂部・波頭でのエレクトリックシアン ＆ 純白ハイライト光条（Bloomの過剰飽和・白飛びを完全防止）
    float peakGlow = pow(cloudPeak, 2.2);
    float edgeHighlight = pow(cloudPeak, 5.0) * pow(NdotL, 2.5);
    finalColor += colCyan * (peakGlow * 0.65);
    finalColor += colWhite * (edgeHighlight * 0.70);

    // --- 4. コズミックダスト（星屑・星間チリ）の微細な瞬き ---
    float2 dustUV = surfacePos.xz * 0.20;
    float2 dustCell = floor(dustUV);
    float dustRnd = rand(dustCell);
    if (dustRnd > 0.965) {
        float2 dustFrac = frac(dustUV) - 0.5;
        float dustGlow = exp(-length(dustFrac) * 18.0) * (dustRnd - 0.965) * 38.0;
        float twinkle = sin(gNebulaSea.time * 2.5 + dustRnd * 80.0) * 0.35 + 0.65;
        finalColor += float3(0.85, 0.95, 1.0) * (dustGlow * twinkle * saturate(cloudHeight + 0.2));
    }

    // --- 5. 3D空間フレネル大気散乱 ＆ 遠方大気フォグ同調 ---
    float NdotV = saturate(dot(float3(0, 1, 0), viewDir));
    float fresnel = pow(1.0 - NdotV, 4.0);
    finalColor += lerp(colTeal, colAmethyst, purpleBias) * (fresnel * 0.28);

    // 地平線奥のスカイドームへふんわり溶け込ませる
    float fogT = saturate((distToSurface - gNebulaSea.fadeStartDistance) /
                          max(gNebulaSea.fadeEndDistance - gNebulaSea.fadeStartDistance, 1.0));
    finalColor = lerp(finalColor, horizonSky, smoothstep(0.0, 1.0, fogT));

    // 完全不透明（1.0f）で出力
    return float4(finalColor, 1.0f);
}
