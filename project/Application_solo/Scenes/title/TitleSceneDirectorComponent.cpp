#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/State/ITitleLaunchState.h"
#include "Scenes/title/State/TitleLaunchStateCharge.h"
#include "Scenes/title/State/TitleLaunchStateAccelerate.h"
#include "Scenes/title/State/TitleLaunchStateBreak.h"
#include "Scenes/title/State/TitleLaunchStateAfterglow.h"

#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/MathFunction.h"
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Input/GameAction.h"
#include "Platform/Input/InputManager.h"
#include <cmath>
#include <algorithm>

TitleSceneDirectorComponent::~TitleSceneDirectorComponent() {
    CleanupLaunchPostProcess();
}

void TitleSceneDirectorComponent::OnDestroy() {
    CleanupLaunchPostProcess();
}

void TitleSceneDirectorComponent::Initialize() {
    isLaunching_ = false;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    hasExplodedDebris_ = false;
    isMicroFreezing_ = false;
    freezeTimer_ = 0.0f;
    hasTriggeredRelease_ = false;
    isLaunchPostProcessActive_ = false;
    currentLaunchState_ = nullptr;
    currentLaunchStateType_ = LaunchState::Idle;
    stateTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    debrisRepelOffsets_.clear();
    initialBgmVolume_ = 0.70f;

    // ガレキの公転パラメータ初期化 (半径X, 半径Z, 速度, 初期位相, 高さオフセット)
    debrisOrbits_ = {
        {1.8f, 1.4f, 0.45f, 0.0f, 0.35f}, {2.1f, 1.7f, -0.35f, 2.1f, -0.25f}, {1.5f, 1.9f, 0.55f, 4.3f, 0.10f}};
}

void TitleSceneDirectorComponent::OnRegisterProperties() {
    // インスペクタ用
}

TransformComponent* TitleSceneDirectorComponent::GetShipTransform() const {
    if (auto ship = shipObj_.lock()) {
        return ship->GetTransform();
    }
    return nullptr;
}

TransformComponent* TitleSceneDirectorComponent::GetCameraTransform() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetTransform();
    }
    return nullptr;
}

CameraComponent* TitleSceneDirectorComponent::GetCameraComponent() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetComponent<CameraComponent>();
    }
    return nullptr;
}

TitleCosmicNebulaComponent* TitleSceneDirectorComponent::GetNebulaComponent() const {
    if (auto nebula = nebulaObj_.lock()) {
        return nebula->GetComponent<TitleCosmicNebulaComponent>();
    }
    return nullptr;
}

void TitleSceneDirectorComponent::CacheEntities() {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // 自機の取得
    if (shipObj_.expired()) {
        if (auto shipObj = scene->FindGameObject("HeroShip")) {
            shipObj_ = shipObj;
            if (auto t = shipObj->GetTransform()) {
                initialShipPos_ = t->GetPosition();
                initialShipRot_ = t->GetRotation();
            }
        }
    }

    // カメラの取得
    if (cameraObj_.expired()) {
        if (auto camObj = scene->FindGameObject("MainCamera")) {
            cameraObj_ = camObj;
            if (auto t = camObj->GetTransform()) {
                initialCameraPos_ = t->GetPosition();
            }
            if (auto camComp = camObj->GetComponent<CameraComponent>()) {
                initialCameraFov_ = camComp->GetFovAngleY();
            }
        }
    }

    // ガレキの取得
    if (debrisObjs_.empty()) {
        const std::string debrisNames[] = {"OrbitDebris_1", "OrbitDebris_2", "OrbitDebris_3"};
        for (const auto& name : debrisNames) {
            if (auto debrisObj = scene->FindGameObject(name)) {
                debrisObjs_.push_back(debrisObj);
                if (auto t = debrisObj->GetTransform()) {
                    initialDebrisPositions_.push_back(t->GetPosition());
                }
            }
        }
    }

    // 神秘的な星雲コンポーネントの取得
    if (nebulaObj_.expired()) {
        if (auto myObj = GetGameObject()) {
            if (myObj->GetComponent<TitleCosmicNebulaComponent>()) {
                nebulaObj_ = myObj->shared_from_this();
            }
        }
        if (nebulaObj_.expired()) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetComponent<TitleCosmicNebulaComponent>()) {
                    nebulaObj_ = obj;
                    break;
                }
            }
        }
    }

    // 自機スラスターエフェクトのセットアップ（GameScene完全同期）
    SetupThrusterEffect();
}

