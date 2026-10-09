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

void StageIntroDirectorComponent::SetIntroState(StageIntroState newState) {
    introState_ = newState;
    stateTimer_ = 0.0f;

    struct PhaseConfig {
        float StageIntroDirectorComponent::* durationMember;
        float initialThrusterScale;
        float targetThrusterScale;
        bool isPositionInterpolated;
        bool isSnapPositionOnEnter;
        bool isHudActive;
        bool isInputEnabled;
        void (*onEnterHook)(StageIntroDirectorComponent*);
        void (*onUpdateHook)(StageIntroDirectorComponent*, float progress);
    };

    static const std::unordered_map<StageIntroState, PhaseConfig> kPhaseConfigs = {
        { StageIntroState::WarpIn, {
            &StageIntroDirectorComponent::warpInDuration_,
            2.5f, 2.5f,
            true,  false,
            false, false,
            nullptr,
            nullptr
        }},
        { StageIntroState::Arrival, {
            &StageIntroDirectorComponent::arrivalDuration_,
            2.5f, 0.8f,
            false, true,
            false, false,
            nullptr,
            [](StageIntroDirectorComponent* self, float progress) {
                self->ApplyArrivalInertia(progress);
            }
        }},
        { StageIntroState::Active, {
            nullptr,
            0.8f, 0.8f,
            false, false,
            true,  true,
            [](StageIntroDirectorComponent* self) {
                self->TriggerFCSBootupSequence();
            },
            nullptr
        }},
    };

    auto it = kPhaseConfigs.find(introState_);
    if (it == kPhaseConfigs.end()) {
        return;
    }
    const auto& config = it->second;

    SetHUDActive(config.isHudActive);
    SetShipThrusterActive(true);
    SetShipThrusterScale(config.initialThrusterScale, true);

    if (config.isSnapPositionOnEnter) {
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                transform->SetPosition(initialShipLocalPos_);
            }
        }
    }

    if (auto ship = shipObj_.lock()) {
        if (auto playerComp = ship->GetComponent<RailShooterPlayerComponent>()) {
            playerComp->SetInputEnabled(config.isInputEnabled);
        }
    }

    if (config.onEnterHook) {
        config.onEnterHook(this);
    }
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

    struct PhaseConfig {
        float StageIntroDirectorComponent::* durationMember;
        float initialThrusterScale;
        float targetThrusterScale;
        bool isPositionInterpolated;
        bool isSnapPositionOnEnter;
        bool isHudActive;
        bool isInputEnabled;
        void (*onEnterHook)(StageIntroDirectorComponent*);
        void (*onUpdateHook)(StageIntroDirectorComponent*, float progress);
    };

    static const std::unordered_map<StageIntroState, PhaseConfig> kPhaseConfigs = {
        { StageIntroState::WarpIn, {
            &StageIntroDirectorComponent::warpInDuration_,
            2.5f, 2.5f,
            true,  false,
            false, false,
            nullptr,
            nullptr
        }},
        { StageIntroState::Arrival, {
            &StageIntroDirectorComponent::arrivalDuration_,
            2.5f, 0.8f,
            false, true,
            false, false,
            nullptr,
            [](StageIntroDirectorComponent* self, float progress) {
                self->ApplyArrivalInertia(progress);
            }
        }},
        { StageIntroState::Active, {
            nullptr,
            0.8f, 0.8f,
            false, false,
            true,  true,
            [](StageIntroDirectorComponent* self) {
                self->TriggerFCSBootupSequence();
            },
            nullptr
        }},
    };

    auto it = kPhaseConfigs.find(introState_);
    if (it == kPhaseConfigs.end()) {
        return;
    }
    const auto& config = it->second;

    // 所要時間はインスペクタープロパティから動的に取得（コード内ハードコードを完全排除）
    float duration = 1.0f;
    if (config.durationMember) {
        duration = (std::max)(0.001f, this->*(config.durationMember));
    }

    float progress = std::clamp(stateTimer_ / duration, 0.0f, 1.0f);

    // スラスター炎の補間
    float thrusterScale = std::lerp(config.initialThrusterScale, config.targetThrusterScale, progress);
    SetShipThrusterScale(thrusterScale, true);

    // 位置補間（WarpInフェーズの3次急減速）
    if (config.isPositionInterpolated) {
        float t = 1.0f - std::pow(1.0f - progress, 3.0f);
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                Irufemi::Vector3 curPos = initialShipLocalPos_;
                curPos.z = initialShipLocalPos_.z + startOffsetZ_ * (1.0f - t);
                transform->SetPosition(curPos);
            }
        }
    }

    // 固有演出フック
    if (config.onUpdateHook) {
        config.onUpdateHook(this, progress);
    }

    // 次のフェーズへ自動遷移
    if (progress >= 1.0f) {
        if (introState_ == StageIntroState::WarpIn) {
            SetIntroState(StageIntroState::Arrival);
        } else if (introState_ == StageIntroState::Arrival) {
            SetIntroState(StageIntroState::Active);
        }
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
