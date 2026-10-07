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
    float4 mouseUV;       //!< マウスカーソル (xy: マウス正規化UV, z: スピード/強度, w: 予備)
    float4 trailPoints[8]; //!< 過去のマウス軌跡点 (xy: UV, z: 強度・生存率 [0-1], w: 予備)
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

    // --- 2. 曲線トレイル流体場（Curved Trail Wake & Dispersion Field） ---
    // 手で水面を切ったように、マウスが通った実際の曲線の軌跡に沿って星雲ガスが左右に割れ、尾を引く
    float2 gasFluidWisp = float2(0.0, 0.0); // 煙・インクがほどける流体たなびきオフセット
    float gasCoreGap = 0.0;                 // 指先が通った跡の煙の割れ（中心がほどけて薄まる）
    float gasCrestVeil = 0.0;               // 両脇にふわっとたなびく煙のヴェール
    float gasExcitation = 0.0;              // 撫でられたガスの淡いオーロラ励起光

    // rotP 空間におけるトレイル点の座標と重みを準備
    float2 trailP[8];
    float trailWeight[8];
    [unroll]
    for (int t = 0; t < 8; ++t) {
        float2 tUV = gNebula.trailPoints[t].xy;
        float2 pRaw = float2((tUV.x - center.x) * aspect, tUV.y - center.y);
        trailP[t] = float2(
            pRaw.x * 0.96 - pRaw.y * 0.26,
            pRaw.x * 0.26 + pRaw.y * 0.96
        );
        trailWeight[t] = saturate(gNebula.trailPoints[t].z);
    }

    // 各軌跡線分（P[i] -> P[i+1]）を評価し、曲線の引き波を合成
    [unroll]
    for (int i = 0; i < 7; ++i) {
        float wA = trailWeight[i];
        float wB = trailWeight[i + 1];
        if (wA <= 0.005 && wB <= 0.005) {
            continue;
        }

        float2 pA = trailP[i];
        float2 pB = trailP[i + 1];
        float2 segVec = pB - pA;
        float segLen = length(segVec);

        if (segLen < 0.0005) {
            // 静止時の点周辺の微細な緩衝
            float d = length(rotP - pA);
            float core = exp(-pow(d / 0.06, 2.0)) * wA * 0.35;
            gasCoreGap = max(gasCoreGap, core);
            continue;
        }

        float2 dir = segVec / segLen;
        float2 side = float2(-dir.y, dir.x);

        // 線分への正射影パラメータ
        float2 toP = rotP - pA;
        float proj = dot(toP, dir);
        float tClamped = saturate(proj / segLen);
        float2 closestP = pA + dir * (tClamped * segLen);

        float2 delta = rotP - closestP;
        float dist = length(delta);
        float sideDist = dot(delta, side);

        // 過去の軌跡ほど自然に幅が広がり、手元ほどシャープに切れ込む
        float progress = (float)i / 7.0; // 0: 手元(最新), 1: 尾の先端(最古)
        float width = 0.055 + progress * 0.035;
        float normSide = sideDist / width;

        // 線分上の生存強度（線形補間）
        float segLife = lerp(wA, wB, tClamped);

        // 線分両端の外側への滑らかな減衰
        float endDist = 0.0;
        if (proj < 0.0) {
            endDist = -proj;
        } else if (proj > segLen) {
            endDist = proj - segLen;
        }
        float endFade = exp(-pow(endDist / width, 2.0));

        // 手で水面を切ったときの山型引き波プロファイル
        float wakeProfile = exp(-normSide * normSide * 1.5) * segLife * endFade;

        // 1. 水が左右に柔らかく押し分けられる流体変位
        float sidePush = normSide * exp(-normSide * normSide * 1.2);
        gasFluidWisp += side * (sidePush * 0.09 * wakeProfile);

        // 2. 指先が切った中心線がスーッと透き通る（抜けの尾）
        float coreGap = exp(-normSide * normSide * 3.2) * (wakeProfile * 0.65);
        gasCoreGap = max(gasCoreGap, coreGap);

        // 3. 水面を切った跡にキラキラと尾を引く光条ヴェール
        float crestVeil = abs(normSide) * exp(-normSide * normSide * 1.4) * (wakeProfile * 0.70);
        gasCrestVeil = max(gasCrestVeil, crestVeil);

        // 4. 撫でられたガスの淡いオーロラ励起光
        gasExcitation = max(gasExcitation, wakeProfile * 0.25);
    }

    // --- 3. 巨大な重力リング（Accretion Nebula Ring）と「中央の深淵（Void）」 ---
    // 空間座標 rotP 自体は歪ませず、クリーンで自然な深宇宙の幾何学を維持
    float dist = length(float2(rotP.x, rotP.y * 1.25));

    // 参考画像準拠: 中央（dist < 0.28）は漆黒の深淵、半径0.48付近に光の環が展開
    float ringMask = exp(-pow(dist - 0.48, 2.0) * 14.0);
    float outerGlow = exp(-pow(dist - 0.72, 2.0) * 8.0) * 0.45;
    float totalRing = ringMask + outerGlow;

    // 事象の地平面（Event Horizon）近傍の微細な境界リング光
    float innerEdge = exp(-pow(dist - 0.28, 2.0) * 55.0) * 0.35;

    // --- 4. 2D回転行列による連続渦巻き ---
    // ★ 【重要】回転する前の画面座標（rotP）に変位を適用！
    // 自転している swirlP に画面ベクトルを足すと直線上でくるくる回転してしまうため、
    // 変位をかけた画面座標 displacedP を回転行列に通すことで、直線上での自転巻き込みを完全防止
    float2 displacedP = rotP - gasFluidWisp;
    float displacedDist = length(float2(displacedP.x, displacedP.y * 1.25));

    // 内周ほど吸い込まれ、全体として心地よい流動感をもたらす回転速度
    float swirlSpeed = 0.38 + 0.18 / (displacedDist + 0.20);
    float swirlAmount = 3.6 * log(displacedDist + 0.15) - time * swirlSpeed;
    float s = sin(swirlAmount);
    float c = cos(swirlAmount);
    float2 swirlP = float2(
        displacedP.x * c - displacedP.y * s,
        displacedP.x * s + displacedP.y * c
    );

    // ★ 星雲の美しい渦巻きフィラメント
    float f1 = ridgedNoise(swirlP * 2.6 + float2(time * 0.12, 0.0));
    float f2 = ridgedNoise(swirlP * 5.0 - float2(time * 0.08, f1 * 1.2));
    float filament = pow(saturate(f1 * 0.65 + f2 * 0.50), 1.9);

    // ★ 手が水を切った部分の自然な透き通し
    if (gasCoreGap > 0.001) {
        filament *= (1.0 - gasCoreGap * 0.65);
    }

    // ★ 水面を切った跡に長く尾を引く光の筋（光条ヴェール）
    filament += gasCrestVeil * 0.45;

    // 微細な煙・ヴェールの揺らぎ ＋ 励起光
    float wisps = noise(float2(rotP.x * 4.0 + time * 0.08, rotP.y * 5.0 - time * 0.06));
    filament = filament * (0.75 + 0.35 * wisps) + gasExcitation * 0.20;

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
    gasBody = gasBody * (1.0 - gasCoreGap * 0.70);
    gasBody = smoothstep(0.08, 0.95, gasBody);

    float3 finalColor = colDeepSpace;
    finalColor += baseGasColor * (gasBody * 1.15);
    finalColor += colCyan * (pow(filament * ringMask, 2.2) * 1.65);
    finalColor += colWhite * (pow(filament * ringMask, 4.2) * 2.80);
    finalColor += colCyan * innerEdge;

    // --- 7. 星屑・コズミックダスト（深宇宙全体に充満する多重浮遊粉塵・星間チリ） ---
    // A. 最奥の静止星空（深宇宙の基準面・立体感の奥行き対比用）
    float2 starUV1 = uv * 320.0;
    float2 cell1 = floor(starUV1);
    float rnd1 = rand(cell1);
    if (rnd1 > 0.988) {
        float2 frac1 = frac(starUV1) - 0.5;
        float starGlow = exp(-length(frac1) * 24.0) * (rnd1 - 0.988) * 55.0;
        float twinkle = sin(time * 4.0 + rnd1 * 50.0) * 0.30 + 0.70;
        finalColor += float3(0.80, 0.90, 1.0) * starGlow * twinkle * 0.60;
    }

    // B. 星雲の重力渦牽引ベクトル（Swirl Pull Vector）
    float2 swirlOffset = swirlP - rotP;
    // 内周ほど引力が強く、外周ほど自由に漂う牽引勾配
    float coreDrag = clamp(0.18 + 0.45 / (dist + 0.32), 0.18, 0.70);

    // C. 空間全体を照らす星雲光の環境照明度（Ambient Nebula Illumination）
    // 参考画像準拠: ダストは画面全体に存在するが、星雲ガスに近い場所ほど光に照らされて鮮やかに浮き彫りになる
    float dustIllum = saturate(gasBody * 1.3 + filament * 0.8 + 0.28);

    // -------------------------------------------------------------------------
    // レイヤー1: 【超微細パウダーダスト（Micro Dust & Space Powder）】
    // 画面全体・四隅に至るまで空間を満たす無数の粉末状チリ。星雲ガスのたなびきに乗ってフワッと流れる
    // -------------------------------------------------------------------------
    float2 ambientDriftPowder = float2(time * 0.035, -time * 0.020);
    float2 powderP = rotP + ambientDriftPowder + swirlOffset * (coreDrag * 0.38) - gasFluidWisp * 1.0;
    float2 powderUV = powderP * 170.0;
    float2 powderCell = floor(powderUV);
    float powderRnd = rand(powderCell);
    // 高密度: 約28%のセルに微細な粉塵が存在
    if (powderRnd > 0.72) {
        // セル内でランダムに位置をオフセット（格子状の均一感を完全に破壊）
        float2 pOff = float2(rand(powderCell + 1.2), rand(powderCell + 7.4)) * 0.70 + 0.15;
        float2 pFrac = frac(powderUV) - pOff;
        float pDist = length(pFrac);
        float pGlow = exp(-pDist * 34.0) * (powderRnd - 0.72) * 28.0;
        float pTwinkle = sin(time * 5.0 + powderRnd * 90.0) * 0.25 + 0.75;
        // マウスがガスを撫でた瞬間、微細な粉がふわっと光る（割れの中心は抜け、縁で煌めく）
        float pExcitation = (1.0 - gasCoreGap * 0.50) * (1.0 + gasExcitation * 4.5);
        float3 pCol = lerp(float3(0.55, 0.75, 0.85), colCyan, dustIllum);
        finalColor += pCol * (pGlow * dustIllum * pTwinkle * pExcitation);
    }

    // -------------------------------------------------------------------------
    // レイヤー2: 【中粒浮遊コズミックダスト（Floating Cosmic Grains）】
    // 参考画像準拠: 大小様々なランダムな粒径を持ち、星雲の渦とたなびきの流れに沿って漂うチリ
    // -------------------------------------------------------------------------
    float2 ambientDriftGrain = float2(time * 0.025, -time * 0.015);
    float2 grainP = rotP + ambientDriftGrain + swirlOffset * (coreDrag * 0.45) - gasFluidWisp * 1.15;
    float2 grainUV = grainP * 85.0;
    float2 grainCell = floor(grainUV);
    float grainRnd = rand(grainCell + float2(19.2, 53.8));
    // 中密度: 約16%のセルに浮遊ダストが存在
    if (grainRnd > 0.84) {
        float2 gOff = float2(rand(grainCell + 3.9), rand(grainCell + 8.1)) * 0.75 + 0.12;
        float2 gFrac = frac(grainUV) - gOff;
        float gDist = length(gFrac);
        // ランダムなサイズ・輝度のばらつき（大小のチリが混ざり合う）
        float gSize = lerp(18.0, 30.0, rand(grainCell + 11.5));
        float gGlow = exp(-gDist * gSize) * (grainRnd - 0.84) * 48.0;
        float gTwinkle = sin(time * 6.5 + grainRnd * 70.0) * 0.35 + 0.65;
        // ガスの撫でられ励起
        float gExcitation = (1.0 - gasCoreGap * 0.50) * (1.0 + gasExcitation * 5.0);
        float3 gCol = lerp(colTeal, colCyan, grainRnd * 0.9) * 1.4;
        finalColor += gCol * (gGlow * dustIllum * gTwinkle * gExcitation);
    }

    // -------------------------------------------------------------------------
    // レイヤー3: 【鮮烈な浮遊結晶・ジュエル（Heavy Floating Stardust Crystals）】
    // 参考画像準拠: 時折キラリと輝く大きめの星屑結晶。質量が重く、ガスからあまり引っ張られずにゆったり漂う
    // -------------------------------------------------------------------------
    float2 ambientDriftJewel = float2(-time * 0.018, -time * 0.028);
    float2 jewelP = rotP + ambientDriftJewel + swirlOffset * (coreDrag * 0.20) - gasFluidWisp * 0.70;
    float2 jewelUV = jewelP * 34.0;
    float2 jewelCell = floor(jewelUV);
    float jewelRnd = rand(jewelCell + float2(33.7, 77.1));
    if (jewelRnd > 0.975) {
        float2 jOff = float2(rand(jewelCell + 5.1), rand(jewelCell + 9.3)) * 0.60 + 0.20;
        float2 jewelFrac = frac(jewelUV) - jOff;
        float jDist = length(jewelFrac);
        float coreGlow = exp(-jDist * 16.0) * (jewelRnd - 0.975) * 80.0;
        float outerHalo = exp(-jDist * 6.0) * (jewelRnd - 0.975) * 22.0;
        // 十字スパイク（光条）
        float spike = exp(-abs(jewelFrac.x) * 24.0) * exp(-abs(jewelFrac.y) * 4.0)
                    + exp(-abs(jewelFrac.y) * 24.0) * exp(-abs(jewelFrac.x) * 4.0);
        float jTwinkle = sin(time * 5.0 + jewelRnd * 65.0) * 0.40 + 0.60;
        // ガスの撫でられ励起
        float cutSpike = 1.0 + gasExcitation * 4.0;
        float3 jewelCol = lerp(colCyan, colWhite, saturate((jewelRnd - 0.975) * 50.0));
        finalColor += (jewelCol * (coreGlow + outerHalo * 0.6) + colWhite * spike * 0.40) * (jTwinkle * cutSpike);
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
