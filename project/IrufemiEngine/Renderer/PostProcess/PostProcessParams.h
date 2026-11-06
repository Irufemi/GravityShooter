#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include <cstdint>

namespace Irufemi {

/**
 * @struct CustomEffectParams
 * @brief 個別オブジェクトに適用する詳細エフェクト用パラメータ（汎用構造体）
 */
struct CustomEffectParams {
    Irufemi::Vector4 color1 = {1.0f, 1.0f, 1.0f, 1.0f}; // Edge Color, Slide Color, etc.
    Irufemi::Vector4 color2 = {0.0f, 0.0f, 0.0f, 1.0f}; // Background Color
    float param1 = 0.0f;                                // Threshold, Intensity, etc.
    float param2 = 0.0f;                                // Edge Range, Radius, etc.
    float param3 = 0.0f;                                // Noise Type, Softness, etc.
    float param4 = 0.0f;                                // Time, Angle, etc.

    bool operator==(const CustomEffectParams& other) const {
        return color1 == other.color1 && color2 == other.color2 && param1 == other.param1 && param2 == other.param2 &&
               param3 == other.param3 && param4 == other.param4;
    }
    bool operator!=(const CustomEffectParams& other) const {
        return !(*this == other);
    }

    template <typename Visitor> void Visit(Visitor&& v) {
        v.ColorEdit4("Color 1", color1);
        v.ColorEdit4("Color 2", color2);
        v.DragFloat("Param 1", param1);
        v.DragFloat("Param 2", param2);
        v.DragFloat("Param 3", param3);
        v.DragFloat("Param 4", param4);
    }
};

// 定数バッファの最大サイズ（256個まで）
inline constexpr uint32_t kMaxCustomEffectParams = 256;

/**
 * @struct NoiseParams
 * @brief ノイズエフェクト用パラメータ
 */
struct NoiseParams {
    float intensity = 0.5f; ///< ノイズの強度 (0.0 ~ 1.0)
    float time = 0.0f;      ///< 時間経過（内部で更新される）

    template <typename Visitor> void Visit(Visitor&& v) {
        v.SliderFloat("Noise Intensity", intensity, 0.0f, 1.0f);
    }
};

/**
 * @struct VignetteParams
 * @brief ビネットエフェクト用パラメータ
 */
struct VignetteParams {
    Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< ビネットの色 (RGB)
    float radius = 0.8f;                               ///< 減衰の開始半径 (0.0~1.5)
    float softness = 0.5f;                             ///< 減衰の柔らかさ (0.0~1.0)
    float pad[2];                                      // 16バイトアライメント用パディング

    template <typename Visitor> void Visit(Visitor&& v) {
        v.ColorEdit4("Vignette Color", color);
        v.DragFloat("Vignette Radius", radius, 0.01f, 0.0f, 2.0f);
        v.DragFloat("Vignette Softness", softness, 0.01f, 0.0f, 2.0f);
    }
};

/**
 * @struct SmoothingParams
 * @brief 平滑化エフェクト用パラメータ
 */
struct SmoothingParams {
    Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
    int32_t kernelSize = 3;                    ///< カーネルサイズ (奇数推奨)
    float pad;

    template <typename Visitor> void Visit(Visitor&& v) {
        v.SliderKernelSize("Kernel Size", kernelSize, 1, 31);
    }
};

/**
 * @struct GaussianParams
 * @brief ガウスぼかし用パラメータ
 */
struct GaussianParams {
    Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
    float sigma = 2.0f;                        ///< 標準偏差（ぼけ具合）
    int32_t kernelSize = 3;                    ///< カーネルサイズ (奇数推奨)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Sigma", sigma, 0.01f, 0.01f, 10.0f);
        v.SliderKernelSize("Kernel Size", kernelSize, 1, 31);
    }
};

/**
 * @struct RadialBlurParams
 * @brief 放射状ぼかし用パラメータ
 */
struct RadialBlurParams {
    Irufemi::Vector2 center = {0.5f, 0.5f}; ///< ぼかしの中心点 (UV空間 0.0 ~ 1.0)
    float blurWidth = 0.01f;                ///< ぼかしの幅
    int32_t numSamples = 10;                ///< サンプル数

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat2("Center", center, 0.01f, 0.0f, 1.0f);
        v.DragFloat("Blur Width", blurWidth, 0.001f, 0.0f, 0.1f);
        v.SliderInt("Samples", numSamples, 1, 100);
    }
};

