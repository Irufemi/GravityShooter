#include "RailMechanics/State/RailShooterEnemyStateDive.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include <algorithm>
#include <cmath>

void RailShooterEnemyStateDive::Enter(RailShooterEnemyComponent* /*enemy*/) {
    // 突入開始ログや効果音等のトリガー（必要に応じて拡張）
}

void RailShooterEnemyStateDive::Update(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    // 特攻機（DiveBomber）：自機前方から急加速して体当たり自爆コースへ突撃
    float currentDist = enemy->GetCurrentDistanceOffset();
    currentDist -= enemy->GetSpeed() * 1.8f * dt;
    enemy->SetCurrentDistanceOffset(currentDist);

    // 突撃しながら自機の正面ラインへ向かって急激に収束
    float tLerp = std::clamp(dt * 2.5f, 0.0f, 1.0f);
    auto localOffset = enemy->GetCurrentLocalOffset();
    localOffset.x = std::lerp(localOffset.x, 0.0f, tLerp);
    localOffset.y = std::lerp(localOffset.y, 0.0f, tLerp);
    enemy->SetCurrentLocalOffset(localOffset);

    // 鋭いコルクスクリュー回転（ロール角加算）
    float rollAngle = enemy->GetDiveRollAngle();
    rollAngle += dt * 14.0f;
    enemy->SetDiveRollAngle(rollAngle);

    // 自機後方に完全に抜けたら消滅
    if (currentDist < -30.0f) {
        enemy->NotifyDespawn(DespawnReason::OutOfBounds);
    }
}

void RailShooterEnemyStateDive::Exit(RailShooterEnemyComponent* /*enemy*/) {}
