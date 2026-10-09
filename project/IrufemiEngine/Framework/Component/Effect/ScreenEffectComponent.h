#pragma once
#include "Framework/Component/Component.h"
#include "Framework/Component/Effect/ScreenEffectDrivers.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @class ScreenEffectComponent
 * @brief 画面全体への一時的なポストエフェクト演出（Vignette, Glitch等）を管理・ブレンドするコンポーネント
 * @details 内部処理は IScreenEffectDriver へ委譲され、完全なデータ駆動・Strategy パターンで動作します。
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

    void SetMode(PostProcessMode mode);
    PostProcessMode GetMode() const {
        return mode_;
    }

    void SetDuration(float duration) {
        duration_ = duration;
    }
    float GetDuration() const {
        return duration_;
    }

    // --- 既存 API 互換レイヤー (Driver 委譲) ---
    void SetTargetGlitchParams(const PostProcessManager::GlitchParams& params);
    PostProcessManager::GlitchParams& GetTargetGlitchParams();
    const PostProcessManager::GlitchParams& GetTargetGlitchParams() const;

    void SetTargetVignetteParams(const PostProcessManager::VignetteParams& params);
    PostProcessManager::VignetteParams& GetTargetVignetteParams();
    const PostProcessManager::VignetteParams& GetTargetVignetteParams() const;

    void SetTargetChromaticAberrationParams(const PostProcessManager::ChromaticAberrationParams& params);
    PostProcessManager::ChromaticAberrationParams& GetTargetChromaticAberrationParams();
    const PostProcessManager::ChromaticAberrationParams& GetTargetChromaticAberrationParams() const;

    void SetTargetRadialBlurParams(const PostProcessManager::RadialBlurParams& params);
    PostProcessManager::RadialBlurParams& GetTargetRadialBlurParams();
    const PostProcessManager::RadialBlurParams& GetTargetRadialBlurParams() const;

    IScreenEffectDriver* GetDriver() const {
        return driver_.get();
    }

private:
    PostProcessMode mode_ = PostProcessMode::None;
    float duration_ = 0.3f;
    float currentWeight_ = 0.0f;
    bool isPlaying_ = false;
    bool wasModeActiveBeforePlay_ = false;

    // 戦略ドライバ（エフェクト固有のパラメータ・補間計算・シリアライズをカプセル化）
    std::unique_ptr<IScreenEffectDriver> driver_;

    void EnsureDriver();
};
