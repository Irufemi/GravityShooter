#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include <algorithm>
#include <cmath>

void RailShooterPlayerComponent::OnRegisterProperties() {
    RegisterProperty("XYSpeed", &xySpeed_);
    RegisterProperty("MoveLimitMin", &moveLimitMin_);
    RegisterProperty("MoveLimitMax", &moveLimitMax_);
    RegisterProperty("Acceleration", &acceleration_);
    RegisterProperty("Friction", &friction_);
    RegisterProperty("MaxSpeed", &maxSpeed_);
    RegisterProperty("MaxRollAngle", &maxRollAngle_);
    RegisterProperty("MaxPitchAngle", &maxPitchAngle_);
    RegisterProperty("MaxYawAngle", &maxYawAngle_);
    RegisterProperty("HoverAmplitude", &hoverAmplitude_);
    RegisterProperty("HoverFrequency", &hoverFrequency_);
}

void RailShooterPlayerComponent::SetState(PlayerFlightState newState) {
    if (currentState_ == newState) {
        return;
    }
    PlayerFlightState oldState = currentState_;
    currentState_ = newState;

    // リスナーへの通知（Observerパターン）
    for (const auto& listener : stateChangeListeners_) {
        if (listener) {
            listener(newState, oldState);
        }
    }
}

void RailShooterPlayerComponent::UpdateStateTransitions(float inputLen, float currentSpeed) {
    if (currentState_ == PlayerFlightState::Dying) {
        return; // 死亡状態は固定
    }

    PlayerFlightState nextState = currentState_;

    // 入力および速度に基づくステートマシン遷移判定
    if (inputLen > 0.8f && currentSpeed > maxSpeed_ * 0.80f) {
        nextState = PlayerFlightState::Boost;
    } else if (inputLen < 0.1f && currentSpeed > 3.0f) {
        nextState = PlayerFlightState::Brake;
    } else if (currentSpeed > 0.3f) {
        nextState = PlayerFlightState::Cruise;
    } else {
        nextState = PlayerFlightState::Idle;
    }

    if (nextState != currentState_) {
        SetState(nextState);
    }
}

