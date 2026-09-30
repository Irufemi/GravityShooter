#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <functional>
#include <vector>

/**
 * @enum PlayerFlightState
 * @brief 自機の飛行・機動ステート（一線級シューターのStateパターン）
 */
enum class PlayerFlightState {
    Idle,   ///< 静止ホバリング中（微小呼吸振動）
    Cruise, ///< 通常移動中（標準バンク・ピッチ）
    Boost,  ///< 急加速中（前傾姿勢、アフターバーナー点火）
    Brake,  ///< 急制動中（ノーズアップ、減速）
    Dying   ///< 撃破・制御不能
};

/**
 * @class RailShooterPlayerComponent
 * @brief レールシューティング用のプレイヤー制御コンポーネント（3軸姿勢制御・Stateパターン対応）
 */
class RailShooterPlayerComponent : public Component {
public:
    using StateChangeCallback = std::function<void(PlayerFlightState newState, PlayerFlightState oldState)>;
    using ThrottleChangeCallback = std::function<void(float throttle01)>;

    RailShooterPlayerComponent() = default;
    ~RailShooterPlayerComponent() override = default;

    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "RailShooterPlayerComponent";
    }

    /**
     * @brief 飛行ステート変更リスナーを登録する（Observerパターン）
     */
    void AddOnStateChangeListener(StateChangeCallback callback) {
        stateChangeListeners_.push_back(std::move(callback));
    }

    /**
     * @brief スロットル（速度比率 0.0〜1.0）変更リスナーを登録する（Observerパターン）
     */
    void AddOnThrottleChangeListener(ThrottleChangeCallback callback) {
        throttleChangeListeners_.push_back(std::move(callback));
    }

    /**
     * @brief 現在の飛行ステートを取得する
     */
    PlayerFlightState GetCurrentFlightState() const {
        return currentState_;
    }

    /**
     * @brief 飛行ステートを強制設定する（被弾・撃破演出連携用）
     */
    void SetState(PlayerFlightState newState);

private:
    void UpdateStateTransitions(float inputLen, float currentSpeed);

    PlayerFlightState currentState_ = PlayerFlightState::Idle;
    std::vector<StateChangeCallback> stateChangeListeners_;
    std::vector<ThrottleChangeCallback> throttleChangeListeners_;

    float xySpeed_ = 10.0f;                      ///< 上下左右に避ける（回避運動）スピード
    Irufemi::Vector3 currentOffset_ = {0, 0, 0}; ///< レールの中心からどのくらいずれているか（上下左右のズレ幅）

    // 画面内を動き回れる範囲（限界値）
    Irufemi::Vector3 moveLimitMin_ = {-10.0f, -10.0f, 0.0f}; ///< 移動できる限界の左下座標
    Irufemi::Vector3 moveLimitMax_ = {10.0f, 10.0f, 0.0f};   ///< 移動できる限界の右上座標

    // --- グラビティ操作・慣性・姿勢制御パラメータ ---
    Irufemi::Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};
    float acceleration_ = 150.0f; // 急発進の加速度
    float friction_ = 10.0f;      // 急制動の摩擦係数
    float maxSpeed_ = 15.0f;      // 最高速度

    // --- 3軸姿勢制御パラメータ（Pitch / Yaw / Roll） ---
    float rollAngle_ = 0.0f;    ///< 現在のロール角（Z軸: 左右の傾き）
    float maxRollAngle_ = 0.8f; ///< 最大ロール角度（約45度）

    float pitchAngle_ = 0.0f;     ///< 現在のピッチ角（X軸: 上下の傾き）
    float maxPitchAngle_ = 0.40f; ///< 最大ピッチ角度（約23度: 上昇時にノーズアップ）

    float yawAngle_ = 0.0f;     ///< 現在のヨー角（Y軸: 旋回方向への首振り）
    float maxYawAngle_ = 0.20f; ///< 最大ヨー角度（約11度: 旋回スリップ）

    float hoverTimer_ = 0.0f;      ///< 浮遊アニメーション用の経過時間
    float hoverAmplitude_ = 0.25f; ///< 浮遊の揺れ幅
    float hoverFrequency_ = 2.0f;  ///< 浮遊の揺れ速度
};
