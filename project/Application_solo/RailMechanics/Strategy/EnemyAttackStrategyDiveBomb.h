#pragma once
#include "RailMechanics/Strategy/IEnemyAttackStrategy.h"

/**
 * @class EnemyAttackStrategyDiveBomb
 * @brief 特攻機（DiveBomber）の攻撃戦略：射撃を行わず突撃・急降下に専念
 */
class EnemyAttackStrategyDiveBomb : public IEnemyAttackStrategy {
public:
    EnemyAttackStrategyDiveBomb() = default;
    ~EnemyAttackStrategyDiveBomb() override = default;

    void OnEnterCombat(RailShooterEnemyComponent* enemy) override;
    void UpdateCombat(RailShooterEnemyComponent* enemy, float dt) override;
    void OnExitCombat(RailShooterEnemyComponent* enemy) override;
};