void TitleSceneDirectorComponent::SetupThrusterEffect() {
    if (thrusterObj_.lock()) {
        return;
    }
    auto scene = GetScene();
    if (!scene) {
        return;
    }
    auto shipObj = scene->FindGameObject("HeroShip");
    if (!shipObj) {
        return;
    }

    // GameSceneと全く同じ player_thruster_effect.json をノズル位置にアタッチ生成
    auto thruster = shipObj->Instantiate("resources/prefabs/player_thruster_effect.json", nozzleOffset_, true);
    if (thruster) {
        thruster->SetIsSerializable(false); // シーン保存時の汚染防止
        thruster->SetHideInHierarchy(true);
        thrusterObj_ = thruster;
        currentThrusterScaleZ_ = 0.85f;
        targetThrusterScaleZ_ = 0.85f;

        if (auto transform = thruster->GetTransform()) {
            transform->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }

        // タイトル画面で確実にスラスター粒子を点火
        auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto pe : emitters) {
            pe->Play();
        }
    }
}

void TitleSceneDirectorComponent::TriggerDebrisExplosion() {
    if (hasExplodedDebris_) {
        return;
    }
    hasExplodedDebris_ = true;

    auto scene = GetScene();
    for (size_t i = 0; i < debrisObjs_.size(); ++i) {
        if (auto debris = debrisObjs_[i].lock()) {
            Irufemi::Vector3 debrisPos{0.0f, 0.0f, 0.0f};
            if (auto dt = debris->GetTransform()) {
                debrisPos = dt->GetWorldPosition();
            }

            // A. Voxel破砕の発火（放射スピン＋超推力吹き飛ばし）
            if (auto voxelComp = debris->GetComponent<VoxelParticleComponent>()) {
                float angle = static_cast<float>(i) * 2.094395f; // 120度刻み
                Irufemi::Vector3 blowVelocity{
                    std::cos(angle) * 11.0f, std::sin(angle) * 6.5f + 2.0f,
                    -18.0f // スラスター後流へ強烈に吹き飛ばす
                };
                voxelComp->Explode(blowVelocity, {8.0f, 14.0f, 6.0f}, {1.0f, 1.0f, 1.0f});
            }

            // B. 複合レイヤー：破砕位置に粉塵・衝撃パーティクルを展開
            if (scene) {
                auto dust = scene->InstantiatePrefab("resources/prefabs/debris_dust_effect.json", debrisPos);
                if (dust) {
                    dust->SetIsSerializable(false);
                    dust->SetHideInHierarchy(true);
                    auto emitters = dust->GetComponentsInChildren<ParticleEmitterComponent>();
                    for (auto pe : emitters) {
                        pe->Restart(false);
                    }
                }
            }

            // 元のガレキオブジェクトを非アクティブ化してVoxel破砕粒子＆粉塵煙のみを描画
            debris->SetActive(false);
        }
    }
}

