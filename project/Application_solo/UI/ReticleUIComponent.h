#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>

class PlayerTargetingComponent;
class Primitive2DRendererComponent;

/**
 * @class ReticleUIComponent
 * @brief ゲームシーンにおける照準（レティクル）の統合制御および手応え（Juice）演出コンポーネント
 */
class ReticleUIComponent : public Component {
public:
    ReticleUIComponent() = default;
    ~ReticleUIComponent() override = default;

    void Initialize() override;
    void Update() override;

    /**
     * @brief FCSブート時のスプリング展開アニメーションを発火する
     */
    void TriggerBootupAnimation();

    std::string GetComponentName() const override {
        return "ReticleUIComponent";
    }

private:
    PlayerTargetingComponent* targetingComp_ = nullptr;
    Primitive2DRendererComponent* primitiveRenderer_ = nullptr;

    float currentScale_ = 1.0f;
    float pulseTimer_ = 0.0f;
    bool wasHovering_ = false;

    // FCSブートアップ展開アニメーション
    bool isBootingUp_ = false;
    float bootupTimer_ = 0.0f;
    const float kBootupDuration_ = 0.35f;

    const float kMagnetFriction_ = 0.5f; //!< 敵ホバー時のエイム吸着摩擦
};
