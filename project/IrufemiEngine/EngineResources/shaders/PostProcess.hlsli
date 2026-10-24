#pragma once
#include "Bindless.hlsli"
#include "PostProcessBindlessParams.hlsli"

// --- ポストプロセス モード定義 (C++の PostProcessMode と一致させる) ---
static const int32_t kPostProcessMode_None = 0;
static const int32_t kPostProcessMode_Grayscale = 1;
static const int32_t kPostProcessMode_Sepia = 2;
static const int32_t kPostProcessMode_Vignette = 3;
static const int32_t kPostProcessMode_Smoothing = 4;
static const int32_t kPostProcessMode_GaussianFilter = 5;
static const int32_t kPostProcessMode_DepthBasedOutline = 6;
static const int32_t kPostProcessMode_RadialBlur = 7;
static const int32_t kPostProcessMode_Dissolve = 8;
static const int32_t kPostProcessMode_Noise = 9;
static const int32_t kPostProcessMode_HSV = 10;
static const int32_t kPostProcessMode_ToneMapping = 11;
static const int32_t kPostProcessMode_Fade = 12;
static const int32_t kPostProcessMode_Slide = 13;
static const int32_t kPostProcessMode_Glitch = 15;
static const int32_t kPostProcessMode_DualKawaseBlur = 16;
static const int32_t kPostProcessMode_LuminanceBasedOutline = 17;
static const int32_t kPostProcessMode_Pixelation = 18;
static const int32_t kPostProcessMode_Pointillism = 19;
static const int32_t kPostProcessMode_Posterization = 20;
static const int32_t kPostProcessMode_NightVision = 21;
static const int32_t kPostProcessMode_Kaleidoscope = 22;
static const int32_t kPostProcessMode_ChromaticAberration = 23;
static const int32_t kPostProcessMode_DisplacementMap = 24;
static const int32_t kPostProcessMode_DirectionalBlur = 25;
static const int32_t kPostProcessMode_Halftone = 26;
static const int32_t kPostProcessMode_DepthOfField = 27;
static const int32_t kPostProcessMode_GravityDistortion = 29;

// --- ヘルパー関数 ---
#include "Noise.hlsli"
#include "SpaceTransforms.hlsli"

// 輝度(Luminance)の取得
float32_t GetLuminance(float32_t3 color) {
    return dot(color, float32_t3(0.299f, 0.587f, 0.114f));
}

// RGB -> HSV
float32_t3 RGBToHSV(float32_t3 rgb) {
    float32_t maxVal = max(rgb.r, max(rgb.g, rgb.b));
    float32_t minVal = min(rgb.r, min(rgb.g, rgb.b));
    float32_t delta = maxVal - minVal;
    float32_t3 hsv = float32_t3(0, 0, maxVal);
    if (delta > 0) {
        if (maxVal == rgb.r) hsv.x = (rgb.g - rgb.b) / delta;
        else if (maxVal == rgb.g) hsv.x = 2 + (rgb.b - rgb.r) / delta;
        else hsv.x = 4 + (rgb.r - rgb.g) / delta;
        hsv.x /= 6.0;
        if (hsv.x < 0) hsv.x += 1.0;
        hsv.y = delta / maxVal;
    }
    return hsv;
}

// HSV -> RGB
float32_t3 HSVToRGB(float32_t3 hsv) {
    float32_t h = hsv.x * 6.0;
    float32_t s = hsv.y;
    float32_t v = hsv.z;
    float32_t C = v * s;
    float32_t X = C * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float32_t m = v - C;
    float32_t3 rgb = float32_t3(0, 0, 0);
    if (h < 1.0) rgb = float32_t3(C, X, 0);
    else if (h < 2.0) rgb = float32_t3(X, C, 0);
    else if (h < 3.0) rgb = float32_t3(0, C, X);
    else if (h < 4.0) rgb = float32_t3(0, X, C);
    else if (h < 5.0) rgb = float32_t3(X, 0, C);
    else rgb = float32_t3(C, 0, X);
    return rgb + m;
}

