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
    auto camComp = director->GetCameraComponent();
    const auto& initialShipPos = director->GetInitialShipPos();

    // 自機は光の彼方へ突き抜け、Scaleを急速に縮小して光の点へ
    if (shipTransform) {
        float boostZ = initialShipPos.z - 0.45f + 65.0f + p3 * 75.0f;
        shipTransform->SetPosition({initialShipPos.x, initialShipPos.y, boostZ});

        float shrinkScale = std::lerp(1.0f, 0.05f, p3);
        shipTransform->SetScale({shrinkScale, shrinkScale, shrinkScale});
    }

    // FOVを通常視野角へスムーズに戻す
    if (camComp) {
        float initialFov = director->GetInitialCameraFov();
        float returnFov = std::lerp(initialFov + (16.0f * 3.14159265f / 180.0f) * 0.15f, initialFov, p3);
        camComp->SetFovAngleY(returnFov);
    }

    director->SetTargetThrusterScaleZ(2.4f);

    // 状態遷移判定
    if (stateTimer >= TitleSceneDirectorComponent::kDurationBreak_) {
        director->ChangeState(std::make_unique<TitleLaunchStateAfterglow>());
    }
}