void TitleSceneDirectorComponent::OnImpactRelease() {
    if (hasTriggeredRelease_) {
        return;
    }
    hasTriggeredRelease_ = true;
    isMicroFreezing_ = false;

    // 1. ガレキのVoxel粉砕飛散を一斉発火！
    TriggerDebrisExplosion();

    // 2. 音響の多重レイヤー同時炸裂！（エンジンのPlayByFileを活用した極めてクリーンな呼び出し）
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            // A: 爆砕重低音（se_debris_shatter）
            am->PlayByFile("resources/audio/SE/se_debris_shatter.mp3", 0.95f, 1.0f, AudioCategory::SE,
                           "resources/audio/SE/se_menu_decide.mp3");
            // B: 推進点火・ドップラー遠ざかり音（se_player_boost）
            am->PlayByFile("resources/audio/SE/se_player_boost.mp3", 1.0f, 1.0f, AudioCategory::SE);
        }
    }

    // 3. カメラの大激震シェイク発火（振幅 0.045m, 継続1.8秒, 22Hz）
    if (auto camObj = cameraObj_.lock()) {
        auto shake = camObj->GetComponent<CameraShakeComponent>();
        if (!shake) {
            auto newShake = camObj->AddComponent<CameraShakeComponent>();
            shake = newShake.get();
        }
        if (shake) {
            shake->PlayShakeSeconds(0.045f, kDurationAccelerate_, 22.0f);
        }
    }


}

void TitleSceneDirectorComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float deltaTime = engine->GetDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    CacheEntities();

    if (!isLaunching_) {
        UpdateIdling(deltaTime);
    } else {
        UpdateLaunchSequence(deltaTime);
    }
}

