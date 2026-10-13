#include "TitleLaunchStateBreak.h"
#include "TitleLaunchStateAfterglow.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include <algorithm>
#include <cmath>

void TitleLaunchStateBreak::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 空間歪曲・重力ワープ通過音 (低音ドローン＋空間スウォッシュ) を発火
    // 遷移時間（約1.2秒）に合わせて0.7秒持続 + 0.5秒フェードアウトで自動停止（長尺音の背景残留を完全防止）
    if (auto engine = director->GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->PlayByFileWithFade("resources/audio/SE/se_gravity_warp.mp3", 0.7f, 0.5f, 0.90f, AudioCategory::SE);
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

    // 1. 自機は特異点の中心（奥の渦）へ向かって光速ダイブ
    float startShipZ = initialShipPos.z - 0.45f + 50.0f;
    float boostZ = startShipZ + p3 * 75.0f;
    if (shipTransform) {
        shipTransform->SetPosition({initialShipPos.x, 0.52f, boostZ});
        shipTransform->SetScale({1.0f, 1.0f, 1.0f});
    }

    // 2. カメラは前進の勢いをそのまま維持（急接近の不連続性を完全排除）
    if (cameraTransform) {
        float startCamZ = initialCameraPos.z + 22.0f;
        float camDollyZ = startCamZ + p3 * 25.0f;
        cameraTransform->SetPosition({initialCameraPos.x, initialCameraPos.y, camDollyZ});
    }

    // 3. ハイパースペース動的FOV
    if (camComp) {
        float initialFov = director->GetInitialCameraFov();
        float warpFov = initialFov + (18.0f * 3.14159265f / 180.0f) + (10.0f * 3.14159265f / 180.0f) * p3;
        camComp->SetFovAngleY(warpFov);
    }

    director->SetTargetThrusterScaleZ(2.8f);

    // 4. 自機が特異点へ激突・事象の地平線へ突入した瞬間に重力特異点跳躍を発火！
    // 0.48秒の時間を確保し、空間がねじ曲がり深淵へ引きずり込まれる映像美をしっかり演出
    if (p3 >= 0.10f && !director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.48f);
    }

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationBreak_) {
        director->ChangeState(std::make_unique<TitleLaunchStateAfterglow>());
    }
}