/**
 * @struct OutlineParams
 * @brief アウトラインエフェクト用パラメータ
 */
struct OutlineParams {
    float intensity = 6.0f;    ///< アウトラインの強度
    int32_t maskMaxRadius = 5; ///< マスクベースアウトラインの最大探索半径
    float pad[2];
    Irufemi::Matrix4x4 projectionInverse; ///< 逆投影行列 (自動でセットされる)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Outline Intensity", intensity, 0.1f, 0.0f, 20.0f);
        v.SliderInt("Mask Max Radius", maskMaxRadius, 1, 20);
    }
};

/**
 * @struct LuminanceOutlineParams
 * @brief 輝度ベースのアウトラインエフェクト用パラメータ
 */
struct LuminanceOutlineParams {
    float threshold = 0.5f;                                   ///< 輪郭抽出のしきい値
    float pad[3];                                             // 16バイトアライメント
    Irufemi::Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 1.0f}; ///< アウトラインの色

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Threshold", threshold, 0.01f, 0.0f, 1.0f);
        v.ColorEdit4("Outline Color", outlineColor);
    }
};

/**
 * @struct PixelationParams
 * @brief ピクセレーション（モザイク）エフェクト用パラメータ
 */
struct PixelationParams {
    float pixelSize = 4.0f; ///< 1ドットを構成するピクセル数（解像度ダウン係数）
    float pad[3];

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Pixel Size", pixelSize, 0.1f, 1.0f, 64.0f);
    }
};

/**
 * @struct PointillismParams
 * @brief 点描画エフェクト用パラメータ
 */
struct PointillismParams {
    float strokeSize = 10.0f; ///< 筆の荒さ
    float colorSteps = 8.0f;  ///< 色の階調数
    float pad[2];

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Stroke Size", strokeSize, 0.1f, 1.0f, 50.0f);
        v.DragFloat("Color Steps", colorSteps, 0.1f, 2.0f, 32.0f);
    }
};

/**
 * @struct DissolveParams
 * @brief ディゾルブエフェクト用パラメータ
 */
struct DissolveParams {
    Irufemi::Vector4 edgeColor = {1.0f, 1.0f, 1.0f, 1.0f};       ///< 境界線の色 (無彩色化)
    Irufemi::Vector4 backgroundColor = {0.0f, 0.0f, 0.0f, 1.0f}; ///< 背景色 (追加)
    float threshold = 0.0f;                                      ///< 消失しきい値 (0.0 ~ 1.0)
    float edgeRange = 0.03f;                                     ///< 境界線の幅
    int32_t noiseType = 0;                                       ///< 使用するノイズテクスチャのインデックス (0 or 1)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.SliderFloat("Threshold", threshold, 0.0f, 1.0f);
        v.SliderFloat("Edge Range", edgeRange, 0.0f, 0.2f);
        v.ColorEdit4("Edge Color", edgeColor);
        v.ColorEdit4("Background Color", backgroundColor);
        const char* noiseTypes[] = {"Noise 0", "Noise 1"};
        v.Combo("Noise Type", noiseType, noiseTypes, 2);
    }
};

/**
 * @struct HSVParams
 * @brief HSVエフェクト用パラメータ
 */
struct HSVParams {
    float hue = 0.0f;        ///< 色相オフセット (-1.0 ~ 1.0)
    float saturation = 0.0f; ///< 彩度オフセット (-1.0 ~ 1.0)
    float value = 0.0f;      ///< 明度オフセット (-1.0 ~ 1.0)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("HueOffset", hue, 0.001f, -1.0f, 1.0f);
        v.DragFloat("SaturationOffset", saturation, 0.001f, -1.0f, 1.0f);
        v.DragFloat("ValueOffset", value, 0.001f, -1.0f, 1.0f);
    }
};

/**
 * @struct ToneMappingParams
 * @brief トーンマッピングエフェクト用パラメータ
 */
struct ToneMappingParams {
    float exposure = 1.0f; ///< 露出補正 (0.0 ~ )

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Exposure", exposure, 0.01f, 0.0f, 10.0f);
    }
};

