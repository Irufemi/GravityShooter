#pragma once
#include "RailMechanics/State/IRailShooterEnemyState.h"

/**
 * @class RailShooterEnemyStateCombat
 * @brief 敵キャラクターの交戦フェーズ（交戦距離維持・浮遊サイン波・通常射撃/偏差予兆射撃）
 */
class RailShooterEnemyStateCombat : public IRailShooterEnemyState {
public:
    RailShooterEnemyStateCombat() = default;
    ~RailShooterEnemyStateCombat() override = default;

    void Enter(RailShooterEnemyComponent* enemy) override;
    void Update(RailShooterEnemyComponent* enemy, float dt) override;
    void Exit(RailShooterEnemyComponent* enemy) override;

    EnemyAIState GetStateType() const override {
        return EnemyAIState::Combat;
    }

private:
    float stateTimer_ = 0.0f; //!< 交戦滞空タイマー
    float hoverTimer_ = 0.0f; //!< 浮遊サイン波タイマー
};