void TitleSceneDirectorComponent::UpdateIdling(float deltaTime) {
    idleTimer_ += deltaTime;

    auto shipTransform = GetShipTransform();
    auto cameraTransform = GetCameraTransform();

    // 1. 自機のホバリング浮遊（上下微動 + ロール揺らぎ）
    if (shipTransform) {
        float hoverY = initialShipPos_.y + std::sin(idleTimer_ * 1.8f) * 0.07f;
        float rollZ = initialShipRot_.z + std::sin(idleTimer_ * 1.2f) * 0.035f;
        float pitchX = initialShipRot_.x + std::cos(idleTimer_ * 1.5f) * 0.020f;

        shipTransform->SetPosition({initialShipPos_.x, hoverY, initialShipPos_.z});
        shipTransform->SetRotation({pitchX, initialShipRot_.y, rollZ});
    }

    // 2. カメラの呼吸揺れ
    if (cameraTransform) {
        float swayX = initialCameraPos_.x + std::sin(idleTimer_ * 0.8f) * 0.03f;
        float swayY = initialCameraPos_.y + std::cos(idleTimer_ * 0.9f) * 0.025f;
        cameraTransform->SetPosition({swayX, swayY, initialCameraPos_.z});
    }

    // 3. ガレキの公転運動と自転（＋マウスカーソルによる有機的重力反発）
    if (debrisRepelOffsets_.size() != debrisObjs_.size()) {
        debrisRepelOffsets_.assign(debrisObjs_.size(), {0.0f, 0.0f, 0.0f});
    }

    // 仮想カーソル座標およびカメラのビュー射影変換情報を取得
    auto engine = GetEngine();
    Irufemi::Vector2 cursorPos{640.0f, 360.0f};
    if (engine && engine->GetInputManager()) {
        cursorPos = engine->GetInputManager()->GetVirtualCursorPosition();
    }
    auto cameraComp = GetCameraComponent();
    std::shared_ptr<Camera> camera = cameraComp ? cameraComp->GetCamera() : nullptr;

    // 衝撃波の減衰更新
    shockwaveIntensity_ = (std::max)(0.0f, shockwaveIntensity_ - deltaTime * 1.6f);

    for (size_t i = 0; i < debrisObjs_.size(); ++i) {
        auto debris = debrisObjs_[i].lock();
        if (!debris || i >= debrisOrbits_.size()) {
            continue;
        }
        auto t = debris->GetTransform();
        if (!t) {
            continue;
        }

        const auto& orbit = debrisOrbits_[i];
        float angle = orbit.phase + idleTimer_ * orbit.speed;

        float x = std::cos(angle) * orbit.radiusX;
        float z = std::sin(angle) * orbit.radiusZ;
        float y = orbit.heightOffset + std::sin(angle * 1.5f) * 0.15f;
        Irufemi::Vector3 basePos{x, y, z};

        // --- マウス・カーソル接近による重力反発（2Dスクリーン投影判定） ---
        Irufemi::Vector3 targetRepelOffset{0.0f, 0.0f, 0.0f};
        if (camera) {
            Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
            Irufemi::Vector3 clipPos = Irufemi::Math::Transform(basePos, viewProj);

            // 画面手前にある場合のみ投影判定
            if (clipPos.z > 0.0f && clipPos.z < 1.0f) {
                float screenX = (clipPos.x + 1.0f) * 0.5f * camera->GetViewportWidth();
                float screenY = (1.0f - clipPos.y) * 0.5f * camera->GetViewportHeight();
                Irufemi::Vector2 uiPos = camera->ScreenToUIPosition({screenX, screenY});

                float diffX = uiPos.x - cursorPos.x;
                float diffY = uiPos.y - cursorPos.y;
                float distSq = diffX * diffX + diffY * diffY;
                const float kRepelRadius = 150.0f; // 反発影響半径 (px)

                if (distSq < kRepelRadius * kRepelRadius && distSq > 1.0f) {
                    float dist = std::sqrt(distSq);
                    float factor = 1.0f - (dist / kRepelRadius);
                    float force = factor * factor * 0.55f; // 最大55cm押し出し

                    // カーソルから外側へ逃げるベクトル
                    targetRepelOffset.x = (diffX / dist) * force;
                    targetRepelOffset.y = -(diffY / dist) * force;
                    targetRepelOffset.z = 0.0f;
                }
            }
        }

        // --- 重力波衝撃波（全ガレキ一斉共鳴・外周放射押し出し）の重畳 ---
        if (shockwaveIntensity_ > 0.001f) {
            float radialLen = std::hypot(basePos.x, basePos.y);
            if (radialLen > 0.05f) {
                float normX = basePos.x / radialLen;
                float normY = basePos.y / radialLen;
                float shockForce = shockwaveIntensity_ * 1.30f;
                targetRepelOffset.x += normX * shockForce;
                targetRepelOffset.y += normY * shockForce;
            }
        }

        // バネ・減衰追従（滑らかに目標オフセットへ移行し、離れたら公転軌道へ復帰）
        float repelLerp = 1.0f - std::exp(-8.0f * deltaTime);
        debrisRepelOffsets_[i].x = std::lerp(debrisRepelOffsets_[i].x, targetRepelOffset.x, repelLerp);
        debrisRepelOffsets_[i].y = std::lerp(debrisRepelOffsets_[i].y, targetRepelOffset.y, repelLerp);
        debrisRepelOffsets_[i].z = std::lerp(debrisRepelOffsets_[i].z, targetRepelOffset.z, repelLerp);

        // 最終トランスフォーム適用
        t->SetPosition(basePos + debrisRepelOffsets_[i]);

        // 自転
        auto currentRot = t->GetRotation();
        currentRot.x += deltaTime * 0.4f * (i + 1);
        currentRot.y += deltaTime * 0.6f;
        t->SetRotation(currentRot);
    }

    // 4. 自機スラスターの呼吸脈動（GameSceneアイドル時と完全準拠）
    targetThrusterScaleZ_ = 0.85f + std::sin(idleTimer_ * 2.2f) * 0.10f;
    float lerpFactor = 1.0f - std::exp(-10.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto t = thruster->GetTransform()) {
            t->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }
    }
}

void TitleSceneDirectorComponent::StartLaunchSequence() {
    if (isLaunching_) {
        return;
    }

    isLaunching_ = true;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    hasExplodedDebris_ = false;

    // State パターン: [Phase 1: 蓄勢 (Charge)] へ突入
    ChangeState(std::make_unique<TitleLaunchStateCharge>());
}

