#include "RailMechanics/Strategy/EnemyAttackStrategyDiveBomb.h"
#include "RailMechanics/RailShooterEnemyComponent.h"

void EnemyAttackStrategyDiveBomb::OnEnterCombat(RailShooterEnemyComponent* /*enemy*/) {
    // 特攻機は射撃を行わない
}

void EnemyAttackStrategyDiveBomb::UpdateCombat(RailShooterEnemyComponent* /*enemy*/, float /*dt*/) {
    // 特攻機は射撃を行わず、突撃フェーズへの移行を待つ
}

void EnemyAttackStrategyDiveBomb::OnExitCombat(RailShooterEnemyComponent* /*enemy*/) {
    // クリーンアップ
}
