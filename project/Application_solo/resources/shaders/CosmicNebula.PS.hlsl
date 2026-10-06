/**
 * @file CosmicNebula.PS.hlsl
 * @brief 神秘的な深宇宙星雲（Cosmic Nebula）プロシージャルピクセルシェーダー
 * @details 漆黒の深宇宙に、Ridged Multifractal による透き通るエメラルド〜シアン〜バイオレットの
 *          光の筋（フィラメント）が重力で渦を巻き、マウスカーソルに連動して空間が歪み、星屑が瞬きます。
 */

#include "Fullscreen.hlsli"
#include "PerFrame.hlsli"
#include "Noise.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct CosmicNebulaParams {
    float pulseIntensity; //!< 出撃・決定パルス [0.0 - 1.0]
    float time;           //!< 経過時間
    float swirlStrength;  //!< 渦の回転強度
    float density;        //!< 星雲濃度
    float4 centerUV;      //!< 重力渦の中心 (xy: 追従中心, zw: パララックスオフセット)
    float4 mouseUV;       //!< マウスカーソル (xy: マウス正規化UV, z: インタラクション強度, w: 予備)
};
ConstantBuffer<CosmicNebulaParams> gNebula : register(b6);

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

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    // シネマティックで生きた星雲の流動時間
    float time = gNebula.time * 0.28;

    // --- 1. アスペクト比補正 & 静止した重力中心（タイトルロゴ背後） ---
    const float aspect = 16.0 / 9.0;
    float2 center = gNebula.centerUV.xy; // (0.5, 0.42) に静止
    float2 p = float2((uv.x - center.x) * aspect, uv.y - center.y);

    // わずかに傾いた楕円座標系（シネマティックな重力円盤）
    float2 rotP = float2(
        p.x * 0.96 - p.y * 0.26,
        p.x * 0.26 + p.y * 0.96
    );

    // --- 2. 進行方向・速度ベクトル同期 流体押し分け & 航跡場（Velocity-Aligned Wake & Dispersion Field） ---
    // マウスカーソルの移動速度ベクトル (gNebula.mouseUV.zw) を rotP 空間に変換
    float2 mousePosUV = gNebula.mouseUV.xy;
    float2 mouseVelUV = gNebula.mouseUV.zw;

    // ゲーム論理UVから rotP 座標系への変換
    float2 mouseP = float2((mousePosUV.x - center.x) * aspect, mousePosUV.y - center.y);
    float2 mouseRotP = float2(
        mouseP.x * 0.96 - mouseP.y * 0.26,
        mouseP.x * 0.26 + mouseP.y * 0.96
    );

    float2 mouseVel = float2(
        mouseVelUV.x * aspect * 0.96 - mouseVelUV.y * 0.26,
        mouseVelUV.x * aspect * 0.26 + mouseVelUV.y * 0.96
    );

    float mouseSpeed = length(mouseVel);
    float wakeDistort = 0.0;
    float swirlTorque = 0.0; // 背後の渦巻きへ波及する回転トルク変調量

    // --- 空間歪み計算（A. 常時発動する重力レンズ ＋ B. 進行方向・速度同期の流体押し分け＆航跡） ---
    float2 deltaCursor = rotP - mouseRotP;
    float cursorDist = length(deltaCursor);

    // A. 常時発動する重力レンズ屈折（静止時・微動時でもカーソル直下の星雲がプクッと歪み、存在感を主張）
    float lensMask = exp(-pow(cursorDist / 0.28, 2.0));
    float2 lensDisplacement = (deltaCursor / (cursorDist + 0.035)) * (lensMask * 0.075);

    float2 dynamicDisplacement = float2(0.0, 0.0);

    // B. マウス移動時の動的押し分け・航跡場（速度感度・変位量を大幅強化）
    if (mouseSpeed > 0.003) {
        // カーソル周囲（半径約0.32）へ影響範囲を拡大
        float localMask = exp(-pow(cursorDist / 0.32, 2.5));

        float2 moveDir = mouseVel / mouseSpeed;
        float2 sideDir = float2(-moveDir.y, moveDir.x);

        float distFwd = dot(deltaCursor, moveDir);
        float distSide = dot(deltaCursor, sideDir);

        // 先端左右押し分け（完全連続な双極子変位場）
        float normSide = distSide / 0.18;
        float smoothLateralFactor = normSide * exp(-normSide * normSide);
        float forwardDecay = exp(-pow((distFwd - 0.02) / 0.14, 2.0));
        float bowShockPower = forwardDecay * saturate(mouseSpeed * 3.5);
        float2 bowShockPush = sideDir * (smoothLateralFactor * bowShockPower * 0.18);

        // 後方引き波（完全連続な航跡場）
        float rearFactor = smoothstep(0.06, -0.06, distFwd);
        float wakeLength = max(0.0, -distFwd);
        float wakeWidth = clamp(0.09 + wakeLength * 0.30, 0.09, 0.18);
        float normWakeSide = distSide / wakeWidth;
        float wakeSideDecay = exp(-normWakeSide * normWakeSide);
        float maxTrailLength = saturate(mouseSpeed * 1.5) * 0.35;
        float wakeLongDecay = exp(-pow(wakeLength / (maxTrailLength + 0.001), 1.5));
        float wakePower = rearFactor * wakeSideDecay * wakeLongDecay * saturate(mouseSpeed * 4.0);

        // 背後の渦流（接線方向）に沿った流送ベクトル
        float2 vortexTangent = float2(-rotP.y, rotP.x);
        float rLen = length(vortexTangent);
        if (rLen > 0.001) {
            vortexTangent /= rLen;
        }

        // 航跡内での流体引きずり
        float2 wakePull = (-moveDir * 0.35 + vortexTangent * 0.40 + sideDir * (normWakeSide * exp(-normWakeSide * normWakeSide) * 0.70)) * (wakePower * 0.14);

        dynamicDisplacement = (bowShockPush + wakePull) * localMask;

        // 中心に対するカーソルの回転角運動量（トルク）を背後の渦の位相（Swirl）に波及
        float crossTorque = (mouseRotP.x * mouseVel.y - mouseRotP.y * mouseVel.x);
        swirlTorque = (bowShockPower * 0.6 + wakePower * 1.8) * crossTorque * 0.70 * localMask;
    }

    // 重力レンズ変位と動的流体変位を合成適用
    float2 totalDisplacement = lensDisplacement + dynamicDisplacement;
    rotP -= totalDisplacement;
    wakeDistort = length(totalDisplacement) * 22.0;

    // --- 3. 巨大な重力リング（Accretion Nebula Ring）と「中央の深淵（Void）」 ---
    float dist = length(float2(rotP.x, rotP.y * 1.25));

    // 参考画像準拠: 中央（dist < 0.28）は漆黒の深淵、半径0.48付近に光の環が展開
    float ringMask = exp(-pow(dist - 0.48, 2.0) * 14.0);
    float outerGlow = exp(-pow(dist - 0.72, 2.0) * 8.0) * 0.45;
    float totalRing = ringMask + outerGlow;

    // 事象の地平面（Event Horizon）近傍の微細な境界リング光
    float innerEdge = exp(-pow(dist - 0.28, 2.0) * 55.0) * 0.35;

    // --- 4. 2D回転行列による連続渦巻き（背後の渦への波及・トルク適用） ---
    // atan2の角度不連続(±π)を一切使わず、連続な cos/sin 回転行列で空間をねじる
    // 内周ほど吸い込まれ、全体として心地よい流動感をもたらす回転速度
    float swirlSpeed = 0.38 + 0.18 / (dist + 0.20);
    float swirlAmount = 3.6 * log(dist + 0.15) - time * swirlSpeed + swirlTorque;
    float s = sin(swirlAmount);
    float c = cos(swirlAmount);
    float2 swirlP = float2(
        rotP.x * c - rotP.y * s,
        rotP.x * s + rotP.y * c
    );

    // 連続なベクトル空間 swirlP から幾重にも重なるシルクの光条（フィラメント）を生成
    float f1 = ridgedNoise(swirlP * 2.6 + float2(time * 0.12, 0.0));
    float f2 = ridgedNoise(swirlP * 5.0 - float2(time * 0.08, f1 * 1.2));
    float filament = pow(saturate(f1 * 0.65 + f2 * 0.50), 1.9);

    // 航跡・押し分けによるフィラメントガスの自然なヨレとちぎれ（Fluid Filament Dispersion）
    if (wakeDistort > 0.001) {
        float tearNoise = ridgedNoise(swirlP * 6.5 + float2(dist * 3.0, 0.0));
        float tearFactor = smoothstep(0.25, 0.80, tearNoise) * saturate(wakeDistort);
        filament = lerp(filament, filament * (1.0 - tearFactor * 0.50) + tearFactor * 0.20, saturate(wakeDistort * 0.8));
    }

    // 微細な煙・ヴェールの揺らぎ
    float wisps = noise(float2(rotP.x * 4.0 + time * 0.08, rotP.y * 5.0 - time * 0.06));
    filament *= (0.75 + 0.35 * wisps);

    // --- 5. 参考画像完全準拠のカラーパレット ---
    // ベース: 完全に締まった深宇宙の漆黒（中央の深淵と外周奥）
    float3 colDeepSpace = float3(0.002, 0.004, 0.008);

    // 落ち着いたディープティール（星雲の主ガス）
    float3 colTeal      = float3(0.03, 0.40, 0.44);
    // 鮮烈なエレクトリックシアン / ターコイズ（光条コア）
    float3 colCyan      = float3(0.12, 0.78, 0.88);
    // 最鋭のハイライト光（純白光の糸）
    float3 colWhite     = float3(0.85, 0.96, 1.0);
    // 参考画像にある微かな淡いアメジストパープル
    float3 colAmethyst  = float3(0.22, 0.08, 0.32);

    // 左下・右上に微かに香るパープルグラデーション（連続関数）
    float purpleBias = (rotP.x * -0.5 + rotP.y * 0.5) + 0.5;
    purpleBias = saturate(purpleBias);
    float3 baseGasColor = lerp(colTeal, colAmethyst, purpleBias * 0.55);

    // --- 6. 発光加算合成（Emissive Accumulation） ---
    float gasBody = totalRing * (0.28 + 0.85 * filament);
    gasBody = smoothstep(0.08, 0.95, gasBody);

    float3 finalColor = colDeepSpace;
    finalColor += baseGasColor * (gasBody * 1.15);
    finalColor += colCyan * (pow(filament * ringMask, 2.2) * 1.65);
    finalColor += colWhite * (pow(filament * ringMask, 4.2) * 2.80);
    finalColor += colCyan * innerEdge;

    // --- 7. 星屑の瞬き（深宇宙の静止した星空） ---
    float2 starUV1 = uv * 360.0;
    float2 cell1 = floor(starUV1);
    float rnd1 = rand(cell1);
    if (rnd1 > 0.985) {
        float2 frac1 = frac(starUV1) - 0.5;
        float starGlow = exp(-length(frac1) * 22.0) * (rnd1 - 0.985) * 70.0;
        float twinkle = sin(time * 5.0 + rnd1 * 50.0) * 0.35 + 0.65;
        finalColor += float3(0.85, 0.92, 1.0) * starGlow * twinkle;
    }

    float2 starUV2 = uv * 140.0;
    float2 cell2 = floor(starUV2);
    float rnd2 = rand(cell2 + float2(12.3, 45.6));
    if (rnd2 > 0.994) {
        float2 frac2 = frac(starUV2) - 0.5;
        float starDist = length(frac2);
        float starGlow = exp(-starDist * 14.0) * (rnd2 - 0.994) * 160.0;
        float twinkle = sin(time * 7.0 + rnd2 * 70.0) * 0.45 + 0.55;
        float spike = exp(-abs(frac2.x) * 24.0) * exp(-abs(frac2.y) * 3.5)
                    + exp(-abs(frac2.y) * 24.0) * exp(-abs(frac2.x) * 3.5);
        finalColor += (float3(0.90, 0.96, 1.0) * starGlow + colCyan * spike * 0.35) * twinkle;
    }

    // --- 8. 出撃決定時の重力パルス光（Launch Pulse Glow） ---
    if (gNebula.pulseIntensity > 0.001) {
        float pulseRing = abs(dist - gNebula.pulseIntensity * 2.2);
        float pulseGlow = exp(-pulseRing * 14.0) * (1.0 - gNebula.pulseIntensity) * 3.5;
        finalColor += colCyan * pulseGlow;
        float centerFlash = exp(-dist * 3.2) * (1.0 - gNebula.pulseIntensity) * 2.5;
        finalColor += colWhite * centerFlash;
    }

    // 周辺減光（Vignette）
    float vignette = 1.0 - smoothstep(0.60, 1.65, length(p));
    finalColor *= vignette;

    output.color = float4(finalColor, 1.0);
    return output;
}
