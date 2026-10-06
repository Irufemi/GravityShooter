#include "Scenes/title/TitleSceneDirectorComponent.h"

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
#include "Input/GameAction.h"
#include "Platform/Input/InputManager.h"
#include <cmath>
#include <algorithm>

void TitleSceneDirectorComponent::Initialize() {
    isLaunching_ = false;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    hasExplodedDebris_ = false;
    isMicroFreezing_ = false;
    freezeTimer_ = 0.0f;
    hasTriggeredRelease_ = false;
    launchState_ = LaunchState::Idle;
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

    // 3. カメラの大激震シェイク発火（振幅 0.045m, 継続1.1秒, 22Hz）
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

    // 4. 星雲の衝撃パルス第2波（最大爆発波紋）
    if (auto nebulaComp = GetNebulaComponent()) {
        nebulaComp->TriggerPulse(1.5f);
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

    // FSM: [Phase 1: 蓄勢 (Charge)] へ突入
    SetLaunchState(LaunchState::Charge);
}

void TitleSceneDirectorComponent::SkipLaunchSequence() {
    if (hasTriggeredSceneTransition_) {
        return;
    }

    hasTriggeredSceneTransition_ = true;
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            sm->TransitionTo(nextSceneName_, SceneTransition::Type::Fade, 0.35f);
        }
    }
}

void TitleSceneDirectorComponent::SetLaunchState(LaunchState newState) {
    if (launchState_ == newState) {
        return;
    }
    launchState_ = newState;
    stateTimer_ = 0.0f;
    OnEnterLaunchState(launchState_);
}

void TitleSceneDirectorComponent::OnEnterLaunchState(LaunchState state) {
    auto shipTransform = GetShipTransform();
    auto nebulaComp = GetNebulaComponent();

    switch (state) {
    case LaunchState::Charge: {
        // 出撃開始時の姿勢・座標をキャッシュ
        if (shipTransform) {
            launchStartRot_ = shipTransform->GetRotation();
        } else {
            launchStartRot_ = initialShipRot_;
        }

        // 神秘的な星雲の重力パルス第1波（中心収束・発光）
        if (nebulaComp) {
            nebulaComp->TriggerPulse(1.2f);
        }

        // 出撃SE再生（Springin' Sound Stock 製「気を溜める1」によるエネルギー充填・タメ）
        if (auto engine = GetEngine()) {
            if (auto am = engine->GetAudioManager()) {
                am->PlayByFile("resources/audio/SE/se_launch_charge.mp3", 0.95f, 1.0f, AudioCategory::SE,
                               "resources/audio/SE/se_player_boost.mp3");
            }
        }

        // タイトルBGMの音量をキャッシュ（即座に停止せず、Charge進行に合わせて対数フェードアウト）
        if (auto scene = GetScene()) {
            if (auto bgmObj = scene->FindGameObject("BGMPlayer")) {
                if (auto audioSource = bgmObj->GetComponent<AudioSourceComponent>()) {
                    initialBgmVolume_ = 0.70f;
                }
            }
        }
        break;
    }
    case LaunchState::Accelerate: {
        // 臨界蓄圧（マイクロフリーズ）開始：直ちにガレキ破砕せず、0.045秒のタメを作る
        isMicroFreezing_ = true;
        freezeTimer_ = 0.0f;
        hasTriggeredRelease_ = false;

        // 自機スラスターノズル内のGPUパーティクルを瞬間オーバードライブ再点火
        if (auto thruster = thrusterObj_.lock()) {
            auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
            for (auto pe : emitters) {
                pe->Restart(false);
            }
        }

        // 予兆としての単発衝撃インパルス（振幅 0.02m の瞬間シェイク）
        if (auto camObj = cameraObj_.lock()) {
            auto shake = camObj->GetComponent<CameraShakeComponent>();
            if (!shake) {
                auto newShake = camObj->AddComponent<CameraShakeComponent>();
                shake = newShake.get();
            }
            if (shake) {
                shake->PlayShakeSeconds(0.02f, kDurationFreeze_, 30.0f);
            }
        }
        break;
    }
    case LaunchState::Break: {
        // 光速離脱開始
        break;
    }
    case LaunchState::Afterglow: {
        // 自機を画面外へ退避（完全消失）
        if (auto ship = shipObj_.lock()) {
            ship->SetActive(false);
        }
        break;
    }
    default:
        break;
    }
}

