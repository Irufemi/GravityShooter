#pragma once
#include "RailMechanics/Strategy/IEnemyAttackStrategy.h"

/**
 * @class EnemyAttackStrategyNormal
 * @brief 通常機（StandardGunner）の攻撃戦略：一定周期でプレイヤーの現在位置へ直接弾を発射
 */
class EnemyAttackStrategyNormal : public IEnemyAttackStrategy {
public:
    EnemyAttackStrategyNormal() = default;
    ~EnemyAttackStrategyNormal() override = default;

    void OnEnterCombat(RailShooterEnemyComponent* enemy) override;
    void UpdateCombat(RailShooterEnemyComponent* enemy, float dt) override;
    void OnExitCombat(RailShooterEnemyComponent* enemy) override;

private:
    float shootTimer_ = 0.6f; //!< 射撃タイマー
};
