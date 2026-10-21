#include "TitleLaunchStateAccelerate.h"
#include "TitleLaunchStateBreak.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include <algorithm>
#include <cmath>

void TitleLaunchStateAccelerate::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 1. チャージSEの強制停止（真空の静寂を作り出す）
    director->StopChargeVoice();

    // 2. 臨界蓄圧（マイクロフリーズ）開始：直ちにガレキ破砕せず、0.045秒のタメを作る
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

    // 予兆としての単発衝撃インパルス（振幅 0.035m の瞬間シェイク）
    if (auto camObj = director->GetCameraObject().lock()) {
        if (auto shake = camObj->GetComponent<CameraShakeComponent>()) {
            shake->PlayShakeSeconds(0.035f, TitleSceneDirectorComponent::kDurationFreeze_, 28.0f);
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

        // 0.15秒間は前進をロックし、超推力に耐える強烈な高周波ジッター（軋む身震い）を重畳
        if (director->GetFreezeTimer() < TitleSceneDirectorComponent::kDurationFreeze_) {
            if (shipTransform) {
                float timer = director->GetFreezeTimer();
                float jitterX = std::sin(timer * 160.0f) * 0.015f;
                float jitterY = std::cos(timer * 130.0f) * 0.008f;
                shipTransform->SetPosition({initialShipPos.x + jitterX, initialShipPos.y + jitterY, initialShipPos.z - 0.45f});
                shipTransform->SetRotation({0.0f, 0.0f, jitterX * 2.5f});
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

    // 自機の超推力突進（画面中央の特異点へ向けて自然にスロープ上昇しながら前進）
    if (shipTransform) {
        float boostZ = initialShipPos.z - 0.45f + accelCurve * 70.0f;
        float targetY = std::lerp(initialShipPos.y, 0.52f, accelCurve);
        shipTransform->SetPosition({initialShipPos.x, targetY, boostZ});
        shipTransform->SetRotation({0.0f, 0.0f, 0.0f});
    }

    // カメラ滑らかドリーイン（自機を見送りつつ衝撃波に乗って前進）
    if (cameraTransform) {
        float camDollyZ = initialCameraPos.z + accelCurve * 28.0f;
        cameraTransform->SetPosition({initialCameraPos.x, initialCameraPos.y, camDollyZ});
    }

    // 動的FOV（ワープスピード効果）: 加速に合わせて自然にワイドに広がる
    if (camComp) {
        float extraFov = (18.0f * 3.14159265f / 180.0f) * (p2 * p2);
        camComp->SetFovAngleY(director->GetInitialCameraFov() + extraFov);
    }

    // スラスター急伸長
    director->SetTargetThrusterScaleZ(std::lerp(1.2f, 2.4f, p2));

    // 星雲渦の連動加速（自機の加速に伴い、角運動量注入で渦の回転が立ち上がる）
    if (auto nebulaComp = director->GetNebulaComponent()) {
        nebulaComp->SetSwirlMultiplier(std::lerp(1.0f, 2.2f, accelCurve));
    }

    // 遠景・中景小惑星群のスラスター衝撃波散開（Blast Wave）更新
    for (auto& ast : director->GetAsteroidInstances()) {
        if (ast.blastVelocity.x != 0.0f || ast.blastVelocity.y != 0.0f || ast.blastVelocity.z != 0.0f) {
            ast.currentPos.x += ast.blastVelocity.x * dt * p2 * 1.8f;
            ast.currentPos.y += ast.blastVelocity.y * dt * p2 * 1.8f;
            ast.currentPos.z += ast.blastVelocity.z * dt * p2 * 1.8f;
            ast.rotation.x += dt * 3.5f;
            ast.rotation.y += dt * 5.0f;
        }
    }

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationAccelerate_) {
        director->ChangeState(std::make_unique<TitleLaunchStateBreak>());
    }
}