void TitleSceneDirectorComponent::OnUpdateLaunchState(LaunchState state, float deltaTime) {
    stateTimer_ += deltaTime;

    auto shipTransform = GetShipTransform();
    auto cameraTransform = GetCameraTransform();
    auto camComp = GetCameraComponent();

    switch (state) {
    // =========================================================================
    // [Phase 1: 蓄勢 (Charge)] 0.00s 〜 0.50s (タメ・重力収束・整流)
    // =========================================================================
    case LaunchState::Charge: {
        float p1 = std::clamp(stateTimer_ / kDurationCharge_, 0.0f, 1.0f);
        float alignT = 1.0f - std::pow(1.0f - p1, 2.0f); // 2次イーズアウト

        // 自機の沈み込み＆斜め姿勢から正面水平([0, 0, 0])への整流
        if (shipTransform) {
            float backZ = initialShipPos_.z - p1 * 0.45f;
            shipTransform->SetPosition({initialShipPos_.x, initialShipPos_.y, backZ});
            shipTransform->SetRotation({std::lerp(launchStartRot_.x, 0.0f, alignT),
                                        std::lerp(launchStartRot_.y, 0.0f, alignT),
                                        std::lerp(launchStartRot_.z, 0.0f, alignT)});
        }

        // ガレキが自機中心へキュッと収束
        for (size_t i = 0; i < debrisObjs_.size(); ++i) {
            if (auto debris = debrisObjs_[i].lock()) {
                if (auto dt = debris->GetTransform()) {
                    const auto& origPos = initialDebrisPositions_[i];
                    dt->SetPosition({origPos.x * (1.0f - p1 * 0.50f), origPos.y * (1.0f - p1 * 0.50f),
                                     origPos.z * (1.0f - p1 * 0.50f)});
                }
            }
        }

        // スラスター炎の引き絞り（点火直前のエネルギー圧縮）
        targetThrusterScaleZ_ = 0.35f;

        if (camComp) {
            camComp->SetFovAngleY(initialCameraFov_);
        }

        // BGMシネマティック・フェードアウト（1.5乗の対数減衰で美しく消去）
        if (auto scene = GetScene()) {
            if (auto bgmObj = scene->FindGameObject("BGMPlayer")) {
                if (auto audioSource = bgmObj->GetComponent<AudioSourceComponent>()) {
                    float fadeT = std::pow(1.0f - p1, 1.5f);
                    audioSource->SetVolume(initialBgmVolume_ * fadeT);
                    if (p1 >= 1.0f) {
                        audioSource->Stop();
                    }
                }
            }
        }

        // 状態遷移判定
        if (stateTimer_ >= kDurationCharge_) {
            SetLaunchState(LaunchState::Accelerate);
        }
        break;
    }

    // =========================================================================
    // [Phase 2: 咆哮 (Accelerate)] 0.50s 〜 1.60s (アフターバーナー急加速・動的FOV)
    // =========================================================================
    case LaunchState::Accelerate: {
        // --- 臨界蓄圧（マイクロフリーズ・身震い）制御 ---
        if (isMicroFreezing_) {
            freezeTimer_ += deltaTime;

            // 0.045秒間は前進をロックし、超推力に耐える80Hz高周波ジッター（身震い）を重畳
            if (freezeTimer_ < kDurationFreeze_) {
                if (shipTransform) {
                    float jitter = std::sin(freezeTimer_ * 180.0f) * 0.008f;
                    shipTransform->SetPosition({initialShipPos_.x + jitter, initialShipPos_.y, initialShipPos_.z - 0.45f});
                    shipTransform->SetRotation({0.0f, 0.0f, jitter * 1.5f});
                }
                return; // フリーズ待機（前進加速・破砕展開を一時保留）
            }

            // 臨界解放マイルストーン発火！（大爆発・Voxel破砕・多重音響・激震の解き放ち）
            OnImpactRelease();
        }

        // フリーズ解除後の実効加速時間
        float effectiveTimer = stateTimer_ - kDurationFreeze_;
        float p2 = std::clamp(effectiveTimer / (kDurationAccelerate_ - kDurationFreeze_), 0.0f, 1.0f);
        float accelCurve = p2 * p2 * p2 * p2; // 4次急加速曲線（初速ゼロから猛烈な爆発的射出）

        // 自機の超推力突進
        if (shipTransform) {
            float boostZ = initialShipPos_.z - 0.45f + accelCurve * 65.0f;
            shipTransform->SetPosition({initialShipPos_.x, initialShipPos_.y, boostZ});
            shipTransform->SetRotation({0.0f, 0.0f, 0.0f});
        }

        // カメラ追従ドリーイン
        if (cameraTransform) {
            float camDollyZ = initialCameraPos_.z + accelCurve * 52.0f;
            cameraTransform->SetPosition({initialCameraPos_.x, initialCameraPos_.y, camDollyZ});
        }

        // 動的FOV（ワープスピード効果）: 加速中盤で画面周辺がワイドに歪む
        if (camComp) {
            float fovSin = std::sin(p2 * 3.14159265f); // 0 -> 1 -> 0
            float extraFov = (16.0f * 3.14159265f / 180.0f) * fovSin;
            camComp->SetFovAngleY(initialCameraFov_ + extraFov);
        }

        // スラスター急伸長
        targetThrusterScaleZ_ = std::lerp(1.2f, 2.4f, p2);

        // 状態遷移判定
        if (stateTimer_ >= kDurationAccelerate_) {
            SetLaunchState(LaunchState::Break);
        }
        break;
    }

    // =========================================================================
    // [Phase 3: 突破 (Break)] 1.60s 〜 2.20s (超光速離脱・光の点へ消滅)
    // =========================================================================
    case LaunchState::Break: {
        float p3 = std::clamp(stateTimer_ / kDurationBreak_, 0.0f, 1.0f);

        // 自機は光の彼方へ突き抜け、Scaleを急速に縮小して光の点へ
        if (shipTransform) {
            float boostZ = initialShipPos_.z - 0.45f + 65.0f + p3 * 75.0f;
            shipTransform->SetPosition({initialShipPos_.x, initialShipPos_.y, boostZ});

            float shrinkScale = std::lerp(1.0f, 0.05f, p3);
            shipTransform->SetScale({shrinkScale, shrinkScale, shrinkScale});
        }

        // FOVを通常視野角へスムーズに戻す
        if (camComp) {
            float returnFov =
                std::lerp(initialCameraFov_ + (16.0f * 3.14159265f / 180.0f) * 0.15f, initialCameraFov_, p3);
            camComp->SetFovAngleY(returnFov);
        }

        targetThrusterScaleZ_ = 2.4f;

        // 状態遷移判定
        if (stateTimer_ >= kDurationBreak_) {
            SetLaunchState(LaunchState::Afterglow);
        }
        break;
    }

    // =========================================================================
    // [Phase 4: 余韻・静寂 (Afterglow)] 2.20s 〜 3.20s (星雲残光・風の抜け・暗転)
    // =========================================================================
    case LaunchState::Afterglow: {
        float p4 = std::clamp(stateTimer_ / kDurationAfterglow_, 0.0f, 1.0f);

        // ガレキが静寂の中でゆっくりと通常回転へ戻る
        for (size_t i = 0; i < debrisObjs_.size(); ++i) {
            if (auto debris = debrisObjs_[i].lock()) {
                if (auto dt = debris->GetTransform()) {
                    auto rot = dt->GetRotation();
                    rot.y += deltaTime * 0.2f * (i + 1);
                    dt->SetRotation(rot);
                }
            }
        }

        // 余韻開始から 0.50秒（全体約2.70秒地点）で 指定シーンへの優雅なフェードアウト（0.6秒）を発火
        if (stateTimer_ >= 0.50f && !hasTriggeredSceneTransition_) {
            hasTriggeredSceneTransition_ = true;
            if (auto engine = GetEngine()) {
                if (auto sm = engine->GetSceneManager()) {
                    sm->TransitionTo(nextSceneName_, SceneTransition::Type::Fade, 0.6f);
                }
            }
        }
        break;
    }
    default:
        break;
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

    // FSM更新
    OnUpdateLaunchState(launchState_, deltaTime);

    // スラスターのスケールを急峻に追従
    float lerpFactor = 1.0f - std::exp(-18.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto transform = thruster->GetTransform()) {
            transform->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }
    }
}
