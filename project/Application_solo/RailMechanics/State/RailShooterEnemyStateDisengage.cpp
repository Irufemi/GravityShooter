#include "RailMechanics/State/RailShooterEnemyStateDisengage.h"
#include "RailMechanics/RailShooterEnemyComponent.h"

void RailShooterEnemyStateDisengage::Enter(RailShooterEnemyComponent* /*enemy*/) {
    // 離脱開始
}

void RailShooterEnemyStateDisengage::Update(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    // 相対同期を解除し、自機の脇をすり抜けて後方へ急加速
    float currentDist = enemy->GetCurrentDistanceOffset();
    currentDist -= enemy->GetSpeed() * 2.5f * dt;
    enemy->SetCurrentDistanceOffset(currentDist);

    // 自機後方に完全に抜けたら消滅（画面外へ抜けるまで安全に生存）
    if (currentDist < -30.0f) {
        enemy->NotifyDespawn(DespawnReason::OutOfBounds);
    }
}

void RailShooterEnemyStateDisengage::Exit(RailShooterEnemyComponent* /*enemy*/) {
}
