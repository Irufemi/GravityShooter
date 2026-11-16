#include "Combat/Boss/BossStateIdle.h"
#include "Combat/Boss/BossStateCoreExposed.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/EnemyBeamComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <memory>

void BossStateIdle::Enter(BossComponent* boss) {
    Irufemi::Log::OutPutLog(std::cout, "Boss entered Idle State (Shield Active)\n");
}

void BossStateIdle::Update(BossComponent* boss) {
    if (!boss || !boss->GetGameObject()) {
        return;
    }

    // --- ビーム攻撃の更新 ---
    auto engine = boss->GetEngine();
    if (!engine) {
        return;
    }
    float deltaTime = engine->GetGameDeltaTime();
    if (deltaTime <= 0.0f) {
        return;
    }
    boss->UpdateBeamAttack(deltaTime);

    // CoreExposed への遷移チェック
    if (boss->IsShieldDepleted()) {
        boss->ChangeState(std::make_unique<BossStateCoreExposed>());
    }
}

void BossStateIdle::Exit(BossComponent* boss) {}

void BossStateIdle::OnTakeDamage(BossComponent* boss, float damage) {
    Irufemi::Log::OutPutLog(std::cout, "Boss blocked damage with shield!\n");
}
