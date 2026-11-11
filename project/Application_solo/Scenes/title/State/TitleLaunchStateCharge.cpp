#include "TitleLaunchStateCharge.h"
#include "TitleLaunchStateAccelerate.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundHelper.h"
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

    // 出撃SE再生（Springin' Sound Stock 製「気を溜める1」によるエネルギー充填・タメ）
    if (auto engine = director->GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            if (!director->GetChargeSePath().empty()) {
                const auto* fbDesc = SoundHelper::GetDescriptor(GameSE::PlayerBoost);
                std::string fallbackPath = fbDesc ? fbDesc->path : "";
                auto voice = am->PlayByFile(director->GetChargeSePath(), 0.95f, 1.0f, AudioCategory::SE, fallbackPath);
                director->SetChargeVoiceInstance(voice);
            }
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

    // 自機の沈み込み＆斜め姿勢から正面水平([0, 0, 0])への整流 ＋ 充填エネルギー蓄積による身震い
    if (shipTransform) {
        float backZ = initialShipPos.z - p1 * 0.45f;
        // チャージ進行に伴い、臨界点直前のエネルギー凝縮による微細な身震い（高周波ジッター）を付与
        float jitterAmp = p1 * p1 * 0.005f;
        float jitterX = std::sin(stateTimer * 90.0f) * jitterAmp;
        float jitterY = std::cos(stateTimer * 110.0f) * jitterAmp;

        shipTransform->SetPosition({initialShipPos.x + jitterX, initialShipPos.y + jitterY, backZ});
        shipTransform->SetRotation({std::lerp(launchStartRot.x, 0.0f, alignT),
                                    std::lerp(launchStartRot.y, 0.0f, alignT),
                                    std::lerp(launchStartRot.z, 0.0f, alignT)});
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
