#include "Scenes/title/TitleSceneDirectorComponent.h"

#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include <cmath>
#include <algorithm>

void TitleSceneDirectorComponent::Initialize() {
    isLaunching_ = false;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    idleTimer_ = 0.0f;

    // ガレキの公転パラメータ初期化 (半径X, 半径Z, 速度, 初期位相, 高さオフセット)
    debrisOrbits_ = {
        { 1.8f, 1.4f, 0.45f, 0.0f, 0.35f },
        { 2.1f, 1.7f, -0.35f, 2.1f, -0.25f },
        { 1.5f, 1.9f, 0.55f, 4.3f, 0.10f }
    };
}

void TitleSceneDirectorComponent::OnRegisterProperties() {
    // インスペクタ用
}

void TitleSceneDirectorComponent::CacheEntities() {
    auto scene = GetScene();
    if (!scene) return;

    // 自機の取得
    if (!shipTransform_) {
        if (auto shipObj = scene->FindGameObject("HeroShip")) {
            shipTransform_ = shipObj->GetComponent<TransformComponent>();
            if (shipTransform_) {
                initialShipPos_ = shipTransform_->GetPosition();
                initialShipRot_ = shipTransform_->GetRotation();
            }
        }
    }

    // カメラの取得
    if (!cameraTransform_) {
        if (auto camObj = scene->FindGameObject("MainCamera")) {
            cameraTransform_ = camObj->GetComponent<TransformComponent>();
            if (cameraTransform_) {
                initialCameraPos_ = cameraTransform_->GetPosition();
            }
        }
    }

    // ガレキの取得
    if (debrisTransforms_.empty()) {
        const std::string debrisNames[] = { "OrbitDebris_1", "OrbitDebris_2", "OrbitDebris_3" };
        for (const auto& name : debrisNames) {
            if (auto debrisObj = scene->FindGameObject(name)) {
                if (auto t = debrisObj->GetComponent<TransformComponent>()) {
                    debrisTransforms_.push_back(t);
                    initialDebrisPositions_.push_back(t->GetPosition());
                }
            }
        }
    }

    // 神秘的な星雲コンポーネントの取得
    if (!nebulaComp_) {
        nebulaComp_ = GetGameObject() ? GetGameObject()->GetComponent<TitleCosmicNebulaComponent>() : nullptr;
        if (!nebulaComp_) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj) {
                    if (auto nc = obj->GetComponent<TitleCosmicNebulaComponent>()) {
                        nebulaComp_ = nc;
                        break;
                    }
                }
            }
        }
    }

    // 自機スラスターエフェクトのセットアップ（GameScene完全同期）
    SetupThrusterEffect();
}

void TitleSceneDirectorComponent::SetupThrusterEffect() {
    if (thrusterObj_.lock()) return;
    auto scene = GetScene();
    if (!scene) return;
    auto shipObj = scene->FindGameObject("HeroShip");
    if (!shipObj) return;

    // GameSceneと全く同じ player_thruster_effect.json をノズル位置にアタッチ生成
    auto thruster = shipObj->Instantiate("resources/prefabs/player_thruster_effect.json", nozzleOffset_, true);
    if (thruster) {
        thruster->SetIsSerializable(false); // シーン保存時の汚染防止
        thruster->SetHideInHierarchy(true);
        thrusterObj_ = thruster;
        currentThrusterScaleZ_ = 0.8f;
        targetThrusterScaleZ_ = 0.8f;

        // 子エミッターの初期パラメータをキャッシュ
        thrusterEmitters_.clear();
        auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto emitter : emitters) {
            if (auto pObj = emitter->GetParticleObject()) {
                EmitterInitialParams p;
                p.pObj = pObj;
                p.baseVelocity = 18.0f; // アイドル時の基準初速
                p.baseLifeTimeMax = pObj->GetLifeTimeMax();
                if (p.baseLifeTimeMax <= 0.01f) p.baseLifeTimeMax = 0.16f;
                p.baseEmissionRate = pObj->GetEmissionRate();
                p.baseStartScale = pObj->GetStartScale();
                p.baseMidScale = pObj->GetMidScale();

                // アイドル初速を反映
                pObj->SetVelocity(p.baseVelocity);
                pObj->SetLifeTimeMax(p.baseLifeTimeMax);
                thrusterEmitters_.push_back(p);
            }
        }

        if (auto transform = thruster->GetComponent<TransformComponent>()) {
            transform->SetScale({ 1.0f, 1.0f, currentThrusterScaleZ_ });
        }
    }
}

