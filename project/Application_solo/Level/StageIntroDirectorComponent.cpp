#include "StageIntroDirectorComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Audio/AudioManager.h"
#include "Input/GameAction.h"
#include "Platform/Input/InputManager.h"
#include "UI/UISound.h"
#include <algorithm>
#include <cmath>

StageIntroDirectorComponent::~StageIntroDirectorComponent() {
    CleanupWarpVisuals();
}

void StageIntroDirectorComponent::OnDestroy() {
    CleanupWarpVisuals();
}

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
    RegisterProperty("Bootup Duration", &bootupDuration_);
    RegisterProperty("Start Offset Z", &startOffsetZ_);
    RegisterProperty("WarpIn FOV Boost", &warpInFovBoostDeg_);
    RegisterProperty("WarpIn Blur Width", &maxWarpBlurWidth_);
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

    // カメラの取得とベースFOVのキャッシュ
    if (cameraObj_.expired()) {
        if (auto camObj = scene->FindGameObject("MainCamera")) {
            cameraObj_ = camObj;
            if (auto cam = camObj->GetComponent<CameraComponent>()) {
                baseCameraFov_ = cam->GetFovAngleY();
            }
        }
    }

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

    // シーン突入開始の瞬間（フェードイン中・第1フレーム）から最大広角とラジアルブラーを先行点火！
    UpdateWarpVisuals(0.0f);

    hasInitializedShip_ = true;
    return true;
}

void StageIntroDirectorComponent::SetIntroState(StageIntroState newState) {
    introState_ = newState;
    stateTimer_ = 0.0f;

    struct PhaseConfig {
        float StageIntroDirectorComponent::*durationMember;
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
        {StageIntroState::WarpIn,
         {&StageIntroDirectorComponent::warpInDuration_, 2.5f, 2.5f, true, false, false, false, nullptr, nullptr}},
        {StageIntroState::Arrival,
         {&StageIntroDirectorComponent::arrivalDuration_, 2.5f, 0.8f, false, true, false, false, nullptr,
          [](StageIntroDirectorComponent* self, float progress) { self->ApplyArrivalInertia(progress); }}},
        {StageIntroState::Bootup,
         {&StageIntroDirectorComponent::bootupDuration_, 0.8f, 0.8f, false, true, true, false,
          [](StageIntroDirectorComponent* self) { self->TriggerFCSBootupSequence(); }, nullptr}},
        {StageIntroState::Active, {nullptr, 0.8f, 0.8f, false, false, true, true, nullptr, nullptr}},
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

    // プレイヤーによるクイックスキップ判定 (決定キー / SPACE / クリックで即Activeへスナップ)
    if (engine) {
        if (auto input = engine->GetInputManager()) {
            if (InputHelper::IsActionPressed(input, GameAction::UI_Submit) || input->IsCursorActionPressed()) {
                SkipIntroSequence();
                return;
            }
        }
    }

    stateTimer_ += deltaTime;

    struct PhaseConfig {
        float StageIntroDirectorComponent::*durationMember;
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
        {StageIntroState::WarpIn,
         {&StageIntroDirectorComponent::warpInDuration_, 2.5f, 2.5f, true, false, false, false, nullptr, nullptr}},
        {StageIntroState::Arrival,
         {&StageIntroDirectorComponent::arrivalDuration_, 2.5f, 0.8f, false, true, false, false, nullptr,
          [](StageIntroDirectorComponent* self, float progress) { self->ApplyArrivalInertia(progress); }}},
        {StageIntroState::Bootup,
         {&StageIntroDirectorComponent::bootupDuration_, 0.8f, 0.8f, false, true, true, false,
          [](StageIntroDirectorComponent* self) { self->TriggerFCSBootupSequence(); }, nullptr}},
        {StageIntroState::Active, {nullptr, 0.8f, 0.8f, false, false, true, true, nullptr, nullptr}},
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

    // 位置補間（WarpInフェーズの3次急減速）および視覚ワーピング連動
    if (config.isPositionInterpolated) {
        float t = 1.0f - std::pow(1.0f - progress, 3.0f);
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                Irufemi::Vector3 curPos = initialShipLocalPos_;
                curPos.z = initialShipLocalPos_.z + startOffsetZ_ * (1.0f - t);
                transform->SetPosition(curPos);
            }
        }

        // カメラFOVワーピングとPreUIラジアルブラーの動的駆動
        UpdateWarpVisuals(progress);
    }

    // 固有演出フック
    if (config.onUpdateHook) {
        config.onUpdateHook(this, progress);
    }

    // 次のフェーズへ自動遷移
    if (progress >= 1.0f) {
        if (introState_ == StageIntroState::WarpIn) {
            CleanupWarpVisuals(); // 到着時に確実に等倍復帰
            SetIntroState(StageIntroState::Arrival);
        } else if (introState_ == StageIntroState::Arrival) {
            SetIntroState(StageIntroState::Bootup);
        } else if (introState_ == StageIntroState::Bootup) {
            SetIntroState(StageIntroState::Active);
        }
    }
}