float32_t WrapValue(float32_t value, float32_t minRange, float32_t maxRange) {
    float32_t range = maxRange - minRange;
    float32_t modValue = fmod(value - minRange, range);
    if (modValue < 0) modValue += range;
    return minRange + modValue;
}

float32_t rand2dTo1d(float2 value) {
    float2 dot_res = dot(value, float2(12.9898, 78.233));
    return frac(sin(dot_res.x) * 43758.5453);
}

// ACES ToneMapping
float32_t3 ACESFilm(float32_t3 x) {
    float32_t a = 2.51f;
    float32_t b = 0.03f;
    float32_t c = 2.43f;
    float32_t d = 0.59f;
    float32_t e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 2次元ガウス関数
float32_t gauss(float32_t x, float32_t y, float32_t sigma) {
    static const float32_t PI = 3.1415926535f;
    float32_t exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float32_t denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

// --- エフェクト関数群 ---

// 1. Grayscale
float32_t3 ApplyGrayscale(float32_t3 color) {
    float32_t value = dot(color, float32_t3(0.2125f, 0.7154f, 0.0721f));
    return float32_t3(value, value, value);
}

// 2. Sepia
float32_t3 ApplySepia(float32_t3 color) {
    float32_t value = dot(color, float32_t3(0.2125f, 0.7154f, 0.0721f));
    return value * float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);
}

// 3. Vignette
float32_t3 ApplyVignette(float32_t3 color, float32_t2 uv, float32_t radius, float32_t softness, float32_t3 vignetteColor) {
    float dist = distance(uv, float2(0.5f, 0.5f));
    float vignette = smoothstep(radius, radius - softness, dist);
    return lerp(vignetteColor, color, vignette);
}

// 4. Noise
float32_t3 ApplyNoise(float32_t3 color, float32_t2 uv, float32_t intensity, float32_t time) {
    float32_t random = rand2dTo1d(uv * (time + 1.0f));
    float32_t noise = lerp(1.0f, random, intensity);
    return color * noise;
}

// 5. HSV
float32_t3 ApplyHSV(float32_t3 color, float32_t hue, float32_t saturation, float32_t value) {
    float32_t3 hsv = RGBToHSV(color);
    hsv.x = WrapValue(hsv.x + hue, 0.0, 1.0);
    hsv.y = saturate(hsv.y + saturation);
    hsv.z = saturate(hsv.z + value);
    return HSVToRGB(hsv);
}

// 6. ToneMapping
float32_t3 ApplyToneMapping(float32_t3 color, float32_t exposure) {
    return ACESFilm(color * exposure);
}

// 7. Fade
float32_t3 ApplyFade(float32_t3 color, float32_t3 fadeColor, float32_t intensity) {
    return lerp(color, fadeColor, intensity);
}

// 8. Slide
float32_t3 ApplySlide(float32_t3 color, float32_t2 uv, float32_t3 slideColor, float32_t threshold) {
    float32_t factor = smoothstep(threshold - 0.02f, threshold, uv.x);
    return lerp(slideColor, color, factor);
}

// 9. Dissolve (注: mask はサンプリング済みを渡す)
float32_t3 ApplyDissolve(float32_t3 color, float32_t mask, float32_t threshold, float32_t edgeRange, float32_t3 edgeColor) {
    if (mask <= threshold) return float32_t3(-1, -1, -1); // 棄却フラグとして負の値を返す
    float32_t edge = 1.0f - smoothstep(threshold, threshold + edgeRange, mask);
    return color + edge * edgeColor;
}

// 10. Glitch
/**
 * @brief グリッチエフェクトを適用する
 * @param color 現在のピクセルカラー
 * @param uv テクスチャ座標
 * @param time 時間
 * @param intensity グリッチの強度
 * @param tex サンプリングする元のテクスチャ
 * @param smp サンプラステート
 * @return グリッチ適用後のカラー
 */
float32_t3 ApplyGlitch(float32_t3 color, float32_t2 uv, GlitchParams params, Texture2D<float32_t4> tex, SamplerState smp, Texture2D<float32_t4> maskTex, int targetMaskId = 0) {
    // 画面端のマスク計算
    float2 centerOffset = uv - float2(0.5, 0.5);
    float dist = length(centerOffset);
    // edgeMaskStrengthが1.0の時、中心を広く保護し、端に向かって滑らかに上がるようにする
    float baseMask = smoothstep(0.25, 0.75, dist);
    // よりビネットのような「端だけ急激に強くなる」カーブにするために2乗する
    baseMask = baseMask * baseMask;
    
    float edgeFactor = lerp(1.0, baseMask, params.edgeMaskStrength);
    float effectiveIntensity = params.intensity * edgeFactor;

    // ブロックノイズ判定とUVの水平ズレ
    float2 block = floor(uv * float2(params.blockSizeX, params.blockSizeY));
    float noise = rand2dTo1d(block + params.time);
    
    // 確率で大きくズレるようにし、ベースのズレも加える
    float isGlitch = step(1.0 - params.probability, noise);
    float offsetX = ((noise - 0.5) * params.offsetMax * isGlitch + (noise - 0.5) * params.offsetBase) * effectiveIntensity;
    float2 displacedUv = saturate(uv + float2(offsetX, 0.0));

    // 個別エフェクトの場合、ズレ先が自分自身のオブジェクトでなければズレをキャンセルする
    if (targetMaskId != 0) {
        int maskId = round(maskTex.SampleLevel(smp, displacedUv, 0).r * 255.0f);
        if (maskId != targetMaskId) {
            displacedUv = uv;
        }
    }

    // RGBシフト（色ズレサンプリング）
    float shift = (params.rgbShiftBase + params.rgbShiftMax * isGlitch) * effectiveIntensity;
    
    float2 uvR = displacedUv + float2(shift, 0.0);
    float2 uvG = displacedUv;
    float2 uvB = displacedUv - float2(shift, 0.0);

    // RGBシフト先もオブジェクト外に出ないようチェックする
    if (targetMaskId != 0) {
        int maskIdR = round(maskTex.SampleLevel(smp, uvR, 0).r * 255.0f);
        if (maskIdR != targetMaskId) uvR = displacedUv;
        
        int maskIdB = round(maskTex.SampleLevel(smp, uvB, 0).r * 255.0f);
        if (maskIdB != targetMaskId) uvB = displacedUv;
    }

    float r = tex.SampleLevel(smp, uvR, 0).r;
    float g = tex.SampleLevel(smp, uvG, 0).g;
    float b = tex.SampleLevel(smp, uvB, 0).b;
    
    // カラーブレンド（isGlitch が発生している箇所のみ、指定色を乗せる）
    float3 glitchedColor = float3(r, g, b);
    float3 blendedColor = lerp(glitchedColor, glitchedColor * params.color.rgb, params.color.a * isGlitch * effectiveIntensity);
    
    // スキャンラインを加味して返す（乗算ブレンドっぽく適用）
    float scanline = sin(uv.y * params.scanlineFreq + params.time * 15.0) * params.scanlineIntensity * effectiveIntensity;
    return saturate(blendedColor * (1.0 + scanline));
}

// 11. Outline
float32_t3 ApplyDepthBasedOutline(float32_t3 color, float32_t2 uv, float32_t2 uvStepSize, float32_t4x4 projectionInverse, float32_t intensity, Texture2D<float32_t4> depthTex, SamplerState smp) {
    float32_t2 difference = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float32_t depth = depthTex.Sample(smp, uv + float32_t2(x, y) * uvStepSize).r;
            float32_t vz = ReconstructViewZ(depth, projectionInverse);
            
            float32_t wx = (x == 0) ? 0 : (x < 0 ? -1.0/6.0 : 1.0/6.0);
            float32_t wy = (y == 0) ? 0 : (y < 0 ? -1.0/6.0 : 1.0/6.0);
            difference.x += vz * wx;
            difference.y += vz * wy;
        }
    }
    float32_t depthWeight = length(difference) * 2.0f;
    float edgeThreshold = 0.5f / max(0.001f, intensity);
    float inkLine = smoothstep(edgeThreshold - 0.05f, edgeThreshold + 0.05f, depthWeight);
    return lerp(color, float32_t3(0, 0, 0), inkLine);
}

