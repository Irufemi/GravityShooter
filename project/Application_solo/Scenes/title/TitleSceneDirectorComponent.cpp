#include "Scenes/title/TitleSceneDirectorComponent.h"

#include "Scenes/title/TitleGravityWaveComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
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

    // 重力波コンポーネントの取得
    if (!gravityWaveComp_) {
        gravityWaveComp_ = GetGameObject() ? GetGameObject()->GetComponent<TitleGravityWaveComponent>() : nullptr;
        if (!gravityWaveComp_) {
            // シーン全体から探索
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj) {
                    if (auto gwc = obj->GetComponent<TitleGravityWaveComponent>()) {
                        gravityWaveComp_ = gwc;
                        break;
                    }
                }
            }
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
}

void TitleSceneDirectorComponent::StartLaunchSequence() {
    if (isLaunching_) return;

    isLaunching_ = true;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;

    // 重力波パルスを発火（全画面を大きく歪ませる）
    if (gravityWaveComp_) {
        gravityWaveComp_->TriggerImpulse(1.0f);
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

    // [フェーズ 1: 0.00s 〜 0.25s] 重力収束＆予備動作
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
    }
    // [フェーズ 2: 0.25s 〜 0.95s] スラスター点火＆前方急加速離脱
    else {
        float p2 = (launchTimer_ - 0.25f) / (kTotalLaunchDuration_ - 0.25f);
        float accelCurve = p2 * p2 * p2; // 3次急加速

        if (shipTransform_) {
            float boostZ = initialShipPos_.z - 0.35f + accelCurve * 50.0f;
            shipTransform_->SetPosition({ initialShipPos_.x, initialShipPos_.y, boostZ });
        }

        // カメラの自機追従ドリーイン
        if (cameraTransform_) {
            float camDollyZ = initialCameraPos_.z + p2 * 8.0f;
            cameraTransform_->SetPosition({ initialCameraPos_.x, initialCameraPos_.y, camDollyZ });
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
