#include "RailMechanics/Strategy/EnemyAttackStrategyPredictiveSniper.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include <algorithm>
#include <cmath>

void EnemyAttackStrategyPredictiveSniper::OnEnterCombat(RailShooterEnemyComponent* enemy) {
    if (enemy) {
        shootTimer_ = (std::max)(enemy->GetSniperTelegraphDuration(), 1.2f);
        enemy->SetShootTimer(shootTimer_);
    }
}

void EnemyAttackStrategyPredictiveSniper::UpdateCombat(RailShooterEnemyComponent* enemy, float dt) {
    if (!enemy) {
        return;
    }

    shootTimer_ -= dt;
    enemy->SetShootTimer(shootTimer_);

    GameObject* playerObj = enemy->GetPlayerObject();
    Irufemi::Vector3 currentPlayerPos = {0.0f, 0.0f, 0.0f};
    if (playerObj && playerObj->GetTransform()) {
        currentPlayerPos = playerObj->GetTransform()->GetWorldPosition();
    }

    float telegraphDuration = enemy->GetSniperTelegraphDuration();
    float lockLeadTime = enemy->GetSniperLockLeadTime();

    // スナイパー：予兆期間中の射線更新およびロック判定
    if (shootTimer_ <= telegraphDuration && shootTimer_ > 0.0f) {
        if (shootTimer_ > lockLeadTime) {
            // [追従フェーズ] プレイヤーの未来位置をリアルタイム計算して射線を追従
            enemy->SetAimLocked(false);
            auto transform = enemy->GetTransform();
            if (transform) {
                Irufemi::Vector3 myPos = transform->GetWorldPosition();
                float dx = currentPlayerPos.x - myPos.x;
                float dy = currentPlayerPos.y - myPos.y;
                float dz = currentPlayerPos.z - myPos.z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                float travelTime = dist / (std::max)(enemy->GetBulletSpeed(), 1.0f);
                travelTime = std::clamp(travelTime, 0.0f, 1.2f); // 過剰な未来予測の暴走を防止

                const auto& playerVel = enemy->GetPlayerVelocity();
                Irufemi::Vector3 lockedTarget = {currentPlayerPos.x + playerVel.x * travelTime,
                                                 currentPlayerPos.y + playerVel.y * travelTime,
                                                 currentPlayerPos.z + playerVel.z * travelTime};
                enemy->SetLockedTargetPos(lockedTarget);

                Irufemi::Vector3 aimDiff = {lockedTarget.x - myPos.x, lockedTarget.y - myPos.y,
                                            lockedTarget.z - myPos.z};
                float aimLen = std::sqrt(aimDiff.x * aimDiff.x + aimDiff.y * aimDiff.y + aimDiff.z * aimDiff.z);
                if (aimLen > 0.0001f) {
                    enemy->SetLockedAimDir({aimDiff.x / aimLen, aimDiff.y / aimLen, aimDiff.z / aimLen});
                } else {
                    enemy->SetLockedAimDir({0.0f, 0.0f, -1.0f});
                }
            }
        } else {
            // [ロック固定フェーズ] 射線固定（追従停止。自機が動いても空間に射線が固定される）
            enemy->SetAimLocked(true);
        }
    }

    if (shootTimer_ <= 0.0f) {
        // 固定された射線ベクトル（または未来予測）へ高威力の偏差弾を発射
        enemy->ShootPredictiveAtPlayer(currentPlayerPos, enemy->GetPlayerVelocity());
        shootTimer_ = enemy->GetShootInterval();
        enemy->ResetTelegraph();
    }
}

void EnemyAttackStrategyPredictiveSniper::OnExitCombat(RailShooterEnemyComponent* enemy) {
    if (enemy) {
        enemy->ResetTelegraph();
    }
}