void TitleSceneDirectorComponent::Update() {
    auto engine = GetEngine();
    if (!engine) return;

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

    // 1. 自機のホバリング浮遊（上下微動 + ロール揺らぎ）
    if (shipTransform_) {
        float hoverY = initialShipPos_.y + std::sin(idleTimer_ * 1.8f) * 0.07f;
        float rollZ = initialShipRot_.z + std::sin(idleTimer_ * 1.2f) * 0.035f;
        float pitchX = initialShipRot_.x + std::cos(idleTimer_ * 1.5f) * 0.020f;

        shipTransform_->SetPosition({ initialShipPos_.x, hoverY, initialShipPos_.z });
        shipTransform_->SetRotation({ pitchX, initialShipRot_.y, rollZ });
    }

    // 2. カメラの呼吸揺れ
    if (cameraTransform_) {
        float swayX = initialCameraPos_.x + std::sin(idleTimer_ * 0.8f) * 0.03f;
        float swayY = initialCameraPos_.y + std::cos(idleTimer_ * 0.9f) * 0.025f;
        cameraTransform_->SetPosition({ swayX, swayY, initialCameraPos_.z });
    }

    // 3. ガレキの公転運動と自転
    for (size_t i = 0; i < debrisTransforms_.size(); ++i) {
        auto t = debrisTransforms_[i];
        if (!t || i >= debrisOrbits_.size()) continue;

        const auto& orbit = debrisOrbits_[i];
        float angle = orbit.phase + idleTimer_ * orbit.speed;

        float x = std::cos(angle) * orbit.radiusX;
        float z = std::sin(angle) * orbit.radiusZ;
        float y = orbit.heightOffset + std::sin(angle * 1.5f) * 0.15f;

        t->SetPosition({ x, y, z });

        // 自転
        auto currentRot = t->GetRotation();
        currentRot.x += deltaTime * 0.4f * (i + 1);
        currentRot.y += deltaTime * 0.6f;
        t->SetRotation(currentRot);
    }

    // 4. 自機スラスターの呼吸脈動（GameSceneアイドル時と完全同期）
    targetThrusterScaleZ_ = 0.8f + std::sin(idleTimer_ * 2.2f) * 0.12f;
    float lerpFactor = 1.0f - std::exp(-10.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto t = thruster->GetComponent<TransformComponent>()) {
            t->SetScale({ 1.0f, 1.0f, currentThrusterScaleZ_ });
        }
    }

    // パーティクルのアイドル初速を維持
    for (auto& e : thrusterEmitters_) {
        if (e.pObj) {
            e.pObj->SetVelocity(e.baseVelocity * (currentThrusterScaleZ_ / 0.8f));
            e.pObj->SetLifeTimeMax(e.baseLifeTimeMax);
        }
    }
}

void TitleSceneDirectorComponent::StartLaunchSequence() {
    if (isLaunching_) return;

    isLaunching_ = true;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;

    // 神秘的な星雲の重力パルスを発火（中心が眩く収束・発光）
    if (nebulaComp_) {
        nebulaComp_->TriggerPulse(1.0f);
    }

    // 出撃重力チャージSE再生
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            auto sound = am->GetOrLoadSoundByFile("resources/audio/se_player_boost.wav", "se_player_boost");
            if (!sound) {
                sound = am->GetOrLoadSoundByFile("resources/audio/se_menu_decision.wav", "se_menu_decision");
            }
            if (sound) {
                am->Play(sound, false, 0.9f);
            }
        }
    }
}