/**
 * @struct FadeParams
 * @brief フェードエフェクト用パラメータ
 */
struct FadeParams {
    Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< フェード色
    float intensity = 0.0f;                            ///< 強度 (0.0 ~ 1.0)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.ColorEdit4("Fade Color", color);
        v.SliderFloat("Fade Intensity", intensity, 0.0f, 1.0f);
    }
};

/**
 * @struct SlideParams
 * @brief スライドエフェクト用パラメータ
 */
struct SlideParams {
    Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< スライドの色
    float threshold = 0.0f;                            ///< 進行度 (0.0 ~ 1.0)

    template <typename Visitor> void Visit(Visitor&& v) {
        v.ColorEdit4("Slide Color", color);
        v.SliderFloat("Slide Threshold", threshold, 0.0f, 1.0f);
    }
};

/**
 * @struct BloomParams
 * @brief ブルームエフェクト用パラメータ
 */
struct BloomParams {
    Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
    float threshold = 0.8f;                    ///< 高輝度抽出のしきい値
    float sigma = 3.0f;                        ///< ぼかしの強さ
    float intensity = 1.0f;                    ///< ブルームの強度
    int32_t kernelSize = 21;                   ///< ぼかしのカーネルサイズ

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Threshold", threshold, 0.01f, 0.0f, 5.0f);
        v.DragFloat("Sigma", sigma, 0.01f, 0.01f, 10.0f);
        v.DragFloat("Intensity", intensity, 0.01f, 0.0f, 10.0f);
        v.SliderKernelSize("Kernel Size", kernelSize, 1, 51);
    }
};

/**
 * @struct GlitchParams
 * @brief グリッチエフェクト用パラメータ
 */
struct GlitchParams {
    float intensity = 1.0f;        ///< グリッチの強さ
    float time = 0.0f;             ///< 時間経過（内部で更新される）
    float edgeMaskStrength = 0.0f; ///< 画面端にかける強さ
    float probability = 0.4f;      ///< グリッチが発生する確率 (0.0 ~ 1.0)

    float blockSizeX = 16.0f; ///< ブロックの横分割数
    float blockSizeY = 32.0f; ///< ブロックの縦分割数
    float offsetBase = 0.03f; ///< 基本の横ズレ幅
    float offsetMax = 0.15f;  ///< グリッチ時の最大横ズレ幅

    float rgbShiftBase = 0.01f;      ///< 基本のRGBズレ幅
    float rgbShiftMax = 0.02f;       ///< グリッチ時の最大RGBズレ幅
    float scanlineFreq = 800.0f;     ///< スキャンラインの周波数（細かさ）
    float scanlineIntensity = 0.05f; ///< スキャンラインの濃さ

    Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 0.0f}; ///< rgb = 色, a = ブレンド強度

    template <typename Visitor> void Visit(Visitor&& v) {
        v.SliderFloat("Glitch Intensity", intensity, 0.0f, 5.0f);
        v.SliderFloat("Edge Mask Strength", edgeMaskStrength, 0.0f, 1.0f);
        v.SliderFloat("Probability", probability, 0.0f, 1.0f);
        v.DragFloat("RGB Shift Base", rgbShiftBase, 0.001f, 0.0f, 0.1f);
    }
};

/**
 * @struct DualKawaseBlurParams
 * @brief カワセブラー用パラメータ
 */
struct DualKawaseBlurParams {
    float blurRadius = 1.0f;    ///< ぼかしのサンプリング半径オフセット
    float intensity = 1.0f;     ///< ブラーの最終的な強度
    int32_t iterationCount = 4; ///< ダウン/アップサンプルの繰り返し回数（最大8等）
    float pad = 0.0f;           // 16バイトアライメント用

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Blur Radius Offset", blurRadius, 0.01f, 0.0f, 5.0f);
        v.SliderInt("Iteration Count", iterationCount, 1, 8);
        v.DragFloat("Intensity", intensity, 0.01f, 0.0f, 10.0f);
    }
};

/**
 * @struct PosterizationParams
 * @brief ポスタリゼーション（階調化）用パラメータ
 */
struct PosterizationParams {
    float colorSteps = 8.0f; ///< 階調の段数（少ないほどベタ塗りになる）

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Color Steps", colorSteps, 0.1f, 2.0f, 32.0f);
    }
};