// 12. RadialBlur
float32_t3 ApplyRadialBlur(float32_t3 color, float32_t2 uv, float32_t2 center, float32_t blurWidth, int32_t samples, Texture2D<float32_t4> tex, SamplerState smp) {
    float32_t2 dir = uv - center;
    float32_t dist = length(dir);
    
    // 中心点からの距離に応じてサンプリング数を最適化 (距離0なら1回、距離0.5以上なら最大回数)
    int32_t actualSamples = max(1, int32_t(float32_t(samples) * saturate(dist * 2.0f)));
    
    float32_t3 sum = 0;
    for (int32_t j = 0; j < actualSamples; ++j) {
        sum += tex.SampleLevel(smp, uv + dir * blurWidth * float32_t(j), 0).rgb;
    }
    return sum / float32_t(actualSamples);
}

// 13. Separable Gaussian Blur (1D)
float32_t3 ApplyGaussian1D(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, float32_t sigma, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    float32_t weightTotal = 0.0f;
    
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t w = gauss(float32_t(i), 0.0f, sigma); // gauss関数のシグネチャ(2D)に合わせるためy=0.0fを渡すか、1D用関数を使う。※この下に1D版も定義します
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb * w;
        weightTotal += w;
    }
    
    if (weightTotal > 0.0f) {
        return sum * rcp(weightTotal);
    }
    return tex.SampleLevel(smp, uv, 0).rgb;
}