void TitleSceneDirectorComponent::SkipLaunchSequence() {
    if (hasTriggeredSceneTransition_) {
        return;
    }

    CleanupLaunchPostProcess();
    hasTriggeredSceneTransition_ = true;
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->StopCategory(AudioCategory::SE);
        }
        if (auto sm = engine->GetSceneManager()) {
            // 世界観に合致した重力シアンブラックで空間を吸い込み暗転
            sm->TransitionTo(nextSceneName_, SceneTransition::Type::RadialBlur, 0.35f, EaseType::EaseInQuad,
                             Irufemi::Vector4{0.01f, 0.03f, 0.06f, 1.0f});
        }
    }
}

void TitleSceneDirectorComponent::ChangeState(std::unique_ptr<ITitleLaunchState> newState) {
    if (currentLaunchState_) {
        currentLaunchState_->Exit(this);
    }
    currentLaunchState_ = std::move(newState);
    stateTimer_ = 0.0f;
    if (currentLaunchState_) {
        currentLaunchStateType_ = static_cast<LaunchState>(currentLaunchState_->GetStateType());
        currentLaunchState_->Enter(this);
    } else {
        currentLaunchStateType_ = LaunchState::Idle;
    }
}

void TitleSceneDirectorComponent::ChangeState(LaunchState newState) {
    using LaunchStateFactory = std::unique_ptr<ITitleLaunchState> (*)();
    static const std::unordered_map<LaunchState, LaunchStateFactory> kLaunchStateFactories = {
        {LaunchState::Idle, []() -> std::unique_ptr<ITitleLaunchState> { return nullptr; }},
        {LaunchState::Charge,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateCharge>(); }},
        {LaunchState::Accelerate,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateAccelerate>(); }},
        {LaunchState::Break,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateBreak>(); }},
        {LaunchState::Afterglow,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateAfterglow>(); }},
    };

    if (auto it = kLaunchStateFactories.find(newState); it != kLaunchStateFactories.end()) {
        ChangeState(it->second());
    }
}

TitleSceneDirectorComponent::LaunchState TitleSceneDirectorComponent::GetLaunchState() const {
    if (currentLaunchState_) {
        return static_cast<LaunchState>(currentLaunchState_->GetStateType());
    }
    return currentLaunchStateType_;
}

void TitleSceneDirectorComponent::TriggerSceneTransition(float fadeDuration) {
    if (hasTriggeredSceneTransition_) {
        return;
    }
    hasTriggeredSceneTransition_ = true;
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            // 重力特異点跳躍（事象の地平線への空間吸い込み・重力色収差・流体渦巻き連動）
            sm->TransitionTo(nextSceneName_, SceneTransition::Type::GravitySingularity, fadeDuration, EaseType::EaseInCubic,
                             Irufemi::Vector4{0.005f, 0.015f, 0.035f, 1.0f});
        }
    }
}

void TitleSceneDirectorComponent::UpdateLaunchSequence(float deltaTime) {
    launchTimer_ += deltaTime;

    // プレイヤーによるクイック・スキップ判定（出撃開始から0.25秒後以降、決定キーまたはクリックで即フェード突入）
    if (launchTimer_ > 0.25f && !hasTriggeredSceneTransition_) {
        if (auto engine = GetEngine()) {
            if (auto input = engine->GetInputManager()) {
                if (InputHelper::IsActionPressed(input, GameAction::UI_Submit) || input->IsCursorActionPressed()) {
                    SkipLaunchSequence();
                    return;
                }
            }
        }
    }

    // Stateパターン更新（多態性呼び出し）
    stateTimer_ += deltaTime;
    if (currentLaunchState_) {
        currentLaunchState_->Update(this, deltaTime);
    }

    // 出撃加速・離脱時の動的シネマティック・ポストプロセス更新
    UpdateLaunchPostProcess(deltaTime);

    // スラスターのスケールを急峻に追従
    float lerpFactor = 1.0f - std::exp(-18.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto transform = thruster->GetTransform()) {
            transform->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }
    }
}

