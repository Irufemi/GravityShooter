#pragma once
#include "RailMechanics/Strategy/IEnemyAttackStrategy.h"

/**
 * @class EnemyAttackStrategyPredictiveSniper
 * @brief 偏差スナイパー（PredictiveSniper）の攻撃戦略：予兆線追従 ➔ 射線ロックオン ➔ 未来予測偏差射撃
 */
class EnemyAttackStrategyPredictiveSniper : public IEnemyAttackStrategy {
public:
    EnemyAttackStrategyPredictiveSniper() = default;
    ~EnemyAttackStrategyPredictiveSniper() override = default;

    void OnEnterCombat(RailShooterEnemyComponent* enemy) override;
    void UpdateCombat(RailShooterEnemyComponent* enemy, float dt) override;
    void OnExitCombat(RailShooterEnemyComponent* enemy) override;

private:
    float shootTimer_ = 1.2f; //!< 予兆および射撃タイマー
};