// 1次元ガウス関数（オーバーロード）
float32_t gauss1D(float32_t x, float32_t sigma) {
    if (sigma <= 0.0f) return 1.0f;
    static const float32_t PI = 3.1415926535f;
    float32_t exponent = -(x * x) * rcp(2.0f * sigma * sigma);
    float32_t denominator = sqrt(2.0f * PI) * sigma;
    return exp(exponent) * rcp(denominator);
}

// 13. Separable Gaussian Blur (1D) 改良版
float32_t3 ApplyGaussian1D_Optimized(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, float32_t sigma, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    float32_t weightTotal = 0.0f;
    
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t w = gauss1D(float32_t(i), sigma);
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb * w;
        weightTotal += w;
    }
    
    if (weightTotal > 0.0f) {
        return sum * rcp(weightTotal);
    }
    return tex.SampleLevel(smp, uv, 0).rgb;
}

// 14. Separable Box Blur (1D)
float32_t3 ApplyBoxBlur1D(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb;
    }
    
    int32_t sampleCount = (halfSize * 2) + 1;
    return sum / float32_t(sampleCount);
}

// 15. LuminanceBasedOutline
float32_t3 ApplyLuminanceBasedOutline(float32_t3 color, float32_t2 uv, float32_t2 uvStepSize, float32_t threshold, float32_t4 outlineColor, Texture2D<float32_t4> tex, SamplerState smp) {
    const float Gx[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
    const float Gy[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };
    
    float valueX = 0.0f;
    float valueY = 0.0f;
    
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float2 offset = float2(x, y) * uvStepSize;
            float3 sampleColor = tex.SampleLevel(smp, uv + offset, 0).rgb;
            float luminance = dot(sampleColor, float3(0.299f, 0.587f, 0.114f));
            
            valueX += luminance * Gx[y + 1][x + 1];
            valueY += luminance * Gy[y + 1][x + 1];
        }
    }
    
    float edgeWeight = sqrt(valueX * valueX + valueY * valueY);
    float factor = smoothstep(threshold - 0.05f, threshold + 0.05f, edgeWeight);
    return lerp(color, outlineColor.rgb, factor * outlineColor.a);
}