/**
 * @struct NightVisionParams
 * @brief 暗視ゴーグル風エフェクト用パラメータ
 */
struct NightVisionParams {
    float intensity = 0.5f; ///< ノイズとスキャンラインの強度
    float time = 0.0f;      ///< 時間経過（内部で更新される）
    float pad[2];

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Intensity", intensity, 0.01f, 0.0f, 2.0f);
    }
};

/**
 * @struct KaleidoscopeParams
 * @brief 万華鏡エフェクト用パラメータ
 */
struct KaleidoscopeParams {
    float segments = 6.0f; ///< 分割数
    float pad[3];

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Segments", segments, 0.1f, 1.0f, 32.0f);
    }
};

/**
 * @struct ChromaticAberrationParams
 * @brief 色収差エフェクト用パラメータ
 */
struct ChromaticAberrationParams {
    float intensity = 0.05f; ///< 色ズレの幅
    float pad[3];

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Intensity", intensity, 0.001f, 0.0f, 1.0f);
    }
};

/**
 * @struct DisplacementMapParams
 * @brief 画面の歪み・陽炎エフェクト用パラメータ
 */
struct DisplacementMapParams {
    float intensity = 0.05f; ///< 歪みの強さ
    float timeScale = 1.0f;  ///< うねりの速度
    float time = 0.0f;       ///< 時間
    float pad;

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Intensity", intensity, 0.001f, 0.0f, 1.0f);
        v.DragFloat("Time Scale", timeScale, 0.01f, 0.0f, 10.0f);
    }
};

/**
 * @struct DirectionalBlurParams
 * @brief 方向ブラーエフェクト用パラメータ
 */
struct DirectionalBlurParams {
    Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ブラーの方向
    float strength = 0.05f;                    ///< ブラーの強さ
    int samples = 10;                          ///< サンプル数

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat2("Direction", direction, 0.01f, -1.0f, 1.0f);
        v.DragFloat("Strength", strength, 0.001f, 0.0f, 1.0f);
        v.SliderInt("Samples", samples, 2, 32);
    }
};

/**
 * @struct HalftoneParams
 * @brief ハーフトーンエフェクト用パラメータ
 */
struct HalftoneParams {
    float scale = 150.0f;    ///< ドットの細かさ
    float angle = 0.785398f; ///< ドットの回転角 (45度 = 約0.785ラジアン)
    float blend = 1.0f;      ///< 適用強度
    float pad;

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Scale", scale, 1.0f, 10.0f, 500.0f);
        v.SliderAngle("Angle", angle, -180.0f, 180.0f);
        v.DragFloat("Blend", blend, 0.01f, 0.0f, 1.0f);
    }
};

/**
 * @struct DepthOfFieldParams
 * @brief 被写界深度エフェクト用パラメータ
 */
struct DepthOfFieldParams {
    float focusDistance = 10.0f; ///< ピントが合う距離 (View Z)
    float focusRange = 5.0f;     ///< ピントが合う範囲 (前後)
    float blurSize = 10.0f;      ///< 最大ブラーサイズ (ピクセル半径)
    int samples = 16;            ///< ブラーのサンプリング数

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat("Focus Distance", focusDistance, 0.1f, 0.0f, 1000.0f);
        v.DragFloat("Focus Range", focusRange, 0.1f, 0.1f, 500.0f);
        v.DragFloat("Blur Size", blurSize, 0.1f, 0.0f, 50.0f);
        v.SliderInt("Samples", samples, 4, 64);
    }
};

/**
 * @struct GravityDistortionParams
 * @brief 重力波・空間歪曲エフェクト用パラメータ
 */
struct GravityDistortionParams {
    Irufemi::Vector2 center = {0.5f, 0.5f}; ///< 歪み中心 UV (0.0〜1.0)
    float radius = 0.0f;                    ///< 現在の波面半径
    float intensity = 0.0f;                 ///< 歪み強度
    float waveWidth = 0.08f;                ///< 波面の太さ
    float chromaticRatio = 0.04f;           ///< 色収差の強さ (RGBズレ)
    float time = 0.0f;                      ///< 経過時間
    float pad = 0.0f;

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat2("Center UV", center, 0.01f, 0.0f, 1.0f);
        v.DragFloat("Radius", radius, 0.01f, 0.0f, 2.0f);
        v.DragFloat("Intensity", intensity, 0.01f, -1.0f, 1.0f);
        v.DragFloat("Wave Width", waveWidth, 0.005f, 0.01f, 0.5f);
        v.DragFloat("Chromatic Ratio", chromaticRatio, 0.005f, 0.0f, 0.2f);
    }
};

