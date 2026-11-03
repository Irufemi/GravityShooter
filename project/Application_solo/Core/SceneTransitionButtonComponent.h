#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include "Framework/UI/UIAnimator.h"
#include "Core/Math/Vector3.h"

class ButtonComponent;
class TransformComponent;
class SpriteRendererComponent;

/**
 * @class SceneTransitionButtonComponent
 * @brief ButtonComponentのクリックを検知し、アニメーションとシーン遷移を行うゲーム固有コンポーネント
 */
class SceneTransitionButtonComponent : public Component {
public:
    SceneTransitionButtonComponent() = default;
    ~SceneTransitionButtonComponent() override = default;

    void OnAwake() override;
    void Initialize() override;
    void Update() override;

    std::string GetComponentName() const override {
        return "SceneTransitionButtonComponent";
    }
    void OnRegisterProperties() override;

    /**
     * @enum TransitionType
     * @brief シーン遷移エフェクトの種別
     */
    enum class TransitionType : int { Fade = 0, Dissolve = 1, Slide = 2, RadialBlur = 3 };

private:
    std::string onClickLoadScene_ = ""; // クリック時に自動で遷移するシーン名
    TransitionType transitionType_ = TransitionType::Fade;
    float transitionDuration_ = 1.0f; // トランジションにかける時間
    float transitionDelay_ = 0.8f;    // シーン遷移開始までの待機時間（デフォルトはアニメと同じ）
    float clickAnimDuration_ = 0.8f;  // クリックアニメーションの長さ（フラッシュなど）

    // --- 内部状態 ---
    UIAnimator animator_;

    bool isTransitionPending_ = false;
    float transitionTimer_ = 0.0f;
    Irufemi::Vector3 originalScale_ = {1.0f, 1.0f, 1.0f};

    ButtonComponent* button_ = nullptr;
    SpriteRendererComponent* sprite_ = nullptr;
};