// 16. Pixelation
float32_t3 ApplyPixelation(float32_t2 uv, float32_t pixelSize, float32_t2 resolution, Texture2D<float32_t4> tex, SamplerState smp) {
    float2 blocks = resolution / max(1.0f, pixelSize);
    float2 pixelatedUV = floor(uv * blocks) / blocks;
    return tex.SampleLevel(smp, pixelatedUV, 0).rgb;
}

// 17. Pointillism
float32_t3 ApplyPointillism(float32_t2 uv, float32_t strokeSize, float32_t colorSteps, Texture2D<float32_t4> tex, SamplerState smp) {
    // 筆のサイズに合わせてグリッドを作る（strokeSize が大きいほど分割が粗くなる）
    // 参考: 1000.0f を基準にして、strokeSize (1.0 ~ 50.0) で割る
    float gridSize = max(20.0f, 2000.0f / max(1.0f, strokeSize));
    
    // ボロノイ分割を計算（戻り値の seedPos はグリッド空間の座標）
    VoronoiResult vRes = Voronoi(uv * gridSize);
    
    // テクスチャサンプリング用の UV に変換（0.0 ~ 1.0 空間に戻す）
    float2 sampleUV = vRes.seedPos / gridSize;
    
    // 各ドット（セル）の代表色をサンプリング
    float3 sampleColor = tex.SampleLevel(smp, saturate(sampleUV), 0).rgb;
    
    // 境界にほんの少しだけ陰影（立体感）をつける
    // セル中心からエッジに向かって少し暗くする
    float edgeDarken = smoothstep(0.85f, 0.0f, vRes.minDist);
    sampleColor *= (0.85f + 0.15f * edgeDarken);
    
    // 1. Linear -> sRGB変換（ガンマ補正）
    float3 srgbColor = pow(abs(sampleColor), 1.0f / 2.2f);
    
    // 2. RGB -> HSV変換
    float3 hsv = RGBToHSV(srgbColor);
    
    // 3. V（明度）だけを正しいロジック（round）でポスタライズ
    float steps = max(1.0f, colorSteps - 1.0f);
    hsv.z = round(hsv.z * steps) / steps;
    
    // 印象派らしく、彩度（S）を少しだけ強調する
    hsv.y = saturate(hsv.y * 1.2f);
    
    // 4. HSV -> RGBに戻し、再び Linear空間に戻す
    float3 finalSrgb = HSVToRGB(hsv);
    return pow(abs(finalSrgb), 2.2f);
}

// 18. Posterization (Toon/Cel-Shader)
float32_t3 ApplyPosterization(float32_t3 color, float32_t colorSteps) {
    // 【重要】なぜ単純なRGBの丸め（floor(color * steps) / steps）を使わないのか？
    // 1. リニア(Linear)空間のまま計算すると、人間の目の暗部の感覚と合わず全体的に暗く沈んでしまうため。
    // 2. RGBそれぞれの値を個別に丸めると、カラーバランスが崩れて「色が濁る」「意図しない色に化ける」バグが発生するため。
    // 
    // 【解決策】一度sRGB（ガンマ補正）空間に変換して人間の目に合わせ、
    // 色相(H)と彩度(S)はそのままに、明度(V)のみを四捨五入(round)で階調化することで高品質なアニメ塗りを実現する。

    // 1. Linear -> sRGB変換（暗部の解像度を人間の目に合わせる）
    float3 srgbColor = pow(abs(color), 1.0f / 2.2f);
    
    // 2. RGB -> HSV変換（色味を保持するため）
    float3 hsv = RGBToHSV(srgbColor);
    
    // 3. V（明度）だけを正しいロジック（round）でポスタライズ
    // ※ floorではなくroundを使うことで、意図しない暗転を防ぐ
    float steps = max(1.0f, colorSteps - 1.0f);
    hsv.z = round(hsv.z * steps) / steps;
    
    // アニメ塗りの鮮やかさを出すため、彩度（S）を少しだけ強調する
    hsv.y = saturate(hsv.y * 1.1f);
    
    // 4. HSV -> RGBに戻し、再び Linear空間に戻す
    float3 finalSrgb = HSVToRGB(hsv);
    return pow(abs(finalSrgb), 2.2f);
}

