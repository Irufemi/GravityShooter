#pragma once
#include "RailMechanics/State/IRailShooterEnemyState.h"

/**
 * @class RailShooterEnemyStateDive
 * @brief 敵キャラクターの特攻急降下フェーズ（DiveBomber専用: 自機正面ラインへ収束しつつ急加速突進）
 */
class RailShooterEnemyStateDive : public IRailShooterEnemyState {
public:
    RailShooterEnemyStateDive() = default;
    ~RailShooterEnemyStateDive() override = default;

    void Enter(RailShooterEnemyComponent* enemy) override;
    void Update(RailShooterEnemyComponent* enemy, float dt) override;
    void Exit(RailShooterEnemyComponent* enemy) override;

    EnemyAIState GetStateType() const override {
        return EnemyAIState::Dive;
    }
};
