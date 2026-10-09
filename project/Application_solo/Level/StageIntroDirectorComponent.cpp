#include "StageIntroDirectorComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "UI/UISound.h"
#include <algorithm>
#include <cmath>

void StageIntroDirectorComponent::Initialize() {
    introState_ = StageIntroState::WarpIn;
    stateTimer_ = 0.0f;
    hasInitializedShip_ = false;

    // 初期化時点でHUDを即座に非表示化
    SetHUDActive(false);
}

void StageIntroDirectorComponent::Start() {
    // 全コンポーネントのStartフェーズで先行して自機をワープイン開始地点へ配置し、
    // 定位置（Z=0）でのスラスター放出やHUD表示を未然に遮断する
    EnsureShipInitialized();
}

void StageIntroDirectorComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("WarpIn Duration", &warpInDuration_);
    RegisterProperty("Arrival Duration", &arrivalDuration_);
    RegisterProperty("Start Offset Z", &startOffsetZ_);
    RegisterProperty("Target Ship Name", &targetShipName_);
}

bool StageIntroDirectorComponent::EnsureShipInitialized() {
    if (hasInitializedShip_) {
        return true;
    }

    auto scene = GetScene();
    if (!scene) {
        return false;
    }

    auto ship = scene->FindGameObject(targetShipName_);
    if (!ship) {
        return false;
    }

    shipObj_ = ship;
    auto transform = ship->GetTransform();
    if (!transform) {
        return false;
    }

    initialShipLocalPos_ = transform->GetPosition();

    // 自機を入力無効化し、初期位置（カメラ後方）へオフセット
    if (auto playerComp = ship->GetComponent<RailShooterPlayerComponent>()) {
        playerComp->SetInputEnabled(false);
    }

    Irufemi::Vector3 startPos = initialShipLocalPos_;
    startPos.z += startOffsetZ_;
    transform->SetPosition(startPos);
    transform->CheckAndComputeMatrix();

    SetHUDActive(false);
    SetShipThrusterScale(2.5f, true); // 高速突入用の超高出力ブースト炎（2.5倍）を展開
    SetShipThrusterActive(true);      // 突入開始位置（後方）でスラスター点火

    hasInitializedShip_ = true;
    return true;
}

void StageIntroDirectorComponent::Update() {
    // 演出完了済みの場合は処理をスキップ
    if (introState_ == StageIntroState::Active) {
        return;
    }

    if (!hasInitializedShip_) {
        if (!EnsureShipInitialized()) {
            return;
        }
    }

    auto engine = GetEngine();
    float deltaTime = engine ? engine->GetDeltaTime() : (1.0f / 60.0f);
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    stateTimer_ += deltaTime;
    OnUpdateIntroState(introState_, deltaTime);
}

void StageIntroDirectorComponent::SetIntroState(StageIntroState newState) {
    introState_ = newState;
    stateTimer_ = 0.0f;
    OnEnterIntroState(newState);
}

void StageIntroDirectorComponent::OnEnterIntroState(StageIntroState state) {
    using EnterAction = void (*)(StageIntroDirectorComponent*);
    static const std::unordered_map<StageIntroState, EnterAction> kEnterHandlers = {
        { StageIntroState::WarpIn, [](StageIntroDirectorComponent* self) {
            self->SetHUDActive(false);
            self->SetShipThrusterActive(true);
            self->SetShipThrusterScale(2.5f, true);
        }},
        { StageIntroState::Arrival, [](StageIntroDirectorComponent* self) {
            if (auto ship = self->shipObj_.lock()) {
                if (auto transform = ship->GetTransform()) {
                    transform->SetPosition(self->initialShipLocalPos_);
                }
            }
            self->SetShipThrusterActive(true);
        }},
        { StageIntroState::Active, [](StageIntroDirectorComponent* self) {
            self->SetHUDActive(true);
            self->SetShipThrusterActive(true);
            self->SetShipThrusterScale(0.8f, false);
            if (auto ship = self->shipObj_.lock()) {
                if (auto playerComp = ship->GetComponent<RailShooterPlayerComponent>()) {
                    playerComp->SetInputEnabled(true);
                }
            }
            self->TriggerFCSBootupSequence();
        }},
    };

    if (auto it = kEnterHandlers.find(state); it != kEnterHandlers.end()) {
        it->second(this);
    }
}