/**
 * @struct BlackHoleSupernovaParams
 * @brief ブラックホール（事象の地平面＆求心スワール）および超新星衝撃波用パラメータ
 */
struct BlackHoleSupernovaParams {
    Irufemi::Vector2 center = {0.5f, 0.5f}; ///< スクリーン中心UV (0.0〜1.0)
    float blackHoleRadius = 0.0f;           ///< 事象の地平面半径
    float blackHolePullIntensity = 0.0f;    ///< 求心スワール吸引強度
    float supernovaRadius = 0.0f;           ///< スーパーノヴァ衝撃波半径
    float supernovaIntensity = 0.0f;        ///< 衝撃波変位強度
    float supernovaChromaticRatio = 0.0f;   ///< GRAVITY DAZE 色収差比率
    float supernovaFlashIntensity = 0.0f;   ///< 瞬間ガンマ線バースト閃光強度

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat2("Center UV", center, 0.01f, 0.0f, 1.0f);
        v.DragFloat("Black Hole Radius", blackHoleRadius, 0.01f, 0.0f, 1.0f);
        v.DragFloat("Pull Intensity", blackHolePullIntensity, 0.01f, 0.0f, 2.0f);
        v.DragFloat("Supernova Radius", supernovaRadius, 0.01f, 0.0f, 3.0f);
        v.DragFloat("Supernova Intensity", supernovaIntensity, 0.01f, -2.0f, 2.0f);
        v.DragFloat("Chromatic Ratio", supernovaChromaticRatio, 0.005f, 0.0f, 0.5f);
        v.DragFloat("Flash Intensity", supernovaFlashIntensity, 0.01f, 0.0f, 5.0f);
    }
};

/**
 * @struct LightShaftsParams
 * @brief ゴッドレイ（光の筋）エフェクト用パラメータ
 */
struct LightShaftsParams {
    Irufemi::Vector2 lightScreenPos = {0.5f, 0.5f}; ///< 光源のスクリーン座標 (0.0~1.0)
    float density = 1.0f;                           ///< サンプリング密度
    float decay = 0.95f;                            ///< 減衰率
    float weight = 0.5f;                            ///< 重み
    float exposure = 1.0f;                          ///< 露出
    int32_t samples = 64;                           ///< サンプリング数
    float pad;                                      ///< パディング

    template <typename Visitor> void Visit(Visitor&& v) {
        v.DragFloat2("Light Screen Pos", lightScreenPos, 0.01f, -1.0f, 2.0f);
        v.DragFloat("Density", density, 0.01f, 0.0f, 5.0f);
        v.DragFloat("Decay", decay, 0.001f, 0.8f, 1.0f);
        v.DragFloat("Weight", weight, 0.01f, 0.0f, 2.0f);
        v.DragFloat("Exposure", exposure, 0.01f, 0.0f, 5.0f);
        v.SliderInt("Samples", samples, 8, 128);
    }
};

/**
 * @struct CombinedParams
 * @brief 統合ポストプロセス用定数バッファ構造体
 */
struct CombinedParams {
    int32_t effectCount;
    int32_t useMask;
    int32_t pad0[2]; // HLSLの int4[4] の開始位置（16バイト境界）に合わせるためのパディング
    int32_t effects[16];

    // Vignette
    Irufemi::Vector4 vignetteColor;
    float vignetteRadius;
    float vignetteSoftness;
    float pad1[2];

    // Noise
    float noiseIntensity;
    float noiseTime;
    float pad_noise[2]; // HLSLの float4(dissolveEdgeColor) 用に16バイト境界までパディング

    // Dissolve
    Irufemi::Vector4 dissolveEdgeColor;
    Irufemi::Vector4 dissolveBackgroundColor;
    float dissolveThreshold;
    float dissolveEdgeRange;

    // HSV
    float hsvHue;
    float hsvSaturation;
    float hsvValue;

