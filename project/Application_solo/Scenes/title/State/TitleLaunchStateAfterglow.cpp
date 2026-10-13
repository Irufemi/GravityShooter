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

    // 自機消失の直後（0.05秒）で即座に重力特異点跳躍を発火し、間延びを完全排除
    if (stateTimer >= 0.05f && !director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.45f);
    }
}
