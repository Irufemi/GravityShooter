#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Combat/Boss/BossComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include <algorithm>

void BossDamageVisualizerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Damage Shake Intensity", &damageShakeIntensity_);
    RegisterProperty("Damage Shake Duration", &damageShakeDuration_);
    RegisterProperty("Damage Shake Frequency", &damageShakeFrequency_);
    RegisterProperty("Death Shake Intensity", &deathShakeIntensity_);
    RegisterProperty("Death Shake Duration", &deathShakeDuration_);
    RegisterProperty("Death Shake Frequency", &deathShakeFrequency_);
}

void BossDamageVisualizerComponent::Initialize() {
    mainCameraObj_.reset();
}

void BossDamageVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    bossComp_ = gameObject_->GetComponent<BossComponent>();
    if (bossComp_) {
        std::weak_ptr<GameObject> weakObj = gameObject_->shared_from_this();

        // 被弾イベントリスナーを登録
        bossComp_->AddOnDamageTakenListener([weakObj](float damage) {
            if (auto obj = weakObj.lock()) {
                if (auto visualizer = obj->GetComponent<BossDamageVisualizerComponent>()) {
                    visualizer->TriggerDamageShake(damage);
                }
            }
        });

        // 撃破イベントリスナーを登録
        bossComp_->AddOnBossDiedListener([weakObj]() {
            if (auto obj = weakObj.lock()) {
                if (auto visualizer = obj->GetComponent<BossDamageVisualizerComponent>()) {
                    visualizer->TriggerDeathShake();
                }
            }
        });
    }
}

std::shared_ptr<GameObject> BossDamageVisualizerComponent::GetMainCamera() const {
    if (auto cam = mainCameraObj_.lock()) {
        return cam;
    }
    if (gameObject_) {
        if (auto scene = gameObject_->GetScene()) {
            if (auto cam = scene->FindGameObject("MainCamera")) {
                mainCameraObj_ = cam;
                return cam;
            }
        }
    }
    return nullptr;
}

CameraShakeComponent* BossDamageVisualizerComponent::GetCameraShake() const {
    if (auto cam = GetMainCamera()) {
        return cam->GetComponent<CameraShakeComponent>();
    }
    return nullptr;
}

void BossDamageVisualizerComponent::TriggerDamageShake(float damage) {
    if (auto shake = GetCameraShake()) {
        // 基準ダメージ(50.0f)に対するスケーリング係数 (0.5倍 〜 2.0倍の範囲でクランプ)
        float factor = std::clamp(damage / 50.0f, 0.5f, 2.0f);
        float intensity = damageShakeIntensity_ * factor;
        float duration = damageShakeDuration_ * factor;
        float frequency = damageShakeFrequency_ * factor;

        shake->PlayShakeSeconds(intensity, duration, frequency);
    }
}

void BossDamageVisualizerComponent::TriggerDeathShake() {
    if (auto shake = GetCameraShake()) {
        shake->PlayShakeSeconds(deathShakeIntensity_, deathShakeDuration_, deathShakeFrequency_);
    }
}

bool BossDamageVisualizerComponent::IsDeathShakePlaying() const {
    if (auto shake = GetCameraShake()) {
        return shake->IsPlaying();
    }
    return false;
}
