#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Player/PlayerHealthComponent.h"
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
    isThrusterActive_ = false;
    healthComp_ = nullptr;
}

void PlayerThrusterVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    // 自機ヘルスコンポーネントをキャッシュ（生存状態監視用）
    healthComp_ = gameObject_->GetComponent<PlayerHealthComponent>();

    // 1. EffectManager経由でノズル位置にスラスターエフェクトをアタッチ (SpawnAttached方式)
    if (auto fxMgr = EffectManagerComponent::GetInstance()) {
        auto thruster = fxMgr->PlayAttachedEffect("Thruster", gameObject_->shared_from_this(), nozzleOffset_);
        if (thruster) {
            thrusterObj_ = thruster;
            if (!isThrusterActive_) {
                thruster->SetIsActive(false);
                auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
                for (auto pe : emitters) {
                    pe->Stop();
                }
            }
        }
    }

    // 2. RailShooterPlayerComponent のイベントを購読 (Observer パターン)
    if (auto playerComp = gameObject_->GetComponent<RailShooterPlayerComponent>()) {
        playerComp->AddOnThrottleChangeListener([this](float throttle) { OnThrottleChanged(throttle); });
        playerComp->AddOnStateChangeListener(
            [this](PlayerFlightState newState, PlayerFlightState oldState) { OnStateChanged(newState, oldState); });
    }
}

void PlayerThrusterVisualizerComponent::OnDestroy() {
    isThrusterActive_ = false;
    if (auto thruster = thrusterObj_.lock()) {
        auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto pe : emitters) {
            pe->Stop();
        }
        thruster->Destroy();
        thrusterObj_.reset();
    }
}

void PlayerThrusterVisualizerComponent::SetThrusterActive(bool active) {
    isThrusterActive_ = active;
    if (auto thruster = thrusterObj_.lock()) {
        if (!active) {
            // 1. 全エミッターを停止（Stop()内で即座にGPUバッファへemit=0をフラッシュ）
            auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
            for (auto pe : emitters) {
                pe->Stop();
            }

            // 2. スラスター本体および全子ノードのスケールを即座にゼロへ潰し、描画を視覚的にも完全遮断
            if (auto t = thruster->GetComponent<TransformComponent>()) {
                t->SetScale({0.0f, 0.0f, 0.0f});
                t->CheckAndComputeMatrix();
            }
            auto allTransforms = thruster->GetComponentsInChildren<TransformComponent>();
            for (auto childT : allTransforms) {
                if (childT) {
                    childT->SetScale({0.0f, 0.0f, 0.0f});
                    childT->CheckAndComputeMatrix();
                }
            }
            currentScaleZ_ = 0.0f;
            targetScaleZ_ = 0.0f;

            // 3. 自機撃破・死亡時は GameObject 自体を即座にシーンから完全破棄（業界標準のライフサイクル保証）
            if (healthComp_ && healthComp_->IsDead()) {
                thruster->Destroy();
                thrusterObj_.reset();
            } else {
                thruster->SetIsActive(false);
            }
        } else {
            thruster->SetIsActive(true);
            if (auto t = thruster->GetComponent<TransformComponent>()) {
                t->SetScale({1.0f, 1.0f, (std::max)(currentScaleZ_, minScaleZ_)});
                t->CheckAndComputeMatrix();
            }
            auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
            for (auto pe : emitters) {
                pe->Restart(false);
            }
        }
    }
}

void PlayerThrusterVisualizerComponent::OnThrottleChanged(float throttle) {
    // スロットル（0.0〜1.0）に応じて目標の炎伸長スケールを決定
    targetScaleZ_ = std::lerp(minScaleZ_, maxScaleZ_, throttle);
}

void PlayerThrusterVisualizerComponent::OnStateChanged(PlayerFlightState newState, PlayerFlightState oldState) {
    (void)oldState;
    if (newState == PlayerFlightState::Dying) {
        // 撃破時はスラスターを即座に停止・完全消火
        SetThrusterActive(false);
    } else if (newState == PlayerFlightState::Boost) {
        targetScaleZ_ = maxScaleZ_ * 1.35f; // ブースト時はさらに35%伸長
    }
}

void PlayerThrusterVisualizerComponent::Update() {
    // 自機が存在しない、非アクティブ、または親が休眠している場合は即座に完全停止
    if (!gameObject_ || !gameObject_->GetIsActive()) {
        if (isThrusterActive_) {
            SetThrusterActive(false);
        }
        return;
    }

    // 自機が死亡・撃破状態（PlayerHealthComponent::IsDead）なら自動的に完全消火
    if (healthComp_ && healthComp_->IsDead()) {
        if (isThrusterActive_) {
            SetThrusterActive(false);
        }
        return;
    }

    if (!isThrusterActive_) {
        return;
    }

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
