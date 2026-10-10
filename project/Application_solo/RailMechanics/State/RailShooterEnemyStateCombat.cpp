#include "RailMechanics/State/RailShooterEnemyStateCombat.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "RailMechanics/Strategy/IEnemyAttackStrategy.h"
#include "RailMechanics/State/RailShooterEnemyStateDisengage.h"
#include <cmath>

void RailShooterEnemyStateCombat::Enter(RailShooterEnemyComponent* enemy) {
    stateTimer_ = 0.0f;
    hoverTimer_ = 0.0f;
    if (enemy && enemy->GetAttackStrategy()) {
        enemy->GetAttackStrategy()->OnEnterCombat(enemy);
    }
}

void RailShooterEnemyStateCombat::Update(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    // 自機と等速で前方一定距離を完全維持
    enemy->SetCurrentDistanceOffset(enemy->GetTargetDistance());

    // レール局所断面での浮遊運動（サイン・コサイン波）
    hoverTimer_ += dt;
    const auto& baseOffset = enemy->GetBaseFormationOffset();
    Irufemi::Vector2 localOffset;
    localOffset.x = baseOffset.x + std::sin(hoverTimer_ * 2.5f) * 4.0f;
    localOffset.y = baseOffset.y + std::cos(hoverTimer_ * 2.0f) * 2.5f;
    enemy->SetCurrentLocalOffset(localOffset);

    // Strategy Pattern による攻撃・照準・予兆アルゴリズムの多態的実行
    if (auto strategy = enemy->GetAttackStrategy()) {
        strategy->UpdateCombat(enemy, dt);
    }

    // 一定時間経過で離脱フェーズへ遷移
    stateTimer_ += dt;
    if (stateTimer_ >= enemy->GetCombatDuration()) {
        enemy->ChangeState(std::make_unique<RailShooterEnemyStateDisengage>());
    }
}

void RailShooterEnemyStateCombat::Exit(RailShooterEnemyComponent* enemy) {
    if (enemy && enemy->GetAttackStrategy()) {
        enemy->GetAttackStrategy()->OnExitCombat(enemy);
    }
}
