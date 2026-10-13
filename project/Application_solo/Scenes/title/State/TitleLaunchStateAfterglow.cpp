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

    // ジェット音（se_player_boost.mp3: 実測3.55秒）の遠ざかり残響・ドップラー消音と完全同期
    // 点火からの経過時間: Accelerate(1.10s) + Break(0.60s) + Afterglow待機(1.25s) = 2.95s
    // ここで 0.60秒の重力特異点跳躍（暗転）を発火することで、2.95s + 0.60s = 3.55秒の消音完了瞬間にジャスト暗転完了
    if (stateTimer >= 1.25f && !director->HasTriggeredSceneTransition()) {
        director->TriggerSceneTransition(0.60f);
    }
}
