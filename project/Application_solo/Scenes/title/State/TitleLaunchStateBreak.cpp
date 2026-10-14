#include "TitleLaunchStateBreak.h"
#include "TitleLaunchStateAfterglow.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"

void TitleLaunchStateBreak::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 1. 自機が正面の重力の壁（中心特異点）に激突！
    // GPGPU流体場（FluidSimulation2D）へ超強度スワール＋8方向放射状外力＋シアン染料を一斉注入！
    if (auto nebulaComp = director->GetNebulaComponent()) {
        nebulaComp->TriggerWallImpactShockwave(Irufemi::Vector2{0.5f, 0.42f}, 1.35f);
    }

    // 2. 激突瞬間の空間インパルス（振幅 0.035m の正面衝撃波シェイク）
    if (auto camObj = director->GetCameraObject().lock()) {
        auto shake = camObj->GetComponent<CameraShakeComponent>();
        if (!shake) {
            auto newShake = camObj->AddComponent<CameraShakeComponent>();
            shake = newShake.get();
        }
        if (shake) {
            shake->PlayShakeSeconds(0.035f, 0.45f, 24.0f);
        }
    }

    // 3. 空間歪曲・重力波突破音 (低音ドローン＋空間スウォッシュ) を発火
    if (auto engine = director->GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->PlayByFileWithFade("resources/audio/SE/se_gravity_warp.mp3", 0.9f, 0.5f, 0.95f, AudioCategory::SE);
        }
    }
}

void TitleLaunchStateBreak::Update(TitleSceneDirectorComponent* director, float dt) {
    (void)dt;
    if (!director) {
        return;
    }

    float stateTimer = director->GetLaunchStateTimer();
    float p3 = std::clamp(stateTimer / TitleSceneDirectorComponent::kDurationBreak_, 0.0f, 1.0f);

    auto shipTransform = director->GetShipTransform();
    auto cameraTransform = director->GetCameraTransform();
    auto camComp = director->GetCameraComponent();
    const auto& initialShipPos = director->GetInitialShipPos();
    const auto& initialCameraPos = director->GetInitialCameraPos();

    // 1. 自機は正面の重力の壁（奥の特異点）へ向かって猛スピードでダイブ
    float startShipZ = initialShipPos.z - 0.45f + 50.0f;
    float boostZ = startShipZ + p3 * 100.0f;
    if (shipTransform) {
        shipTransform->SetPosition({initialShipPos.x, 0.52f, boostZ});
        shipTransform->SetScale({1.0f, 1.0f, 1.0f});
    }

    // 2. カメラは前進の勢いをそのまま維持し、重力波の膜の目前までドリーイン
    if (cameraTransform) {
        float startCamZ = initialCameraPos.z + 22.0f;
        float camDollyZ = startCamZ + p3 * 25.0f;
        cameraTransform->SetPosition({initialCameraPos.x, initialCameraPos.y, camDollyZ});
    }

    // 3. ハイパースペース動的FOV
    if (camComp) {
        float initialFov = director->GetInitialCameraFov();
        float warpFov = initialFov + (18.0f * 3.14159265f / 180.0f) + (6.0f * 3.14159265f / 180.0f) * p3;
        camComp->SetFovAngleY(warpFov);
    }

    director->SetTargetThrusterScaleZ(2.8f);

    // 4. 重力特異点跳躍（暗転）の発火
    // 自機が奥の重力の壁にドカンと突入し、激しい流体の乱れが広がった瞬間（p3 >= 0.55）に暗転へ移行
    if (p3 >= 0.55f && !director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.35f);
    }

    // 5. 自機突入に呼応する星雲特異点のインプロージョン（事象の地平線爆発的拡大＆渦超加速）
    if (auto nebulaComp = director->GetNebulaComponent()) {
        nebulaComp->SetCollapseProgress(p3);
        nebulaComp->SetSwirlMultiplier(std::lerp(2.2f, 5.0f, p3));
    }

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationBreak_) {
        director->ChangeState(std::make_unique<TitleLaunchStateAfterglow>());
    }
}
