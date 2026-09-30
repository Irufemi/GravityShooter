#include "Player/PlayerThrusterVisualizerComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Effects/EffectManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Core/System/IrufemiEngine.h"
#include <algorithm>
#include <cmath>

void PlayerThrusterVisualizerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Nozzle Offset", &nozzleOffset_);
    RegisterProperty("Min Scale Z", &minScaleZ_);
    RegisterProperty("Max Scale Z", &maxScaleZ_);
}

void PlayerThrusterVisualizerComponent::Initialize() {
    currentScaleZ_ = minScaleZ_;
    targetScaleZ_ = minScaleZ_;
}

void PlayerThrusterVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    // 1. EffectManager経由でノズル位置にスラスターエフェクトをアタッチ (SpawnAttached方式)
    if (auto fxMgr = EffectManagerComponent::GetInstance()) {
        auto thruster = fxMgr->PlayAttachedEffect("Thruster", gameObject_->shared_from_this(), nozzleOffset_);
        if (thruster) {
            thrusterObj_ = thruster;
        }
    }

    // 2. RailShooterPlayerComponent のイベントを購読 (Observer パターン)
    if (auto playerComp = gameObject_->GetComponent<RailShooterPlayerComponent>()) {
        playerComp->AddOnThrottleChangeListener([this](float throttle) { OnThrottleChanged(throttle); });
        playerComp->AddOnStateChangeListener(
            [this](PlayerFlightState newState, PlayerFlightState oldState) { OnStateChanged(newState, oldState); });
    }
}

void PlayerThrusterVisualizerComponent::OnThrottleChanged(float throttle) {
    // スロットル（0.0〜1.0）に応じて目標の炎伸長スケールを決定
    targetScaleZ_ = std::lerp(minScaleZ_, maxScaleZ_, throttle);
}

void PlayerThrusterVisualizerComponent::OnStateChanged(PlayerFlightState newState, PlayerFlightState oldState) {
    (void)oldState;
    if (auto thruster = thrusterObj_.lock()) {
        if (newState == PlayerFlightState::Dying) {
            // 撃破時はスラスターを即座に停止・非表示化
            thruster->SetIsActive(false);
        } else if (newState == PlayerFlightState::Boost) {
            targetScaleZ_ = maxScaleZ_ * 1.35f; // ブースト時はさらに35%伸長
        }
    }
}

void PlayerThrusterVisualizerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    // フレームレート非依存の滑らかなスケール補間
    float lerpFactor = 1.0f - std::exp(-12.0f * dt);
    currentScaleZ_ = std::lerp(currentScaleZ_, targetScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto transform = thruster->GetComponent<TransformComponent>()) {
            // ノズル口径（XY）は機体にジャストフィットさせ、後方の噴流長（Z）のみをスロットル連動で伸縮
            transform->SetScale({1.0f, 1.0f, currentScaleZ_});
        }
    }
}