    // ToneMapping
    float toneMappingExposure;
    float pad2[2]; // HLSLの float4(fadeColor) 用に16バイト境界までパディング

    // Fade
    Irufemi::Vector4 fadeColor;
    float fadeIntensity;
    float pad3[3]; // HLSLの float4(slideColor) 用に16バイト境界までパディング

    // Slide
    Irufemi::Vector4 slideColor;
    float slideThreshold;
    float pad4[3]; // HLSLの float4x4(projectionInverse) 用に16バイト境界までパディング

    // Outline
    Irufemi::Matrix4x4 projectionInverse;
    float outlineIntensity;
    int32_t maskOutlineMaxRadius;
    float pad_outline[2]; // HLSLの float2(radialBlurCenter) 用に16バイト境界までパディング

    // RadialBlur
    Irufemi::Vector2 radialBlurCenter;
    float radialBlurWidth;
    int32_t radialBlurSamples;

    // Glitch
    float glitchIntensity;
    float glitchTime;
    float glitchEdgeMaskStrength;
    float glitchProbability;

    float glitchBlockSizeX;
    float glitchBlockSizeY;
    float glitchOffsetBase;
    float glitchOffsetMax;

    float glitchRgbShiftBase;
    float glitchRgbShiftMax;
    float glitchScanlineFreq;
    float glitchScanlineIntensity;
    Irufemi::Vector4 glitchColor;

    // LuminanceBasedOutline
    Irufemi::Vector4 luminanceOutlineColor;
    float luminanceOutlineThreshold;
    float pad_lumOutline[3];

    // Pixelation
    float pixelationSize;
    float pad_pixelation[3];

    // Pointillism
    float pointillismStrokeSize;
    float pointillismColorSteps;
    float pad_pointillism[2];

    // Posterization
    float posterizationSteps;
    float pad_posterization[3];

    // NightVision
    float nightVisionIntensity;
    float nightVisionTime;
    float pad_nightVision[2];

    // Kaleidoscope
    float kaleidoscopeSegments;
    float pad_kaleidoscope[3];

    // ChromaticAberration
    float chromaticAberrationIntensity;
    float pad_chromaticAberration[3];

    // DisplacementMap
    float displacementMapIntensity;
    float displacementMapTime;
    float displacementMapTimeScale;
    float pad_displacementMap;

    // DirectionalBlur
    Irufemi::Vector2 directionalBlurDirection;
    float directionalBlurStrength;
    int directionalBlurSamples;

    // Halftone
    float halftoneScale;
    float halftoneAngle;
    float halftoneBlend;
    float pad_halftone;

    // DepthOfField
    float dofFocusDistance;
    float dofFocusRange;
    float dofBlurSize;
    int32_t dofSamples;

    // GravityDistortion
    Irufemi::Vector2 gravityDistortionCenter;
    float gravityDistortionRadius;
    float gravityDistortionIntensity;
    float gravityDistortionWaveWidth;
    float gravityDistortionChromaticRatio;
    float gravityDistortionTime;
    float pad_gravityDistortion;

    // BlackHoleSupernova (32 bytes = 8 float)
    Irufemi::Vector2 blackHoleSupernovaCenter;
    float blackHoleRadius;
    float blackHolePullIntensity;
    float supernovaRadius;
    float supernovaIntensity;
    float supernovaChromaticRatio;
    float supernovaFlashIntensity;

    // [Bindless]
    uint32_t mainTextureIndex;
    uint32_t extraTextureIndex;
    uint32_t maskTextureIndex;
    uint32_t padding_bindless;

    // 256-byte alignment padding (Current size: 688 bytes, padded to 768 bytes)
    uint32_t alignPadding[20];
};

static_assert(sizeof(CombinedParams) % 256 == 0, "CombinedParams must be 256-byte aligned for D3D12 CBV.");

struct alignas(256) BindlessParams {
    uint32_t mainTextureIndex;
    uint32_t extraTextureIndex;
    uint32_t maskTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t materialTextureIndex;
    uint32_t velocityTextureIndex;
    uint32_t padding[58]; // 256バイトアライメント (64 * 4 = 256)
};
static_assert(sizeof(BindlessParams) % 256 == 0, "BindlessParams must be 256-byte aligned for D3D12 CBV.");

} // namespace Irufemi
