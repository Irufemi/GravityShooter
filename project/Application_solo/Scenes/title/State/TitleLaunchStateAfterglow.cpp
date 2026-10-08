#include "TitleLaunchStateAfterglow.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"

void TitleLaunchStateAfterglow::Enter(TitleSceneDirectorComponent* director) {
    if (!director) {
        return;
    }

    // 自機を画面外へ退避（完全消失）
    if (auto ship = director->GetShipObject().lock()) {
        ship->SetActive(false);
    }
}

void TitleLaunchStateAfterglow::Update(TitleSceneDirectorComponent* director, float dt) {
    if (!director) {
        return;
    }

    float stateTimer = director->GetLaunchStateTimer();
    const auto& debrisObjs = director->GetDebrisObjects();

    // ガレキが静寂の中でゆっくりと通常回転へ戻る
    for (size_t i = 0; i < debrisObjs.size(); ++i) {
        if (auto debris = debrisObjs[i].lock()) {
            if (auto dtComp = debris->GetTransform()) {
                auto rot = dtComp->GetRotation();
                rot.y += dt * 0.2f * (i + 1);
                dtComp->SetRotation(rot);
            }
        }
    }

    // 余韻開始から 0.50秒（全体約2.70秒地点）で 指定シーンへの優雅なフェードアウト（0.6秒）を発火
    if (stateTimer >= 0.50f && !director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.6f);
    }
}
