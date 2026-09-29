#pragma once
#include "Framework/Component/Component.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include <memory>

class GameObject;

/**
 * @class PlayerThrusterVisualizerComponent
 * @brief 自機の推進スラスター演出を制御するコンポーネント（Observerパターンによるスロットル・ステート連動）
 */
class PlayerThrusterVisualizerComponent : public Component {
public:
    PlayerThrusterVisualizerComponent() = default;
    ~PlayerThrusterVisualizerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "PlayerThrusterVisualizerComponent";
    }

private:
    void OnThrottleChanged(float throttle);
    void OnStateChanged(PlayerFlightState newState, PlayerFlightState oldState);

private:
    std::weak_ptr<GameObject> thrusterObj_; ///< アタッチされたスラスターGameObject
    RailShooterPlayerComponent* playerComp_ = nullptr;

    Irufemi::Vector3 nozzleOffset_ = {0.0f, 0.0f, -0.48f}; ///< 機体ノズル相対座標
    float minScaleZ_ = 0.8f;   ///< アイドル時のスケール
    float maxScaleZ_ = 1.8f;   ///< 全力ブースト時のスケール
    float currentScaleZ_ = 1.0f;
    float targetScaleZ_ = 1.0f;
};
