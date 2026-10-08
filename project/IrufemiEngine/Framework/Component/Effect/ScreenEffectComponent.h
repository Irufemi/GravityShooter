#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include <string>
#include <nlohmann/json.hpp>

/**
 * @class ScreenEffectComponent
 * @brief 画面全体への一時的なポストエフェクト演出（Vignette, Glitch等）を管理・ブレンドするコンポーネント
 */
class ScreenEffectComponent : public Component {
public:
    ScreenEffectComponent();
    ~ScreenEffectComponent() override;

    void Initialize() override;
    void Update() override;
    void OnDestroy() override;

    std::string GetComponentName() const override {
        return "ScreenEffectComponent";
    }

    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /// @brief 演出を開始する（Weight を 1.0 にし、時間経過で減衰させる）
    void Play();

    void SetMode(PostProcessMode mode) {
        mode_ = mode;
    }
    PostProcessMode GetMode() const {
        return mode_;
    }

    void SetDuration(float duration) {
        duration_ = duration;
    }
    float GetDuration() const {
        return duration_;
    }

    void SetTargetGlitchParams(const PostProcessManager::GlitchParams& params) {
        targetGlitchParams_ = params;
    }
    PostProcessManager::GlitchParams& GetTargetGlitchParams() {
        return targetGlitchParams_;
    }
    const PostProcessManager::GlitchParams& GetTargetGlitchParams() const {
        return targetGlitchParams_;
    }

    void SetTargetVignetteParams(const PostProcessManager::VignetteParams& params) {
        targetVignetteParams_ = params;
    }
    PostProcessManager::VignetteParams& GetTargetVignetteParams() {
        return targetVignetteParams_;
    }
    const PostProcessManager::VignetteParams& GetTargetVignetteParams() const {
        return targetVignetteParams_;
    }

    void SetTargetChromaticAberrationParams(const PostProcessManager::ChromaticAberrationParams& params) {
        targetChromaticAberrationParams_ = params;
    }
    PostProcessManager::ChromaticAberrationParams& GetTargetChromaticAberrationParams() {
        return targetChromaticAberrationParams_;
    }
    const PostProcessManager::ChromaticAberrationParams& GetTargetChromaticAberrationParams() const {
        return targetChromaticAberrationParams_;
    }

    void SetTargetRadialBlurParams(const PostProcessManager::RadialBlurParams& params) {
        targetRadialBlurParams_ = params;
    }
    PostProcessManager::RadialBlurParams& GetTargetRadialBlurParams() {
        return targetRadialBlurParams_;
    }
    const PostProcessManager::RadialBlurParams& GetTargetRadialBlurParams() const {
        return targetRadialBlurParams_;
    }

private:
    PostProcessMode mode_ = PostProcessMode::None;
    float duration_ = 0.3f;
    float currentWeight_ = 0.0f;
    bool isPlaying_ = false;
    bool wasModeActiveBeforePlay_ = false;

    // 目標となるパラメータ群
    PostProcessManager::GlitchParams targetGlitchParams_;
    PostProcessManager::VignetteParams targetVignetteParams_;
    PostProcessManager::ChromaticAberrationParams targetChromaticAberrationParams_;
    PostProcessManager::RadialBlurParams targetRadialBlurParams_;

    // ベース（元の状態）のキャッシュ
    PostProcessManager::GlitchParams baseGlitchParams_;
    PostProcessManager::VignetteParams baseVignetteParams_;
    PostProcessManager::ChromaticAberrationParams baseChromaticAberrationParams_;
    PostProcessManager::RadialBlurParams baseRadialBlurParams_;
    bool isBaseCached_ = false;

    /// @brief エフェクトのパラメータ補間を適用する
    void ApplyEffectParams(PostProcessManager* ppm, float t);
    void UpdateGlitchParams(PostProcessManager* ppm, float t);
    void UpdateVignetteParams(PostProcessManager* ppm, float t);
    void UpdateChromaticAberrationParams(PostProcessManager* ppm, float t);
    void UpdateRadialBlurParams(PostProcessManager* ppm, float t);

    /// @brief エフェクト終了時にベースパラメータへ復元する
    void RestoreBaseParams(PostProcessManager* ppm);
};