// 19. Night Vision (暗視ゴーグル風)
float32_t3 ApplyNightVision(float32_t3 color, float32_t2 uv, float32_t time, float32_t intensity) {
    float luminance = GetLuminance(color);
    float32_t3 nvColor = luminance * float32_t3(0.1f, 0.95f, 0.2f); // 暗視特有の緑色
    
    // 露出の強調
    nvColor = saturate(nvColor * 2.0f);
    
    // ノイズの付加
    float noise = rand2dTo1d(uv * (time + 1.0f));
    nvColor += (noise - 0.5f) * intensity;
    
    // スキャンライン
    float scanline = sin(uv.y * 800.0f - time * 10.0f) * 0.05f * intensity;
    return saturate(nvColor - scanline);
}

// 汎用UV変換: 万華鏡・放射状対称UVの取得
float32_t2 GetRadialSymmetryUV(float32_t2 uv, float32_t2 center, float32_t segments) {
    float32_t2 delta = uv - center;
    float radius = length(delta);
    float angle = atan2(delta.y, delta.x);
    
    // 角度をセグメントで分割し、モジュロで折り返す
    float pi = 3.1415926535f;
    float segmentAngle = pi * 2.0f / segments;
    
    // angle を 0 ~ 2PI に正規化
    angle = (angle < 0.0f) ? (angle + pi * 2.0f) : angle;
    
    // セグメント内で折り返し (ping-pong)
    float localAngle = fmod(angle, segmentAngle);
    if (fmod(floor(angle / segmentAngle), 2.0f) == 1.0f) {
        localAngle = segmentAngle - localAngle;
    }
    
    return center + float32_t2(cos(localAngle), sin(localAngle)) * radius;
}

// 22. Kaleidoscope (万華鏡 / 複眼)
float32_t3 ApplyKaleidoscope(float32_t2 uv, float32_t segments, Texture2D<float32_t4> tex, SamplerState smp) {
    float32_t2 center = float32_t2(0.5f, 0.5f);
    float32_t2 symUV = GetRadialSymmetryUV(uv, center, max(1.0f, segments));
    return tex.Sample(smp, symUV).rgb;
}

// 汎用サンプリング: RGBズレ（色収差）
float32_t3 SampleWithRGBShift(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 offset) {
    float32_t2 uvR = clamp(uv + offset, 0.0005f, 0.9995f);
    float32_t2 uvG = clamp(uv, 0.0005f, 0.9995f);
    float32_t2 uvB = clamp(uv - offset, 0.0005f, 0.9995f);
    float32_t r = tex.Sample(smp, uvR).r;
    float32_t g = tex.Sample(smp, uvG).g;
    float32_t b = tex.Sample(smp, uvB).b;
    return float32_t3(r, g, b);
}

// 汎用UV歪み: 高品質なSimplexノイズ(sFBm)を利用したオフセット取得
float32_t2 GetNoiseOffsetUV(float32_t2 uv, float32_t time) {
    // Noise.hlsli の Simplex Noise (sFBm) を使用。最初から -1.0 ~ 1.0 の範囲。
    // グリッド状のアーティファクト（縦線・横線）が全く出ないため回転ハックが不要。
    float n1 = sFBm(uv * 10.0f + float32_t2(time, time * 0.5f));
    float n2 = sFBm(uv * 10.0f - float32_t2(time * 0.8f, time));
    return float32_t2(n1, n2);
}

