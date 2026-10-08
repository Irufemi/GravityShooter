#include "TitleLaunchStateCharge.h"
#include "TitleLaunchStateAccelerate.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include <algorithm>
#include <cmath>

void TitleLaunchStateCharge::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    auto shipTransform = director->GetShipTransform();
    if (shipTransform) {
        director->SetLaunchStartRot(shipTransform->GetRotation());
    } else {
        director->SetLaunchStartRot(director->GetInitialShipRot());
    }

    // 神秘的な星雲の重力パルス第1波（中心収束・発光）
    if (auto nebulaComp = director->GetNebulaComponent()) {
        nebulaComp->TriggerPulse(1.2f);
    }

    // 出撃SE再生（Springin' Sound Stock 製「気を溜める1」によるエネルギー充填・タメ）
    if (auto engine = director->GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->PlayByFile("resources/audio/SE/se_launch_charge.mp3", 0.95f, 1.0f, AudioCategory::SE,
                           "resources/audio/SE/se_player_boost.mp3");
        }
    }

    // タイトルBGMの音量をキャッシュ（即座に停止せず、Charge進行に合わせて対数フェードアウト）
    if (auto scene = director->GetScene()) {
        if (auto bgmObj = scene->FindGameObject("BGMPlayer")) {
            if (auto audioSource = bgmObj->GetComponent<AudioSourceComponent>()) {
                director->SetInitialBgmVolume(0.70f);
            }
        }
    }
}

void TitleLaunchStateCharge::Update(TitleSceneDirectorComponent* director, float dt) {
    (void)dt;
    if (!director) {
        return;
    }

    float stateTimer = director->GetLaunchStateTimer();
    float p1 = std::clamp(stateTimer / TitleSceneDirectorComponent::kDurationCharge_, 0.0f, 1.0f);
    float alignT = 1.0f - std::pow(1.0f - p1, 2.0f); // 2次イーズアウト

    auto shipTransform = director->GetShipTransform();
    const auto& initialShipPos = director->GetInitialShipPos();
    const auto& launchStartRot = director->GetLaunchStartRot();

    // 自機の沈み込み＆斜め姿勢から正面水平([0, 0, 0])への整流
    if (shipTransform) {
        float backZ = initialShipPos.z - p1 * 0.45f;
        shipTransform->SetPosition({initialShipPos.x, initialShipPos.y, backZ});
        shipTransform->SetRotation({std::lerp(launchStartRot.x, 0.0f, alignT),
                                    std::lerp(launchStartRot.y, 0.0f, alignT),
                                    std::lerp(launchStartRot.z, 0.0f, alignT)});
    }

    // ガレキが自機中心へキュッと収束
    const auto& debrisObjs = director->GetDebrisObjects();
    const auto& initialDebrisPositions = director->GetInitialDebrisPositions();
    for (size_t i = 0; i < debrisObjs.size(); ++i) {
        if (auto debris = debrisObjs[i].lock()) {
            if (auto dtComp = debris->GetTransform()) {
                if (i < initialDebrisPositions.size()) {
                    const auto& origPos = initialDebrisPositions[i];
                    dtComp->SetPosition({origPos.x * (1.0f - p1 * 0.50f), origPos.y * (1.0f - p1 * 0.50f),
                                         origPos.z * (1.0f - p1 * 0.50f)});
                }
            }
        }
    }

    // スラスター炎の引き絞り（点火直前のエネルギー圧縮）
    director->SetTargetThrusterScaleZ(0.35f);

    if (auto camComp = director->GetCameraComponent()) {
        camComp->SetFovAngleY(director->GetInitialCameraFov());
    }

    // BGMシネマティック・フェードアウト（1.5乗の対数減衰で美しく消去）
    if (auto scene = director->GetScene()) {
        if (auto bgmObj = scene->FindGameObject("BGMPlayer")) {
            if (auto audioSource = bgmObj->GetComponent<AudioSourceComponent>()) {
                float fadeT = std::pow(1.0f - p1, 1.5f);
                audioSource->SetVolume(director->GetInitialBgmVolume() * fadeT);
                if (p1 >= 1.0f) {
                    audioSource->Stop();
                }
            }
        }
    }

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationCharge_) {
        director->ChangeState(std::make_unique<TitleLaunchStateAccelerate>());
    }
}
