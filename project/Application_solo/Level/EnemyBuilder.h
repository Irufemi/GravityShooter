#pragma once
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector2.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include <string>
#include <optional>

class EnemySpawnerComponent;
class SplineComponent;
class SplineFollowerComponent;
class GameObject;

/**
 * @class EnemyBuilder
 * @brief 敵キャラクターの生成とパラメータ設定を安全かつ柔軟に構築する Builder パターンクラス
 */
class EnemyBuilder {
public:
    /**
     * @brief コンストラクタ
     * @param[in] spawner 使用するエネミースポーナー
     */
    explicit EnemyBuilder(EnemySpawnerComponent* spawner);

    /**
     * @brief プレハブパスを設定
     */
    EnemyBuilder& WithPrefab(const std::string& prefabPath);

    /**
     * @brief 交戦滞空時間を設定
     */
    EnemyBuilder& WithCombatDuration(float duration);

    /**
     * @brief 自機との維持目標距離を設定
     */
    EnemyBuilder& WithTargetDistance(float distance);

    /**
     * @brief 射撃インターバルを設定
     */
    EnemyBuilder& WithShootInterval(float interval);

    /**
     * @brief 敵弾の飛翔速度を設定
     */
    EnemyBuilder& WithBulletSpeed(float speed);

    /**
     * @brief 敵の移動速度を設定
     */
    EnemyBuilder& WithSpeed(float speed);

    /**
     * @brief 戦術行動タイプを設定（Strategy Pattern と連動）
     */
    EnemyBuilder& WithBehaviorType(EnemyBehaviorType type);

    /**
     * @brief レール追従パラメータを一括設定
     */
    EnemyBuilder& WithRailTrackingParams(SplineComponent* spline, SplineFollowerComponent* playerFollower,
                                         float initialDistOffset, float targetDistance,
                                         const Irufemi::Vector2& formationOffset);

    /**
     * @brief 指定した座標・回転・スケールで敵オブジェクトを生成・パラメータ構築する
     * @param[in] position ワールド座標
     * @param[in] rotation 回転角度
     * @param[in] scaleMultiplier スケール倍率
     * @return 構築された敵の GameObject ポインタ
     */
    GameObject* Build(const Irufemi::Vector3& position, const Irufemi::Vector3& rotation, float scaleMultiplier = 1.0f);

private:
    EnemySpawnerComponent* spawner_ = nullptr;
    std::string prefabPath_;
    float combatDuration_ = 7.5f;
    float targetDistance_ = 65.0f;
    float shootInterval_ = 1.8f;
    float bulletSpeed_ = 32.0f;
    float speed_ = 15.0f;
    std::optional<EnemyBehaviorType> behaviorType_;

    // レール追従パラメータ
    SplineComponent* spline_ = nullptr;
    SplineFollowerComponent* playerFollower_ = nullptr;
    float initialDistOffset_ = 0.0f;
    float trackingTargetDistance_ = 65.0f;
    Irufemi::Vector2 formationOffset_{0.0f, 0.0f};
    bool hasTrackingParams_ = false;
};
