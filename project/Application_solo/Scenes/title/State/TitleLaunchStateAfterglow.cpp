#include "TitleLaunchStateAfterglow.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
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

    // 安全弁: Break側で未発火だった場合は即座に重力特異点跳躍（暗転）を発火
    if (!director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.45f);
    }
}

void TitleLaunchStateAfterglow::Update(TitleSceneDirectorComponent* director, float dt) {
    if (!director) {
        return;
    }

    const auto& debrisObjs = director->GetDebrisObjects();

    // 暗転完了（シーン切り替え）までの短い余韻中、ガレキの自転を滑らかに維持
    for (size_t i = 0; i < debrisObjs.size(); ++i) {
        if (auto debris = debrisObjs[i].lock()) {
            if (auto dtComp = debris->GetTransform()) {
                auto rot = dtComp->GetRotation();
                rot.y += dt * 0.2f * (i + 1);
                dtComp->SetRotation(rot);
            }
        }
    }
}