void RailShooterPlayerComponent::Update() {
    if (!gameObject_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    // 1フレームの経過時間
    float deltaTime = engine->GetGameDeltaTime();
    if (deltaTime <= 0.0f) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    // 撃破状態（Dying）の場合は入力と通常姿勢制御をスキップ
    if (currentState_ == PlayerFlightState::Dying) {
        return;
    }

    // --- キー入力による上下左右の回避運動 ---
    auto* input = engine->GetInputManager();
    if (!input) {
        return;
    }
    Irufemi::Vector3 moveDir = {0.0f, 0.0f, 0.0f};

    // WASD または 矢印キーで移動方向を入力 (長押し判定)
    if (input->IsKeyDown('W') || input->IsKeyDown(VK_UP)) {
        moveDir.y += 1.0f;
    }
    if (input->IsKeyDown('S') || input->IsKeyDown(VK_DOWN)) {
        moveDir.y -= 1.0f;
    }
    if (input->IsKeyDown('A') || input->IsKeyDown(VK_LEFT)) {
        moveDir.x -= 1.0f;
    }
    if (input->IsKeyDown('D') || input->IsKeyDown(VK_RIGHT)) {
        moveDir.x += 1.0f;
    }

    // ゲームパッド（左スティック）の入力
    float padX = input->GetLeftStickX();
    float padY = input->GetLeftStickY();
    if (std::abs(padX) > 0.1f) {
        moveDir.x += padX;
    }
    if (std::abs(padY) > 0.1f) {
        moveDir.y += padY;
    }

    // 斜め移動したときに移動速度が速くならないように、ベクトルの長さを1に抑える
    float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
    if (len > 1.0f) {
        moveDir.x /= len;
        moveDir.y /= len;
        len = 1.0f;
    }

    // --- 慣性と速度の計算 ---
    if (len > 0.1f) {
        // 入力がある場合、加速度を足して加速
        currentVelocity_.x += moveDir.x * acceleration_ * deltaTime;
        currentVelocity_.y += moveDir.y * acceleration_ * deltaTime;

        // 最高速度でクリップ
        float vLen = std::sqrt(currentVelocity_.x * currentVelocity_.x + currentVelocity_.y * currentVelocity_.y);
        if (vLen > maxSpeed_) {
            currentVelocity_.x = (currentVelocity_.x / vLen) * maxSpeed_;
            currentVelocity_.y = (currentVelocity_.y / vLen) * maxSpeed_;
        }
    } else {
        // 入力がない場合、摩擦（減衰）で急制動（フレームレート完全非依存の指数減衰）
        float decay = std::exp(-friction_ * deltaTime);
        currentVelocity_.x *= decay;
        currentVelocity_.y *= decay;
    }

    // レール中心からのズレ幅（オフセット値）を更新
    currentOffset_.x += currentVelocity_.x * deltaTime;
    currentOffset_.y += currentVelocity_.y * deltaTime;

    // 指定した画面内の限界範囲（クランプ範囲）を超えないように制限する
    currentOffset_.x = std::clamp(currentOffset_.x, moveLimitMin_.x, moveLimitMax_.x);
    currentOffset_.y = std::clamp(currentOffset_.y, moveLimitMin_.y, moveLimitMax_.y);

    // --- 現在の速度とスロットル開度の算出 ---
    float currentSpeed = std::sqrt(currentVelocity_.x * currentVelocity_.x + currentVelocity_.y * currentVelocity_.y);
    float throttle = std::clamp(currentSpeed / maxSpeed_, 0.0f, 1.0f);

    // 1. ステート遷移の更新（Stateパターン）
    UpdateStateTransitions(len, currentSpeed);

    // 2. スロットル値のObserver通知（スラスターやカメラ用）
    for (const auto& listener : throttleChangeListeners_) {
        if (listener) {
            listener(throttle);
        }
    }

    // --- 3軸姿勢制御（Pitch / Yaw / Roll）の計算 ---
    // 1. ロール（左右移動時の動的バンク）
    float targetRoll = -(currentVelocity_.x / maxSpeed_) * maxRollAngle_;
    // 2. ピッチ（上下移動時のノーズアップ/ノーズダウン: 上昇時にノーズが上を向く）
    float targetPitch = -(currentVelocity_.y / maxSpeed_) * maxPitchAngle_;
    // 3. ヨー（左右移動時のスリップ角・首振り）
    float targetYaw = (currentVelocity_.x / maxSpeed_) * maxYawAngle_;

    // ブースト時は前傾姿勢を少し強調
    if (currentState_ == PlayerFlightState::Boost) {
        targetPitch += 0.05f;
    }

    // フレームレート非依存のLerp補間
    float rollPitchLerpSpeed = (currentState_ == PlayerFlightState::Boost) ? 14.0f : 10.0f;
    float rollPitchLerpFactor = 1.0f - std::exp(-rollPitchLerpSpeed * deltaTime);
    float yawLerpFactor = 1.0f - std::exp(-8.0f * deltaTime);

    rollAngle_ = std::lerp(rollAngle_, targetRoll, rollPitchLerpFactor);
    pitchAngle_ = std::lerp(pitchAngle_, targetPitch, rollPitchLerpFactor);
    yawAngle_ = std::lerp(yawAngle_, targetYaw, yawLerpFactor);

    // --- 多軸ホバリング（リサージュ曲線による有機的な呼吸感） ---
    hoverTimer_ += deltaTime * hoverFrequency_;
    float hoverY = std::sin(hoverTimer_) * hoverAmplitude_;
    float hoverPitchWobble = std::sin(hoverTimer_ * 0.85f) * 0.02f;
    float hoverRollWobble = std::cos(hoverTimer_ * 0.65f) * 0.025f;

    // --- ローカル座標・3軸回転の適用 ---
    transform->SetPosition({currentOffset_.x, currentOffset_.y + hoverY, 0.0f});
    transform->SetRotation({pitchAngle_ + hoverPitchWobble, yawAngle_, rollAngle_ + hoverRollWobble});
}