// 23. Chromatic Aberration (色収差)
float32_t3 ApplyChromaticAberration(float32_t2 uv, float32_t intensity, Texture2D<float32_t4> tex, SamplerState smp) {
    // 画面端に行くほどズレが大きくなるように調整
    float32_t2 center = float32_t2(0.5f, 0.5f);
    float32_t2 dir = uv - center;
    float32_t2 offset = dir * intensity;
    return SampleWithRGBShift(tex, smp, uv, offset);
}

// 24. Displacement Map (歪み・陽炎)
float32_t3 ApplyDisplacementMap(float32_t2 uv, float32_t time, float32_t intensity, float32_t rgbShiftRatio, Texture2D<float32_t4> tex, SamplerState smp) {
    float32_t2 offset = GetNoiseOffsetUV(uv, time) * intensity;
    // 歪ませつつ、指定された比率で色収差（RGBズレ）をブレンド
    return SampleWithRGBShift(tex, smp, uv + offset, offset * rgbShiftRatio);
}

// 25. Directional Blur (方向ブラー)
float32_t3 ApplyDirectionalBlur(float32_t2 uv, float32_t2 direction, float32_t strength, int samples, Texture2D<float32_t4> tex, SamplerState smp) {
    if (samples <= 1) return tex.Sample(smp, uv).rgb;
    
    float len = length(direction);
    if (len < 0.0001f) return tex.Sample(smp, uv).rgb;
    
    float32_t3 result = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t2 dir = (direction / len) * strength;
    float totalWeight = 0.0f;
    
    for (int i = 0; i < samples; i++) {
        float weight = 1.0f; // ここをガウス分布などにするとより自然になるが単純平均でも十分
        float32_t2 offset = dir * (float(i) / max(1.0f, float(samples - 1)) - 0.5f);
        result += tex.Sample(smp, uv + offset).rgb * weight;
        totalWeight += weight;
    }
    
    return result / totalWeight;
}

// 26. Halftone (網点 / コミック調)
float32_t3 ApplyHalftone(float32_t3 color, float32_t2 uv, float32_t2 screenRes, float32_t scale, float32_t angle, float32_t blend) {
    float luminance = GetLuminance(color);
    
    // アスペクト比を補正
    float32_t2 aspectUV = uv;
    aspectUV.x *= screenRes.x / screenRes.y;
    
    // 指定した角度で回転
    float s = sin(angle);
    float c = cos(angle);
    float32_t2 p = float32_t2(c * aspectUV.x - s * aspectUV.y, s * aspectUV.x + c * aspectUV.y);
    p *= scale;
    
    // 各マスのローカル座標 (-0.5 ~ 0.5)
    float2 localPos = frac(p) - 0.5f;
    
    // 中心からの距離 (0.0 ~ 約0.707)
    float dist = length(localPos);
    
    // 輝度に応じてドットの半径を決定 (暗いほど大きな黒ドット)
    float maxRadius = 0.707f; // マスを完全に埋めるサイズ
    float radius = (1.0f - luminance) * maxRadius;
    
    // ドットの内側(黒)と外側(白)を分ける (アンチエイリアス用に少しぼかす)
    // dist が radius より大きければ 1.0(紙/色)、小さければ 0.0(黒インク)
    float dot = smoothstep(radius - 0.05f, radius + 0.05f, dist);
    
    // カラーコミック調にするため、元の色から「明るさ(影)」を取り除いた鮮やかなベースカラー(紙の色)を作る
    // ※これをしないと、元々暗い影の部分に黒ドットが乗って完全に真っ黒(何も見えない状態)になります。
    float32_t3 paperColor = color / max(luminance, 0.01f); 
    paperColor = min(paperColor, 1.0f); // 白飛び防止
    
    // 網点適用: 暗い部分は黒インク、明るい部分は鮮やかなインク色(紙の色)
    float32_t3 result = lerp(float32_t3(0.0f, 0.0f, 0.0f), paperColor, dot);
    
    return lerp(color, result, blend);
}

