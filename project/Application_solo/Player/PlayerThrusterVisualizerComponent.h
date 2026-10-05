#pragma once
#include "Framework/Component/Component.h"
#include <memory>

class GameObject;
enum class PlayerFlightState;

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

    /**
     * @brief スラスターエフェクトの表示/非表示（アクティブ状態）を明示的に切り替える
     * @param active trueなら点火・表示、falseなら消火・非表示
     */
    void SetThrusterActive(bool active);
    bool IsThrusterActive() const {
        return isThrusterActive_;
    }

    /**
     * @brief スラスターの目標伸長スケールを設定する（演出・ブースト用）
     * @param targetScaleZ 目標のZスケール
     */
    void SetTargetScaleZ(float targetScaleZ) {
        targetScaleZ_ = targetScaleZ;
    }

    /**
     * @brief スラスターの現在スケールを即座に上書き設定する
     * @param scaleZ 設定するZスケール
     */
    void SetCurrentScaleZ(float scaleZ) {
        currentScaleZ_ = scaleZ;
        targetScaleZ_ = scaleZ;
    }

private:
    /// @brief スロットル変更時のコールバック関数
    /// @param throttle スロットル開度 (0.0 ~ 1.0)
    void OnThrottleChanged(float throttle);

    /// @brief 飛行ステート変更時のコールバック関数
    /// @param newState 遷移後のステート
    /// @param oldState 遷移前のステート
    void OnStateChanged(PlayerFlightState newState, PlayerFlightState oldState);

private:
    std::weak_ptr<GameObject> thrusterObj_; ///< アタッチされたスラスターGameObject

    Irufemi::Vector3 nozzleOffset_ = {0.0f, 0.0f, -0.48f}; ///< 機体ノズル相対座標
    float minScaleZ_ = 0.8f;                               ///< アイドル時のスケール
    float maxScaleZ_ = 1.8f;                               ///< 全力ブースト時のスケール
    float currentScaleZ_ = 1.0f;                           ///< 現在のスラスターZスケール
    float targetScaleZ_ = 1.0f;                            ///< 目標のスラスターZスケール
    bool isThrusterActive_ = false; ///< スラスターの有効・無効状態（初期状態は待機）
};
