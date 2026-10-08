#pragma once
#include "RailMechanics/State/IRailShooterEnemyState.h"

/**
 * @class RailShooterEnemyStateApproach
 * @brief 敵キャラクターの進入フェーズ（初期位置から交戦距離への接近）
 */
class RailShooterEnemyStateApproach : public IRailShooterEnemyState {
public:
    RailShooterEnemyStateApproach() = default;
    ~RailShooterEnemyStateApproach() override = default;

    void Enter(RailShooterEnemyComponent* enemy) override;
    void Update(RailShooterEnemyComponent* enemy, float dt) override;
    void Exit(RailShooterEnemyComponent* enemy) override;

    EnemyAIState GetStateType() const override {
        return EnemyAIState::Approach;
    }

private:
    float stateTimer_ = 0.0f; //!< 進入タイマー
};
