#include "Level/EnemyBuilder.h"
#include "Combat/EnemySpawnerComponent.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Framework/GameObject/GameObject.h"

EnemyBuilder::EnemyBuilder(EnemySpawnerComponent* spawner)
    : spawner_(spawner) {}

EnemyBuilder& EnemyBuilder::WithPrefab(const std::string& prefabPath) {
    prefabPath_ = prefabPath;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithCombatDuration(float duration) {
    combatDuration_ = duration;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithTargetDistance(float distance) {
    targetDistance_ = distance;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithShootInterval(float interval) {
    shootInterval_ = interval;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithBulletSpeed(float speed) {
    bulletSpeed_ = speed;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithSpeed(float speed) {
    speed_ = speed;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithBehaviorType(EnemyBehaviorType type) {
    behaviorType_ = type;
    return *this;
}

EnemyBuilder& EnemyBuilder::WithRailTrackingParams(SplineComponent* spline,
                                                   SplineFollowerComponent* playerFollower,
                                                   float initialDistOffset,
                                                   float targetDistance,
                                                   const Irufemi::Vector2& formationOffset) {
    spline_ = spline;
    playerFollower_ = playerFollower;
    initialDistOffset_ = initialDistOffset;
    trackingTargetDistance_ = targetDistance;
    formationOffset_ = formationOffset;
    hasTrackingParams_ = true;
    return *this;
}

GameObject* EnemyBuilder::Build(const Irufemi::Vector3& position,
                                const Irufemi::Vector3& rotation,
                                float scaleMultiplier) {
    if (!spawner_) {
        return nullptr;
    }

    GameObject* enemyObj = nullptr;
    if (!prefabPath_.empty()) {
        enemyObj = spawner_->SpawnEnemyByPrefab(prefabPath_, position, rotation, scaleMultiplier);
    } else {
        enemyObj = spawner_->SpawnEnemy(position, rotation, scaleMultiplier);
    }

    if (!enemyObj) {
        return nullptr;
    }

    if (auto enemyComp = enemyObj->GetComponent<RailShooterEnemyComponent>()) {
        enemyComp->SetCombatDuration(combatDuration_);
        enemyComp->SetTargetDistance(targetDistance_);
        enemyComp->SetShootInterval(shootInterval_);
        enemyComp->SetBulletSpeed(bulletSpeed_);
        enemyComp->SetSpeed(speed_);

        if (behaviorType_.has_value()) {
            enemyComp->SetBehaviorType(behaviorType_.value());
        }

        if (hasTrackingParams_) {
            enemyComp->SetRailTrackingParams(spline_, playerFollower_, initialDistOffset_,
                                             trackingTargetDistance_, formationOffset_);
        }
    }

    return enemyObj;
}
