#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Effects/EffectManagerComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Core/Utility/Log.h"
#include <cmath>
#include <iostream>

void PlayerDamageVisualizerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Flash Interval", &flashInterval_);
    RegisterProperty("Flash Duration", &flashDuration_);
    RegisterProperty("Shake Intensity", &shakeIntensity_);
}

void PlayerDamageVisualizerComponent::Initialize() {
    flashTimer_ = 0.0f;
    flashRemaining_ = 0.0f;
    isFlashing_ = false;
    colorCached_ = false;
}

void PlayerDamageVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    healthComp_ = gameObject_->GetComponent<PlayerHealthComponent>();
    screenEffectComp_ = gameObject_->GetComponent<ScreenEffectComponent>();

    if (auto scene = gameObject_->GetScene()) {
        if (auto mainCam = scene->FindGameObject("MainCamera")) {
            mainCameraObj_ = mainCam;
            cameraShakeComp_ = mainCam->GetComponent<CameraShakeComponent>();
        }
    }

    if (healthComp_) {
        // 被弾イベントリスナーを登録
        healthComp_->AddOnDamageTakenListener([this](int damage) {
            (void)damage;
            TriggerDamageFlash();
            TriggerCameraShake();
            TriggerScreenEffect();
        });

        // 死亡イベントリスナーを登録
        healthComp_->AddOnPlayerDiedListener([this]() { TriggerDeathVisuals(); });
    }
}

BaseModel* PlayerDamageVisualizerComponent::GetTargetModel() {
    if (!gameObject_) {
        return nullptr;
    }

    // dynamic_cast による型安全なモデルポインタ取得
    if (auto mesh = gameObject_->GetComponent<MeshRendererComponent>()) {
        if (auto renderable = mesh->GetRenderable()) {
            return dynamic_cast<BaseModel*>(renderable);
        }
    } else if (auto skinned = gameObject_->GetComponent<SkinnedMeshRendererComponent>()) {
        if (auto renderable = skinned->GetRenderable()) {
            return dynamic_cast<BaseModel*>(renderable);
        }
    }

    return nullptr;
}

void PlayerDamageVisualizerComponent::TriggerDamageFlash() {
    if (healthComp_) {
        flashRemaining_ = healthComp_->GetMaxInvincibilityTime();
    } else {
        flashRemaining_ = flashDuration_;
    }

    isFlashing_ = true;
    flashTimer_ = 0.0f;

    BaseModel* model = GetTargetModel();
    if (model && !colorCached_) {
        originalBaseColor_ = model->GetColor();
        colorCached_ = true;
    }
}

void PlayerDamageVisualizerComponent::TriggerCameraShake() {
    if (!cameraShakeComp_ || mainCameraObj_.expired()) {
        cameraShakeComp_ = nullptr;
        if (gameObject_) {
            if (auto scene = gameObject_->GetScene()) {
                if (auto mainCam = scene->FindGameObject("MainCamera")) {
                    mainCameraObj_ = mainCam;
                    cameraShakeComp_ = mainCam->GetComponent<CameraShakeComponent>();
                }
            }
        }
    }

    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShake(shakeIntensity_, shakeFrames_, shakeFrequency_);
    }
}

void PlayerDamageVisualizerComponent::TriggerScreenEffect() {
    if (!screenEffectComp_ && gameObject_) {
        screenEffectComp_ = gameObject_->GetComponent<ScreenEffectComponent>();
    }

    if (screenEffectComp_) {
        screenEffectComp_->Play();
    }
}

void PlayerDamageVisualizerComponent::TriggerDeathVisuals() {
    if (!gameObject_) {
        return;
    }

    // 自機コンポーネントを Dying ステートへ移行（入力遮断・姿勢固定）
    if (auto railPlayer = gameObject_->GetComponent<RailShooterPlayerComponent>()) {
        railPlayer->SetState(PlayerFlightState::Dying);
    }

    auto engine = GetEngine();
    auto transform = gameObject_->GetComponent<TransformComponent>();
    Irufemi::Vector3 deathPos = transform ? transform->GetWorldPosition() : Irufemi::Vector3{0, 0, 0};
    Irufemi::Vector3 deathRot = transform ? transform->GetWorldRotation() : Irufemi::Vector3{0, 0, 0};
    Irufemi::Vector3 deathScale = transform ? transform->GetWorldScale() : Irufemi::Vector3{1, 1, 1};

    // ① 自作エンジンの看板機能: 自機モデルの VoxelParticle 破砕四散爆発！
    if (auto voxelManager = engine ? engine->GetVoxelParticleManager() : nullptr) {
        VoxelEmitter p{};
        p.particleType = 5; // Explosive
        p.lifeTime = 2.0f;
        p.gravity = 6.0f;
        p.dispersion = 18.0f; // 激しい四散インパルス
        p.scale = {0.6f, 0.6f, 0.6f};
        p.startColor = {2.5f, 1.8f, 0.8f, 1.0f};        // 激しい閃光オレンジ
        p.endColor = {0.1f, 0.1f, 0.1f, 1.0f};          // 煤・燃え尽き炭
        p.dissolveEdgeColor = {0.2f, 0.8f, 1.0f, 1.0f}; // 自機シアンの余韻

        voxelManager->PlayExplosion("resources/model/PlayerCraft/PlayerCraft.obj", deathPos, deathRot,
                                    {0.0f, 0.0f, 0.0f}, deathScale, p, {3, 3, 3});
    }

    // ② 二次爆発エフェクトの重畳（火球・閃光）
    if (auto fxMgr = EffectManagerComponent::GetInstance()) {
        fxMgr->PlayEffect("Hit", deathPos);
    }

    // ③ 大インパルス・カメラシェイクを発火
    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShakeSeconds(2.0f, 0.6f, 25.0f);
    }

    // ④ 自機モデルを非表示化
    if (auto mesh = gameObject_->GetComponent<MeshRendererComponent>()) {
        mesh->SetVisible(false);
    } else if (auto skinned = gameObject_->GetComponent<SkinnedMeshRendererComponent>()) {
        skinned->SetVisible(false);
    }

    // ⑤ 点滅状態を解除し元の色に戻す
    if (isFlashing_) {
        isFlashing_ = false;
        if (BaseModel* model = GetTargetModel()) {
            if (colorCached_) {
                model->SetColor(originalBaseColor_);
            }
        }
    }
}

void PlayerDamageVisualizerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    // 点滅処理の更新
    if (isFlashing_) {
        flashRemaining_ -= dt;
        flashTimer_ += dt;

        BaseModel* model = GetTargetModel();
        if (model) {
            if (!colorCached_) {
                originalBaseColor_ = model->GetColor();
                colorCached_ = true;
            }

            if (fmod(flashTimer_, flashInterval_ * 2.0f) < flashInterval_) {
                model->SetColor(flashColor_);
            } else {
                model->SetColor(originalBaseColor_);
            }
        }

        if (flashRemaining_ <= 0.0f) {
            isFlashing_ = false;
            flashRemaining_ = 0.0f;
            if (model && colorCached_) {
                model->SetColor(originalBaseColor_);
            }
        }
    }
}