// 27. Depth of Field (被写界深度)
float32_t3 ApplyDepthOfField(float32_t3 color, float32_t2 uv, Texture2D<float32_t4> depthTex, SamplerState smpPoint, SamplerState smpLinear, float32_t4x4 projInv, float focusDist, float focusRange, float blurSize, int32_t samples, float32_t2 screenRes, Texture2D<float32_t4> tex) {
    // 1. 深度取得 & NDC -> View Z に変換
    float ndcDepth = depthTex.SampleLevel(smpPoint, uv, 0).r;
    float viewZ = ReconstructViewZ(ndcDepth, projInv);
    
    // 2. ボケみ (CoC: Circle of Confusion) の計算 (0.0 ~ 1.0)
    float coc = abs(viewZ - focusDist) / max(focusRange, 0.001f);
    coc = saturate(coc); // 0.0 (ピント合う) ~ 1.0 (最大ボケ)
    
    if (coc < 0.05f) return color; // ボケがない場合はスキップして高速化
    
    // 3. Bokeh(円形)ブラーのサンプリング
    float radius = coc * blurSize;
    float32_t3 resultColor = color;
    float totalWeight = 1.0f;
    
    int loopCount = clamp(samples, 1, 128); // GPU負荷爆発防止
    const float kGoldenAngle = 2.39996323f; // 137.5度 (Vogel's model)
    float32_t2 uvStep = 1.0f / screenRes;
    
    for (int i = 1; i <= loopCount; i++) {
        // らせん状にサンプリングポイントを配置
        float r = radius * sqrt(float(i) / float(loopCount));
        float theta = float(i) * kGoldenAngle;
        
        float2 offset = float2(cos(theta), sin(theta)) * r * uvStep;
        float2 sampleUV = uv + offset;
        
        // 周辺のピクセルをサンプリング
        float3 sampleColor = tex.SampleLevel(smpLinear, sampleUV, 0).rgb;
        resultColor += sampleColor;
        totalWeight += 1.0f;
    }
    
    return resultColor / totalWeight;
}

// 29. Gravity Distortion (重力空間歪曲 ＋ 色収差)
float32_t3 ApplyGravityDistortion(float32_t2 uv, float32_t2 center, float32_t radius, float32_t intensity, float32_t waveWidth, float32_t chromaticRatio, float32_t time, Texture2D<float32_t4> tex, SamplerState smp) {
    if (intensity <= 0.0001f || waveWidth <= 0.0001f) {
        return tex.Sample(smp, uv).rgb;
    }

    float32_t2 delta = uv - center;
    float32_t dist = length(delta);
    if (dist <= 0.0001f) {
        return tex.Sample(smp, uv).rgb;
    }

    float32_t2 dir = delta / dist;

    // 波面（リング帯）の距離
    float32_t d = abs(dist - radius);
    if (d > waveWidth) {
        return tex.Sample(smp, uv).rgb;
    }

    // ガウシアン風の滑らかな減衰窓
    float32_t normD = d / waveWidth;
    float32_t wave = (1.0f - normD * normD);
    wave = wave * wave;

    // 波紋の正弦波変位（中心に向かって引き込まれる、または外側に押し出される波）
    float32_t disp = sin((dist - radius) / waveWidth * 3.14159265f) * wave * intensity;

    float32_t2 offset = dir * disp;

    // 波面のリング帯でしっかり赤・青が分離するよう色収差オフセットを強化
    // chromaticRatio を画面比率の色ズレ幅として直接適用（例: 0.02 = 約20px）
    float32_t2 chromaticOffset = dir * (wave * chromaticRatio);

    // 色収差付きサンプリング（SampleWithRGBShift）
    return SampleWithRGBShift(tex, smp, uv + offset, chromaticOffset);
}