void StageIntroDirectorComponent::OnUpdateIntroState(StageIntroState state, float deltaTime) {
    using UpdateAction = void (*)(StageIntroDirectorComponent*, float);
    static const std::unordered_map<StageIntroState, UpdateAction> kUpdateHandlers = {
        { StageIntroState::WarpIn, [](StageIntroDirectorComponent* self, float /*dt*/) {
            float duration = self->warpInDuration_ > 0.0f ? self->warpInDuration_ : 1.0f;
            float progress = std::clamp(self->stateTimer_ / duration, 0.0f, 1.0f);
            float t = 1.0f - std::pow(1.0f - progress, 3.0f);

            if (auto ship = self->shipObj_.lock()) {
                if (auto transform = ship->GetTransform()) {
                    Irufemi::Vector3 curPos = self->initialShipLocalPos_;
                    curPos.z = self->initialShipLocalPos_.z + self->startOffsetZ_ * (1.0f - t);
                    transform->SetPosition(curPos);
                }
            }

            if (progress >= 1.0f) {
                self->SetIntroState(StageIntroState::Arrival);
            }
        }},
        { StageIntroState::Arrival, [](StageIntroDirectorComponent* self, float /*dt*/) {
            float duration = self->arrivalDuration_ > 0.0f ? self->arrivalDuration_ : 0.5f;
            float progress = std::clamp(self->stateTimer_ / duration, 0.0f, 1.0f);

            float thrusterScale = std::lerp(2.5f, 0.8f, progress);
            self->SetShipThrusterScale(thrusterScale, true);
            self->ApplyArrivalInertia(progress);

            if (progress >= 1.0f) {
                self->SetIntroState(StageIntroState::Active);
            }
        }},
        { StageIntroState::Active, [](StageIntroDirectorComponent* /*self*/, float /*dt*/) {
            // 通常戦闘時は更新処理なし
        }},
    };

    if (auto it = kUpdateHandlers.find(state); it != kUpdateHandlers.end()) {
        it->second(this, deltaTime);
    }
}

void StageIntroDirectorComponent::ApplyArrivalInertia(float /*progress*/) {
    // 【将来拡張用フック: B案】
    // 急制動時のノーズダイブ（ピッチ傾斜）と整流バネ挙動（Damped Oscillation）
    // 計算式: pitchOffset = maxPitch * exp(-decay * progress) * sin(omega * progress)
    // 専用パラメータやアニメーション調整の準備完了時に実装を解禁
}

void StageIntroDirectorComponent::TriggerFCSBootupSequence() {
    // 【将来拡張用フック: B案】
    // FCS（火器管制システム）起動演出
    // 1. 専用FCSオンラインSEの再生（高周波サイバネティックチャイム）
    // 2. ReticleUIComponentに対するスプリング展開アニメーション（拡大->定位置収縮）トリガー
}

void StageIntroDirectorComponent::SetHUDActive(bool active) {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    if (auto reticle = scene->FindGameObject("Reticle")) {
        reticle->SetActive(active);
    }
    if (auto lockon = scene->FindGameObject("LockonMarkerUI")) {
        lockon->SetActive(active);
    }
}

void StageIntroDirectorComponent::SetShipThrusterActive(bool active) {
    if (auto ship = shipObj_.lock()) {
        if (auto thrusterComp = ship->GetComponent<PlayerThrusterVisualizerComponent>()) {
            thrusterComp->SetThrusterActive(active);
        }
    }
}

void StageIntroDirectorComponent::SetShipThrusterScale(float targetScaleZ, bool snap) {
    if (auto ship = shipObj_.lock()) {
        if (auto thrusterComp = ship->GetComponent<PlayerThrusterVisualizerComponent>()) {
            if (snap) {
                thrusterComp->SetCurrentScaleZ(targetScaleZ);
            } else {
                thrusterComp->SetTargetScaleZ(targetScaleZ);
            }
        }
    }
}
