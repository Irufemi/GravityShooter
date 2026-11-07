#pragma once

#include <cstdint>

namespace Irufemi {

/**
 * @enum PostProcessPassType
 * @brief エフェクトのレンダーパス実行形態
 */
enum class PostProcessPassType {
    Inline,   ///< CombinedPostEffect内で他エフェクトと1パス一括合成可能
    Separate, ///< ピクセル周囲のサンプリング・変位等のため単独ピンポンパスが必要
    MultiPass ///< 複数の中間テクスチャを使用する自己完結型マルチパス（Bloom, Blur等）
};

} // namespace Irufemi

/**
 * @brief 全ポストプロセスエフェクトの単一の定義源 (Single Source of Truth)
 * @details 順序は HLSL の定数テーブル (kPostProcessMode_*) と 100% 一致します。
 * 形式: V(ModeName, StructType, PassType, DefaultLayer, DisplayName)
 */
#define IRUFEMI_POSTPROCESS_EFFECT_LIST(V)                                                                             \
    V(None, void, Inline, PreUI, "None")                                                                               \
    V(Grayscale, void, Inline, PreUI, "Grayscale")                                                                     \
    V(Sepia, void, Inline, PreUI, "Sepia")                                                                             \
    V(Vignette, VignetteParams, Inline, PreUI, "Vignette")                                                             \
    V(Smoothing, SmoothingParams, MultiPass, PreUI, "Smoothing")                                                       \
    V(GaussianFilter, GaussianParams, MultiPass, PreUI, "GaussianFilter")                                              \
    V(DepthBasedOutline, OutlineParams, Separate, PreUI, "DepthBasedOutline")                                          \
    V(RadialBlur, RadialBlurParams, Separate, PreUI, "RadialBlur")                                                     \
    V(Dissolve, DissolveParams, Inline, PreUI, "Dissolve")                                                             \
    V(Noise, NoiseParams, Inline, PreUI, "Noise")                                                                      \
    V(HSV, HSVParams, Inline, PreUI, "HSV")                                                                            \
    V(ToneMapping, ToneMappingParams, Inline, PreUI, "ToneMapping")                                                    \
    V(Fade, FadeParams, Inline, PostUI, "Fade")                                                                        \
    V(Slide, SlideParams, Inline, PostUI, "Slide")                                                                     \
    V(Bloom, BloomParams, MultiPass, PreUI, "Bloom")                                                                   \
    V(Glitch, GlitchParams, Separate, PreUI, "Glitch")                                                                 \
    V(DualKawaseBlur, DualKawaseBlurParams, MultiPass, PreUI, "DualKawaseBlur")                                        \
    V(LuminanceBasedOutline, LuminanceOutlineParams, Inline, PreUI, "LuminanceBasedOutline")                           \
    V(Pixelation, PixelationParams, Inline, PreUI, "Pixelation")                                                       \
    V(Pointillism, PointillismParams, Separate, PreUI, "Pointillism")                                                  \
    V(Posterization, PosterizationParams, Inline, PreUI, "Posterization")                                              \
    V(NightVision, NightVisionParams, Inline, PreUI, "NightVision")                                                    \
    V(Kaleidoscope, KaleidoscopeParams, Separate, PreUI, "Kaleidoscope")                                               \
    V(ChromaticAberration, ChromaticAberrationParams, Separate, PreUI, "ChromaticAberration")                          \
    V(DisplacementMap, DisplacementMapParams, Separate, PreUI, "DisplacementMap")                                      \
    V(DirectionalBlur, DirectionalBlurParams, Separate, PreUI, "DirectionalBlur")                                      \
    V(Halftone, HalftoneParams, Inline, PreUI, "Halftone")                                                             \
    V(DepthOfField, DepthOfFieldParams, Separate, PreUI, "DepthOfField")                                               \
    V(LightShafts, LightShaftsParams, MultiPass, PreUI, "LightShafts")                                                 \
    V(GravityDistortion, GravityDistortionParams, Separate, PreUI, "GravityDistortion")                                \
    V(BlackHoleSupernova, BlackHoleSupernovaParams, Separate, PreUI, "BlackHoleSupernova")
