#include "TitleLaunchStateAccelerate.h"
#include "TitleLaunchStateBreak.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include <algorithm>
#include <cmath>

void TitleLaunchStateAccelerate::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 臨界蓄圧（マイクロフリーズ）開始：直ちにガレキ破砕せず、0.045秒のタメを作る
    director->SetMicroFreezing(true);
    director->SetFreezeTimer(0.0f);
    director->SetTriggeredRelease(false);

    // 自機スラスターノズル内のGPUパーティクルを瞬間オーバードライブ再点火
    if (auto thruster = director->GetThrusterObject().lock()) {
        auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto pe : emitters) {
            pe->Restart(false);
        }
    }

    // 予兆としての単発衝撃インパルス（振幅 0.02m の瞬間シェイク）
    if (auto camObj = director->GetCameraObject().lock()) {
        auto shake = camObj->GetComponent<CameraShakeComponent>();
        if (!shake) {
            auto newShake = camObj->AddComponent<CameraShakeComponent>();
            shake = newShake.get();
        }
        if (shake) {
            shake->PlayShakeSeconds(0.02f, TitleSceneDirectorComponent::kDurationFreeze_, 30.0f);
        }
    }
}

void TitleLaunchStateAccelerate::Update(TitleSceneDirectorComponent* director, float dt) {
    if (!director) {
        return;
    }

    float stateTimer = director->GetLaunchStateTimer();
    auto shipTransform = director->GetShipTransform();
    auto cameraTransform = director->GetCameraTransform();
    auto camComp = director->GetCameraComponent();
    const auto& initialShipPos = director->GetInitialShipPos();
    const auto& initialCameraPos = director->GetInitialCameraPos();

    // --- 臨界蓄圧（マイクロフリーズ・身震い）制御 ---
    if (director->IsMicroFreezing()) {
        director->AddFreezeTimer(dt);

        // 0.045秒間は前進をロックし、超推力に耐える80Hz高周波ジッター（身震い）を重畳
        if (director->GetFreezeTimer() < TitleSceneDirectorComponent::kDurationFreeze_) {
            if (shipTransform) {
                float jitter = std::sin(director->GetFreezeTimer() * 180.0f) * 0.008f;
                shipTransform->SetPosition({initialShipPos.x + jitter, initialShipPos.y, initialShipPos.z - 0.45f});
                shipTransform->SetRotation({0.0f, 0.0f, jitter * 1.5f});
            }
            return; // フリーズ待機（前進加速・破砕展開を一時保留）
        }

        // 臨界解放マイルストーン発火！（大爆発・Voxel破砕・多重音響・激震の解き放ち）
        director->TriggerImpactRelease();
    }

    // フリーズ解除後の実効加速時間
    float effectiveTimer = stateTimer - TitleSceneDirectorComponent::kDurationFreeze_;
    float p2 = std::clamp(effectiveTimer / (TitleSceneDirectorComponent::kDurationAccelerate_ -
                                            TitleSceneDirectorComponent::kDurationFreeze_),
                          0.0f, 1.0f);
    float accelCurve = p2 * p2 * p2 * p2; // 4次急加速曲線（初速ゼロから猛烈な爆発的射出）

    // 自機の超推力突進
    if (shipTransform) {
        float boostZ = initialShipPos.z - 0.45f + accelCurve * 65.0f;
        shipTransform->SetPosition({initialShipPos.x, initialShipPos.y, boostZ});
        shipTransform->SetRotation({0.0f, 0.0f, 0.0f});
    }

    // カメラ追従ドリーイン
    if (cameraTransform) {
        float camDollyZ = initialCameraPos.z + accelCurve * 52.0f;
        cameraTransform->SetPosition({initialCameraPos.x, initialCameraPos.y, camDollyZ});
    }

    // 動的FOV（ワープスピード効果）: 加速中盤で画面周辺がワイドに歪む
    if (camComp) {
        float fovSin = std::sin(p2 * 3.14159265f); // 0 -> 1 -> 0
        float extraFov = (16.0f * 3.14159265f / 180.0f) * fovSin;
        camComp->SetFovAngleY(director->GetInitialCameraFov() + extraFov);
    }

    // スラスター急伸長
    director->SetTargetThrusterScaleZ(std::lerp(1.2f, 2.4f, p2));

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationAccelerate_) {
        director->ChangeState(std::make_unique<TitleLaunchStateBreak>());
    }
}
