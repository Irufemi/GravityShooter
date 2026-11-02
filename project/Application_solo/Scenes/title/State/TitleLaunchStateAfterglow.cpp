#include "TitleLaunchStateAfterglow.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"

void TitleLaunchStateAfterglow::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 自機を画面外へ退避（Breakの極小収縮を経て完全消失）
    if (auto ship = director->GetShipObject().lock()) {
        ship->SetActive(false);
    }

    // 星雲特異点のインプロージョン最大崩壊状態を維持（暗転完了へシームレスに合流）
    if (auto nebulaComp = director->GetNebulaComponent()) {
        nebulaComp->SetCollapseProgress(1.0f);
        nebulaComp->SetSwirlMultiplier(4.5f);
    }

    // 安全弁: Break側で未発火だった場合は即座に重力特異点跳躍（暗転）を発火
    if (!director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition();
    }
}

void TitleLaunchStateAfterglow::Update(TitleSceneDirectorComponent* director, float dt) {
    if (!director) {
        return;
    }

    auto& asteroids = director->GetAsteroidInstances();

    // 暗転完了（シーン切り替え）までの短い余韻中、小惑星の自転を滑らかに維持
    for (size_t i = 0; i < asteroids.size(); ++i) {
        asteroids[i].rotation.y += dt * 0.2f * static_cast<float>(i + 1);
    }
}
