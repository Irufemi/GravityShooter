#pragma once

class RailShooterEnemyComponent;

/**
 * @class IEnemyAttackStrategy
 * @brief 敵AIの攻撃・照準・予兆アルゴリズムをカプセル化する Strategy パターン基底インタフェース
 */
class IEnemyAttackStrategy {
public:
    virtual ~IEnemyAttackStrategy() = default;

    /**
     * @brief 交戦ステート進入時の初期化
     * @param[in] enemy 制御対象の敵コンポーネント
     */
    virtual void OnEnterCombat(RailShooterEnemyComponent* enemy) = 0;

    /**
     * @brief 毎フレームの照準・射撃・予兆線更新
     * @param[in] enemy 制御対象の敵コンポーネント
     * @param[in] dt デルタタイム
     */
    virtual void UpdateCombat(RailShooterEnemyComponent* enemy, float dt) = 0;

    /**
     * @brief 交戦ステート離脱・中断時のクリーンアップ
     * @param[in] enemy 制御対象の敵コンポーネント
     */
    virtual void OnExitCombat(RailShooterEnemyComponent* enemy) = 0;
};
