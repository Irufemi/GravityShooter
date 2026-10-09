#include "RailMechanics/Strategy/EnemyAttackStrategyNormal.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"

void EnemyAttackStrategyNormal::OnEnterCombat(RailShooterEnemyComponent* enemy) {
    shootTimer_ = 0.6f; // 初弾タイマー
    if (enemy) {
        enemy->SetShootTimer(shootTimer_);
    }
}

void EnemyAttackStrategyNormal::UpdateCombat(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    shootTimer_ -= dt;
    enemy->SetShootTimer(shootTimer_);

    if (shootTimer_ <= 0.0f) {
        GameObject* playerObj = enemy->GetPlayerObject();
        if (playerObj && playerObj->GetTransform()) {
            Irufemi::Vector3 playerPos = playerObj->GetTransform()->GetWorldPosition();
            enemy->ShootAtPlayer(playerPos);
        }
        shootTimer_ = enemy->GetShootInterval();
    }
}

void EnemyAttackStrategyNormal::OnExitCombat(RailShooterEnemyComponent* /*enemy*/) {
    // 通常機の離脱時処理（必要に応じて拡張）
}
