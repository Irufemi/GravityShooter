/**
 * @file GravitationalWave.PS.hlsl
 * @brief タイトル画面用 プロシージャル重力波＆高電圧プラズマ放電ピクセルシェーダー
 * @details ShaderToyライクなSDF、空間重力レンズ歪み、時空断線時の高電圧プラズマ放電アーク、
 *          同心円波紋干渉をGPUプロシージャル計算し、エンジンのBloomパスへHDR出力します。
 */

#include "Fullscreen.hlsli"
#include "PerFrame.hlsli"
#include "Noise.hlsli"

// カメラ/フレーム情報 (register b2 / RootSlot::Camera)
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

// 重力波＆プラズマ放電パラメータ (register b6 / RootSlot::Special)
struct GravityWaveParams {
    float2 mousePos;          //!< マウス/カーソルUV座標 [0.0 - 1.0]
    float2 mouseVelocity;     //!< カーソル移動速度ベクトル
    float  speedThreshold;    //!< 破断（ちぎれ）発生速度しきい値
    float  clickImpulse;      //!< クリック/決定時の衝撃波タイマー [0.0 - 1.0]
    float  gridScale;         //!< グリッド密度 (例: 32.0)
    float  warpStrength;      //!< 空間の歪み強度 (例: 0.12)
    float  tearIntensity;     //!< ちぎれ・破断の持続減衰タイマー [0.0 - 1.0]
    float  pad[1];
};
ConstantBuffer<GravityWaveParams> gWave : register(b6);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float time = gPerFrame.time;

    // アスペクト比補正 (16:9 想定の歪み補正)
    const float aspect = 16.0 / 9.0;
    float2 aspectUV = float2(uv.x * aspect, uv.y);
    float2 aspectMouse = float2(gWave.mousePos.x * aspect, gWave.mousePos.y);

    float2 diff = aspectUV - aspectMouse;
    float dist = length(diff);

    // --- 1. シュワルツシルト風 重力レンズ空間歪み (Gravitational Lensing) ---
    float warp = gWave.warpStrength / (dist * dist + 0.06);
    warp = min(warp, 0.45); // 発散ガード
    float2 warpedUV = uv - (diff / (dist + 0.0001)) * (warp * 0.06);

    // --- 2. 重力衝撃波の伝播 (Gravitational Ripple & Interference) ---
    float rippleGlow = 0.0;
    if (gWave.clickImpulse > 0.001) {
        float rippleRadius = gWave.clickImpulse * 1.6;
        float rippleDist = abs(dist - rippleRadius);
        // 同心円波紋の合成と減衰
        float rippleWave = sin(rippleDist * 38.0 - gWave.clickImpulse * 18.0) * exp(-rippleDist * 7.0);
        warpedUV += (diff / (dist + 0.0001)) * (rippleWave * 0.03 * (1.0 - gWave.clickImpulse));
        // 波紋の光彩リング
        rippleGlow = exp(-rippleDist * 14.0) * (1.0 - gWave.clickImpulse) * 2.2;
    }

    // --- 3. ネオングリッド SDF (SDF Neon Grid) ---
    float2 gridUV = warpedUV * gWave.gridScale;
    float2 cellUV = abs(frac(gridUV) - 0.5);
    float lineDist = min(cellUV.x, cellUV.y);

    // ピクセル単位の完全アンチエイリアス
    float gridLine = 1.0 - smoothstep(0.0, 0.055, lineDist);
    float gridGlow = exp(-lineDist * 18.0) * 0.45;

    // --- 4. 時空破断と高電圧プラズマ放電アーク (Tear & Electric Arc) ---
    float plasmaCore = 0.0;
    float plasmaGlow = 0.0;
    float tearMask = 0.0;

    if (gWave.tearIntensity > 0.01) {
        // カーソル周辺の張力限界領域
        tearMask = smoothstep(0.65, 0.05, dist) * gWave.tearIntensity;

        // グリッド線の断線（Voronoi と fBm による引き裂かれ）
        float tearNoise = fBm(warpedUV * 16.0 + float2(time * 2.0, -time));
        float crack = smoothstep(0.25, 0.55, tearNoise);
        gridLine *= lerp(1.0, crack, tearMask); // 断線部分が途切れる

        // 断面間に走る高電圧プラズマフィラメント (Electric Arc)
        float2 arcUV = warpedUV * 26.0 + float2(time * 32.0, time * 18.0);
        float arcNoise = fBm(arcUV);
        float arcDist = abs(sin(dist * 24.0 + arcNoise * 2.5));

        // 鋭利な超高輝度プラズマ芯 (指数減衰)
        float coreVal = 1.0 / (arcDist * 32.0 + 1.0);
        plasmaCore = pow(coreVal, 3.8) * tearMask;

        // 外周のコロナ放電ハロー
        plasmaGlow = pow(coreVal, 1.4) * tearMask * 0.75;

        // 高周波フリッカー (放電特有の明滅・チラつき)
        float flicker = rand(float2(floor(time * 60.0), 0.5)) * 0.35 + 0.65;
        plasmaCore *= flicker;
        plasmaGlow *= flicker;
    }

    // --- 5. カラー合成 ＆ HDR Bloom 直結 ---
    // 基本色: 自機の洗練されたスマートシアン
    float3 cyanColor = float3(0.0, 0.85, 1.0);
    // 敵・混沌エネルギー: ダークパープル〜エレクトリックマゼンタ
    float3 magentaColor = float3(0.85, 0.1, 1.0);

    // グリッド発光 (HDR 1.5 でBloomを優雅に点灯)
    float3 gridColor = cyanColor * (gridLine * 1.5 + gridGlow);

    // プラズマ放電カラー (ホワイト芯 3.8 HDR + シアン/マゼンタコロナ)
    float3 coronaColor = lerp(cyanColor, magentaColor, fBm(warpedUV * 8.0 + time));
    float3 plasmaTotal = float3(3.8, 3.8, 4.0) * plasmaCore + coronaColor * plasmaGlow * 2.2;

    // 波紋の光彩
    float3 rippleColor = cyanColor * rippleGlow;

    // 背景の微弱な大気深宇宙グラデーション
    float3 bgColor = float3(0.015, 0.02, 0.04) * (1.0 - dist * 0.4);

    // 最終合成
    float3 finalColor = bgColor + gridColor + plasmaTotal + rippleColor;

    // 半透明合成用アルファ (グリッドやプラズマが存在する箇所を抜く)
    float finalAlpha = saturate(gridLine + gridGlow + plasmaCore + plasmaGlow + rippleGlow * 0.5 + 0.15);

    output.color = float4(finalColor, finalAlpha);
    return output;
}