void TitleSceneDirectorComponent::UpdateLaunchPostProcess(float deltaTime) {
    (void)deltaTime;
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto ppm = engine->GetPostProcessManager();
    if (!ppm) {
        return;
    }

    // Phase 1 (Charge) または Idle 中、出撃未開始時は適用しない
    if (!isLaunching_ || currentLaunchStateType_ == LaunchState::Idle ||
        currentLaunchStateType_ == LaunchState::Charge) {
        CleanupLaunchPostProcess();
        return;
    }

    // 大手ゲームエンジンのプラクティス：2DメニューUI（フェード消滅中）を汚染せず、3D空間と星雲にのみ適用
    // Layer::PreUI でエフェクトスタックに安全に登録（初回フレームでのみ登録）
    if (!isLaunchPostProcessActive_) {
        ppm->AddActiveMode(PostProcessMode::Vignette, PostProcessManager::Layer::PreUI);
        ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PreUI);
        ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PreUI);
        isLaunchPostProcessActive_ = true;
    }

    // タイムライン連動アニメーションの目標値
    float targetBlurWidth = 0.0f;
    float targetVignetteRadius = 1.0f;
    float targetChroma = 0.0f;

    // 星雲シェーダー（CosmicNebula.PS.hlsl）および重力特異点跳躍（GravitySingularity）と完全合致した焦点
    const Irufemi::Vector2 kSingularityCenter{0.50f, 0.42f};

    if (currentLaunchStateType_ == LaunchState::Accelerate) {
        // [Phase 2: 咆哮・点火急加速]
        // 臨界蓄圧（マイクロフリーズ 0.045s）中は身震いのみとし、光学的歪みは保留
        float effectiveTimer = (std::max)(0.0f, stateTimer_ - kDurationFreeze_);
        float duration = (std::max)(0.001f, kDurationAccelerate_ - kDurationFreeze_);
        float p = std::clamp(effectiveTimer / duration, 0.0f, 1.0f);

        // 自機の超推力急加速曲線（4次急加速）に自然に呼応する3次イーズイン
        float curve = p * p * p;

        // トンネル視野狭窄: 外周から徐々に重力シアンブラックの暗がりが絞り込まれる (1.00 -> 0.70)
        targetVignetteRadius = std::lerp(1.00f, 0.70f, curve);

        // 特異点ラジアルブラー: 自機の推進力に合わせて外周の星雲がじわじわと流れる (0.00 -> 0.016)
        targetBlurWidth = std::lerp(0.0f, 0.016f, curve);

        // 速度ドップラー色収差: わずかなレンズ周辺赤方偏移 (0.00 -> 0.006)
        targetChroma = std::lerp(0.0f, 0.006f, curve);

    } else if (currentLaunchStateType_ == LaunchState::Break) {
        // [Phase 3: 突破・超光速ダイブ]
        float p = std::clamp(stateTimer_ / (std::max)(0.001f, kDurationBreak_), 0.0f, 1.0f);

        // 超光速の極点へ向けてサインカーブで滑らかに飽和
        float curve = std::sin(p * 1.5707963f);

        // トンネル最深部: 事象の地平線トンネルに飛び込む (0.70 -> 0.52)
        targetVignetteRadius = std::lerp(0.70f, 0.52f, curve);

        // ラジアルブラー最大ピーク (0.016 -> 0.032)
        targetBlurWidth = std::lerp(0.016f, 0.032f, curve);

        // レンズ色収差ピーク (0.006 -> 0.014)
        targetChroma = std::lerp(0.006f, 0.014f, curve);

    } else if (currentLaunchStateType_ == LaunchState::Afterglow) {
        // [Phase 4: 余韻・自機消失後]
        // 自機が特異点へ消滅した直後、ピークのワープ歪みを維持し、
        // 1.25秒後に発火する GravitySingularity シーン遷移（暗転）へシームレスにバトンタッチ
        targetVignetteRadius = 0.52f;
        targetBlurWidth = 0.032f;
        targetChroma = 0.014f;
    }

    // --- 各ポストプロセスパラメータの安全な適用 ---
    // 1. トンネル視野狭窄 (Vignette): 深遠な重力シアンブラックで外周を包み込み、中央の特異点へ視線を誘導
    auto& vig = ppm->GetVignetteParams();
    vig.color = Irufemi::Vector4{0.005f, 0.015f, 0.035f, 1.0f}; // 世界観に合致した重力シアンブラック
    vig.radius = targetVignetteRadius;
    vig.softness = 0.45f;

    // 2. 特異点ラジアルブラー (RadialBlur): 中心(0.50, 0.42)の特異点に向けて周辺星雲を吸い込み歪曲
    auto& rb = ppm->GetRadialBlurParams();
    rb.center = kSingularityCenter;
    rb.blurWidth = targetBlurWidth;
    rb.numSamples = 24;

    // 3. 光学的赤方偏移 (ChromaticAberration): 超光速突破に伴う繊細な波長ズレ
    auto& ca = ppm->GetChromaticAberrationParams();
    ca.intensity = targetChroma;
}