void StageIntroDirectorComponent::ApplyArrivalInertia(float progress) {
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            // 重力エネルギー体の整流挙動: 急制動に伴うノーズダイブとお辞儀（Damped Oscillation）
            // progress: 0.0 -> 1.0 (arrivalDuration_ 0.50秒間)
            float decay = std::exp(-5.0f * progress);
            float oscillation = std::sin(progress * 3.14159265f * 2.5f);
            float pitchOffset = -0.15f * decay * oscillation; // 約-8度から滑らかに0へ収束

            auto rot = transform->GetRotation();
            rot.x = pitchOffset;
            transform->SetRotation(rot);
        }
    }
}

void StageIntroDirectorComponent::TriggerFCSBootupSequence() {
    // 1. 重力FCSオンライン起動チャイム再生 (澄んだ高周波サイバネ展開音)
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->PlayByFile("resources/audio/SE/se_fcs_online.mp3", 0.85f, 1.0f, AudioCategory::SE);
        }
    }

    // 2. 自機のピッチ角を完全な水平ホバリング姿勢へ安定化
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            auto rot = transform->GetRotation();
            rot.x = 0.0f;
            transform->SetRotation(rot);
        }
    }
}

void StageIntroDirectorComponent::SkipIntroSequence() {
    if (introState_ == StageIntroState::Active) {
        return;
    }

    // 視覚ワーピング（FOV/ラジアルブラー）を確実に基準状態へリセット
    CleanupWarpVisuals();

    // 自機座標と回転を定位置・水平ホバリングへ即座にスナップ
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            transform->SetPosition(initialShipLocalPos_);
            auto rot = transform->GetRotation();
            rot.x = 0.0f;
            transform->SetRotation(rot);
        }
    }

    // スラスター炎を通常戦闘スケールへスナップ
    SetShipThrusterScale(0.8f, true);

    // 通常戦闘ステートへ即時移行 (HUD点灯・操作権委譲)
    SetIntroState(StageIntroState::Active);
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

CameraComponent* StageIntroDirectorComponent::GetCameraComponent() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetComponent<CameraComponent>();
    }
    return nullptr;
}

void StageIntroDirectorComponent::UpdateWarpVisuals(float progress) {
    // 突入初動(0.0〜0.25)は最大広角・ブラーを維持し、自機のブレーキ(0.25〜1.0)に合わせて滑らかに収束
    float curve = 1.0f;
    if (progress >= 0.25f) {
        float t = (progress - 0.25f) / 0.75f;
        curve = 0.5f * (1.0f + std::cos(t * 3.14159265f)); // 1.0 -> 0.0 (Smoothstep)
    }

    // 1. カメラFOV動的ブレンド (最大広角から等倍へ急制動収束)
    if (auto cam = GetCameraComponent()) {
        float fovBoostRad = curve * (warpInFovBoostDeg_ * 3.14159265f / 180.0f);
        cam->SetFovAngleY(baseCameraFov_ + fovBoostRad);
    }

    // 2. PreUI ラジアルブラー動的強度制御
    if (auto engine = GetEngine()) {
        if (auto ppm = engine->GetPostProcessManager()) {
            float blurWidth = maxWarpBlurWidth_ * curve;
            if (blurWidth > 0.001f) {
                if (!isRadialBlurActive_) {
                    ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PreUI);
                    isRadialBlurActive_ = true;
                }
                auto& rb = ppm->GetRadialBlurParams();
                rb.center = {0.5f, 0.5f};
                rb.blurWidth = blurWidth;
                rb.numSamples = 16;
            } else {
                CleanupWarpVisuals();
            }
        }
    }
}

void StageIntroDirectorComponent::CleanupWarpVisuals() {
    // カメラFOVを等倍に安全復帰
    if (auto cam = GetCameraComponent()) {
        cam->SetFovAngleY(baseCameraFov_);
    }

    // ラジアルブラーの確実な解除
    if (isRadialBlurActive_) {
        if (auto engine = GetEngine()) {
            if (auto ppm = engine->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
                auto& rb = ppm->GetRadialBlurParams();
                rb.blurWidth = 0.0f;
            }
        }
        isRadialBlurActive_ = false;
    }
}
