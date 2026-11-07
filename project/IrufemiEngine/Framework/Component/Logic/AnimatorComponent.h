#pragma once

#include "Framework/Component/Component.h"
#include "Resource/Model/Animator.h"
#include <memory>
#include <string>

/**
 * @class AnimatorComponent
 * @brief エディタ対応のアニメーション制御コンポーネント。
 * 同じ GameObject 内の SkinnedMeshRendererComponent を探し、ポーズを流し込みます。
 */
class AnimatorComponent : public Component {
public:
    AnimatorComponent();
    ~AnimatorComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief シーン参加時にAnimatorの初期化を行います
     */
    void OnSpawned() override;
    /**
     * @brief Start を実行する。
     */
    void Start() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief Play を実行する。
     */
    void Play(const std::string& animationName, bool loop = true, float fadeDuration = -1.0f);

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "AnimatorComponent";
    }

    /**
     * @brief RawAnimator を取得する。
     * @return 取得された RawAnimator
     */
    Animator* GetRawAnimator() {
        return animator_.get();
    }

    /**
     * @brief デフォルトのクロスフェード時間を設定する
     * @param[in] duration フェード時間（秒）
     */
    void SetDefaultFadeDuration(float duration) {
        defaultFadeDuration_ = duration;
    }
    /**
     * @brief デフォルトのクロスフェード時間を取得する
     * @return フェード時間（秒）
     */
    float GetDefaultFadeDuration() const {
        return defaultFadeDuration_;
    }

private:
    std::unique_ptr<Animator> animator_;
    std::string defaultAnimation_ = "";
    std::string currentLoadedAnimation_ = "";
    float playbackSpeed_ = 1.0f;
    bool applyRootMotion_ = false;
    float defaultFadeDuration_ = 0.2f;

    class SkinnedMeshRendererComponent* cachedRenderer_ = nullptr;
};
