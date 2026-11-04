#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <vector>

namespace Irufemi {

/**
 * @class CombinedPostEffect
 * @brief 統合ポストプロセスエフェクト（Uber Shader による単一/複数エフェクトの一括合成）
 * @details シングルパス系の各種設定パラメータ（ToneMapping, HSV, Vignette, Outline等）を保持・管理
 */
class CombinedPostEffect : public IPostProcessEffect {
public:
    CombinedPostEffect() = default;
    ~CombinedPostEffect() override = default;

    /**
     * @brief バッチ実行するエフェクト一覧を設定する
     */
    void SetBatch(const std::vector<PostProcessManager::Mode>& batch) {
        batch_ = batch;
    }

    /**
     * @brief バッチをクリアする
     */
    void ClearBatch() {
        batch_.clear();
    }

    /**
     * @brief 全シングルパスパラメータを初期値にリセットする
     * @param clearPostUI PostUI（画面遷移系）パラメータもリセットするかどうか
     */
    void Reset(bool clearPostUI);
    void Reset() override { Reset(true); }

    bool RequiresSeparatePass() const override {
        return false;
    }
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

    /**
     * @brief バッチおよびマスクフラグを直接指定してパスを構築する
     */
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen,
                        const std::vector<PostProcessManager::Mode>& batch, bool useMask);

    /**
     * @brief 指定したバッチ情報を用いて描画を実行する
     */
    void RenderWithBatch(const PostProcessRenderContext& context, const std::vector<PostProcessManager::Mode>& batch);

    /**
     * @brief エフェクトなしの画面コピー（Final Pass）を描画する
     */
    static void RenderFinalCopyPass(const PostProcessRenderContext& context);

    // --- シングルパスパラメータ アクセサ ---
    ToneMappingParams& GetToneMappingParams() { return toneMappingParams_; }
    const ToneMappingParams& GetToneMappingParams() const { return toneMappingParams_; }
    void SetToneMappingParams(const ToneMappingParams& p) { toneMappingParams_ = p; }

    HSVParams& GetHSVParams() { return hsvParams_; }
    const HSVParams& GetHSVParams() const { return hsvParams_; }
    void SetHSVParams(const HSVParams& p) { hsvParams_ = p; }

    VignetteParams& GetVignetteParams() { return vignetteParams_; }
    const VignetteParams& GetVignetteParams() const { return vignetteParams_; }
    void SetVignetteParams(const VignetteParams& p) { vignetteParams_ = p; }

    OutlineParams& GetOutlineParams() { return outlineParams_; }
    const OutlineParams& GetOutlineParams() const { return outlineParams_; }
    void SetOutlineParams(const OutlineParams& p) { outlineParams_ = p; }

    RadialBlurParams& GetRadialBlurParams() { return radialBlurParams_; }
    const RadialBlurParams& GetRadialBlurParams() const { return radialBlurParams_; }

    FadeParams& GetFadeParams() { return fadeParams_; }
    const FadeParams& GetFadeParams() const { return fadeParams_; }

    SlideParams& GetSlideParams() { return slideParams_; }
    const SlideParams& GetSlideParams() const { return slideParams_; }

    DissolveParams& GetDissolveParams() { return dissolveParams_; }
    const DissolveParams& GetDissolveParams() const { return dissolveParams_; }

    NoiseParams& GetNoiseParams() { return noiseParams_; }
    const NoiseParams& GetNoiseParams() const { return noiseParams_; }

    GlitchParams& GetGlitchParams() { return glitchParams_; }
    const GlitchParams& GetGlitchParams() const { return glitchParams_; }

    LuminanceOutlineParams& GetLuminanceOutlineParams() { return luminanceOutlineParams_; }
    const LuminanceOutlineParams& GetLuminanceOutlineParams() const { return luminanceOutlineParams_; }

    PixelationParams& GetPixelationParams() { return pixelationParams_; }
    const PixelationParams& GetPixelationParams() const { return pixelationParams_; }