void TitleSceneDirectorComponent::UpdateLaunchSequence(float deltaTime) {
    launchTimer_ += deltaTime;

    float t = std::clamp(launchTimer_ / kTotalLaunchDuration_, 0.0f, 1.0f);

    // [フェーズ 1: 0.00s 〜 0.25s] 重力収束＆予備動作（タメ）
    if (launchTimer_ <= 0.25f) {
        float p1 = launchTimer_ / 0.25f;
        // 自機がわずかに沈み込む（タメ動作）
        if (shipTransform_) {
            float backZ = initialShipPos_.z - p1 * 0.35f;
            shipTransform_->SetPosition({ initialShipPos_.x, initialShipPos_.y, backZ });
        }

        // ガレキが自機中心へキュッと収束
        for (size_t i = 0; i < debrisTransforms_.size(); ++i) {
            if (auto dt = debrisTransforms_[i]) {
                const auto& origPos = initialDebrisPositions_[i];
                float cx = origPos.x * (1.0f - p1 * 0.45f);
                float cy = origPos.y * (1.0f - p1 * 0.45f);
                float cz = origPos.z * (1.0f - p1 * 0.45f);
                dt->SetPosition({ cx, cy, cz });
            }
        }

        // スラスター炎の引き絞り（チャージの予兆）
        targetThrusterScaleZ_ = 0.45f;
        for (auto& e : thrusterEmitters_) {
            if (e.pObj) {
                e.pObj->SetVelocity(e.baseVelocity * 0.5f);
                e.pObj->SetLifeTimeMax(e.baseLifeTimeMax * 0.6f);
            }
        }
    }
    // [フェーズ 2: 0.25s 〜 0.95s] スラスター点火＆前方急加速離脱（アフターバーナー全開）
    else {
        float p2 = (launchTimer_ - 0.25f) / (kTotalLaunchDuration_ - 0.25f);
        float accelCurve = p2 * p2 * p2; // 3次急加速

        if (shipTransform_) {
            float boostZ = initialShipPos_.z - 0.35f + accelCurve * 50.0f;
            shipTransform_->SetPosition({ initialShipPos_.x, initialShipPos_.y, boostZ });
        }

        // カメラの自機追従ドリーイン（自機にしっかり食らいつき、迫力のアフターバーナーを至近距離で捉える）
        if (cameraTransform_) {
            float camDollyZ = initialCameraPos_.z + accelCurve * 42.0f;
            cameraTransform_->SetPosition({ initialCameraPos_.x, initialCameraPos_.y, camDollyZ });
        }

        // アフターバーナー爆発的伸長（初速最大110m/s、寿命0.38秒へブーストし、後方に約35〜40mの細長い超高速火炎を形成！）
        // ※パーティクルのXY太さ（startScale/midScale）を肥大化させると手前の視界を塞ぎ潰れて見えるため、
        //   ノズル口径をキープしたまま、初速と寿命のみで後方へのシャープな長大噴流を実現する。
        targetThrusterScaleZ_ = std::lerp(1.8f, 3.2f, p2);
        for (auto& e : thrusterEmitters_) {
            if (e.pObj) {
                float boostVel = std::lerp(25.0f, 110.0f, p2);
                float boostLife = std::lerp(0.18f, 0.38f, p2);

                e.pObj->SetVelocity(boostVel);
                e.pObj->SetLifeTimeMax(boostLife);
                e.pObj->SetEmissionRate(e.baseEmissionRate * std::lerp(1.0f, 2.2f, p2));
                // ノズル口径を維持し、カメラ前での球状肥大化・平面的潰れを完全に防止
                e.pObj->SetStartScale(e.baseStartScale);
                e.pObj->SetMidScale(e.baseMidScale);
            }
        }
    }

    // スラスターのスケールを急峻に追従
    float lerpFactor = 1.0f - std::exp(-18.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto transform = thruster->GetComponent<TransformComponent>()) {
            transform->SetScale({ 1.0f, 1.0f, currentThrusterScaleZ_ });
        }
    }

    // [フェーズ 3: 0.88s] InGame シーンへのシームレスフェード遷移
    if (launchTimer_ >= 0.88f && !hasTriggeredSceneTransition_) {
        hasTriggeredSceneTransition_ = true;
        if (auto engine = GetEngine()) {
            if (auto sm = engine->GetSceneManager()) {
                sm->TransitionTo("InGame", SceneTransition::Type::Fade, 0.5f);
            }
        }
    }
}
