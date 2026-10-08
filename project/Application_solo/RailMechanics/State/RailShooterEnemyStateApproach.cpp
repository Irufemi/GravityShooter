#include "RailMechanics/State/RailShooterEnemyStateApproach.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "RailMechanics/State/RailShooterEnemyStateCombat.h"
#include "RailMechanics/State/RailShooterEnemyStateDive.h"
#include <cmath>

void RailShooterEnemyStateApproach::Enter(RailShooterEnemyComponent* enemy) {
    stateTimer_ = 0.0f;
    if (enemy) {
        enemy->SetCurrentLocalOffset(enemy->GetBaseFormationOffset());
    }
}

void RailShooterEnemyStateApproach::Update(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    float currentDist = enemy->GetCurrentDistanceOffset();
    float targetDist = enemy->GetTargetDistance();
    float speed = enemy->GetSpeed();

    // 初期距離オフセットから目標交戦距離 (targetDist) へスムーズにレール上を接近
    float diffDist = targetDist - currentDist;
    if (std::abs(diffDist) > 0.5f) {
        float moveDir = (diffDist > 0.0f) ? 1.0f : -1.0f;
        currentDist += moveDir * speed * 1.5f * dt;
        if ((moveDir > 0.0f && currentDist > targetDist) || (moveDir < 0.0f && currentDist < targetDist)) {
            currentDist = targetDist;
        }
    } else {
        currentDist = targetDist;
    }
    enemy->SetCurrentDistanceOffset(currentDist);
    enemy->SetCurrentLocalOffset(enemy->GetBaseFormationOffset());

    stateTimer_ += dt;
    if (std::abs(currentDist - targetDist) < 2.0f || stateTimer_ >= 3.0f) {
        if (enemy->GetBehaviorType() == EnemyBehaviorType::DiveBomber) {
            enemy->ChangeState(std::make_unique<RailShooterEnemyStateDive>());
        } else {
            enemy->ChangeState(std::make_unique<RailShooterEnemyStateCombat>());
        }
    }
}

void RailShooterEnemyStateApproach::Exit(RailShooterEnemyComponent* /*enemy*/) {
    // 進入完了時の後処理（必要に応じて拡張）
}