    PointillismParams& GetPointillismParams() { return pointillismParams_; }
    const PointillismParams& GetPointillismParams() const { return pointillismParams_; }

    PosterizationParams& GetPosterizationParams() { return posterizationParams_; }
    const PosterizationParams& GetPosterizationParams() const { return posterizationParams_; }

    NightVisionParams& GetNightVisionParams() { return nightVisionParams_; }
    const NightVisionParams& GetNightVisionParams() const { return nightVisionParams_; }

    KaleidoscopeParams& GetKaleidoscopeParams() { return kaleidoscopeParams_; }
    const KaleidoscopeParams& GetKaleidoscopeParams() const { return kaleidoscopeParams_; }

    ChromaticAberrationParams& GetChromaticAberrationParams() { return chromaticAberrationParams_; }
    const ChromaticAberrationParams& GetChromaticAberrationParams() const { return chromaticAberrationParams_; }

    DisplacementMapParams& GetDisplacementMapParams() { return displacementMapParams_; }
    const DisplacementMapParams& GetDisplacementMapParams() const { return displacementMapParams_; }

    DirectionalBlurParams& GetDirectionalBlurParams() { return directionalBlurParams_; }
    const DirectionalBlurParams& GetDirectionalBlurParams() const { return directionalBlurParams_; }

    HalftoneParams& GetHalftoneParams() { return halftoneParams_; }
    const HalftoneParams& GetHalftoneParams() const { return halftoneParams_; }

    DepthOfFieldParams& GetDepthOfFieldParams() { return dofParams_; }
    const DepthOfFieldParams& GetDepthOfFieldParams() const { return dofParams_; }

    SmoothingParams& GetSmoothingParams() { return smoothingParams_; }
    const SmoothingParams& GetSmoothingParams() const { return smoothingParams_; }

    GaussianParams& GetGaussianParams() { return gaussianParams_; }
    const GaussianParams& GetGaussianParams() const { return gaussianParams_; }

    GravityDistortionParams& GetGravityDistortionParams() { return gravityDistortionParams_; }
    const GravityDistortionParams& GetGravityDistortionParams() const { return gravityDistortionParams_; }

    BlackHoleSupernovaParams& GetBlackHoleSupernovaParams() { return blackHoleSupernovaParams_; }
    const BlackHoleSupernovaParams& GetBlackHoleSupernovaParams() const { return blackHoleSupernovaParams_; }

    /**
     * @brief 自身が保持するシングルパスパラメータをマネージャーの CombinedParams に流し込む
     */
    void SyncParamsToCombined(CombinedParams& combined) const;

private:
    std::vector<PostProcessManager::Mode> batch_;

    ToneMappingParams toneMappingParams_{};
    HSVParams hsvParams_{};
    VignetteParams vignetteParams_{};
    OutlineParams outlineParams_{};
    RadialBlurParams radialBlurParams_{};
    DissolveParams dissolveParams_{};
    NoiseParams noiseParams_{};
    GlitchParams glitchParams_{};
    LuminanceOutlineParams luminanceOutlineParams_{};
    PixelationParams pixelationParams_{};
    PointillismParams pointillismParams_{};
    PosterizationParams posterizationParams_{};
    NightVisionParams nightVisionParams_{};
    KaleidoscopeParams kaleidoscopeParams_{};
    ChromaticAberrationParams chromaticAberrationParams_{};
    DisplacementMapParams displacementMapParams_{};
    DirectionalBlurParams directionalBlurParams_{};
    HalftoneParams halftoneParams_{};
    DepthOfFieldParams dofParams_{};
    SmoothingParams smoothingParams_{};
    GaussianParams gaussianParams_{};
    FadeParams fadeParams_{};
    SlideParams slideParams_{};
    GravityDistortionParams gravityDistortionParams_{};
    BlackHoleSupernovaParams blackHoleSupernovaParams_{};
};

} // namespace Irufemi
