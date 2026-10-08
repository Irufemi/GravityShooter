#pragma once
#include "RailMechanics/State/IRailShooterEnemyState.h"

/**
 * @class RailShooterEnemyStateDisengage
 * @brief 敵キャラクターの離脱フェーズ（制限時間終了によるすれ違い離脱・後方退場）
 */
class RailShooterEnemyStateDisengage : public IRailShooterEnemyState {
public:
    RailShooterEnemyStateDisengage() = default;
    ~RailShooterEnemyStateDisengage() override = default;

    void Enter(RailShooterEnemyComponent* enemy) override;
    void Update(RailShooterEnemyComponent* enemy, float dt) override;
    void Exit(RailShooterEnemyComponent* enemy) override;

    EnemyAIState GetStateType() const override {
        return EnemyAIState::Disengage;
    }
};