void TitleSceneDirectorComponent::CleanupLaunchPostProcess() {
    if (auto nebulaComp = GetNebulaComponent()) {
        nebulaComp->SetCollapseProgress(0.0f);
        nebulaComp->SetSwirlMultiplier(1.0f);
    }

    if (isLaunchPostProcessActive_) {
        if (auto engine = GetEngine()) {
            if (auto ppm = engine->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::Vignette);
                ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
                ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);

                // パラメータを安全にデフォルト初期化
                auto& rb = ppm->GetRadialBlurParams();
                rb.blurWidth = 0.0f;
                auto& vig = ppm->GetVignetteParams();
                vig.radius = 1.0f;
                auto& ca = ppm->GetChromaticAberrationParams();
                ca.intensity = 0.0f;
            }
        }
        isLaunchPostProcessActive_ = false;
    }
}

bool TitleSceneDirectorComponent::GetClosestFrontDebrisScreenPos(Irufemi::Vector2& outUIPos) const {
    auto cameraComp = GetCameraComponent();
    if (!cameraComp) {
        return false;
    }
    auto camera = cameraComp->GetCamera();
    if (!camera) {
        return false;
    }

    Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
    float bestZ = 9999.0f;
    bool found = false;

    for (size_t i = 0; i < debrisObjs_.size(); ++i) {
        auto debris = debrisObjs_[i].lock();
        if (!debris || i >= debrisOrbits_.size()) {
            continue;
        }
        auto t = debris->GetTransform();
        if (!t) {
            continue;
        }

        Irufemi::Vector3 worldPos = t->GetWorldPosition();
        Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);

        // 画面手前にあり、カメラ視野内
        if (clipPos.z > 0.05f && clipPos.z < 1.0f) {
            // より手前（カメラに近いもの）を優先
            if (clipPos.z < bestZ) {
                bestZ = clipPos.z;
                float screenX = (clipPos.x + 1.0f) * 0.5f * camera->GetViewportWidth();
                float screenY = (1.0f - clipPos.y) * 0.5f * camera->GetViewportHeight();
                outUIPos = camera->ScreenToUIPosition({screenX, screenY});
                found = true;
            }
        }
    }
    return found;
}

void TitleSceneDirectorComponent::TriggerGravitationalShockwave(float power) {
    shockwaveIntensity_ = (std::max)(shockwaveIntensity_, power);
}
