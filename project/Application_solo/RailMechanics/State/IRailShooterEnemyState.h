#pragma once

class RailShooterEnemyComponent;

/**
 * @enum EnemyAIState
 * @brief 敵キャラクターのAI行動状態タイプ
 */
enum class EnemyAIState {
    Approach,  //!< 前方定位置への進入
    Combat,    //!< 自機と一定距離を保って滞空・射撃
    Dive,      //!< 特攻急降下（DiveBomber専用: 自機へ向けて急加速突進）
    Disengage  //!< 制限時間終了によるすれ違い離脱
};

/**
 * @class IRailShooterEnemyState
 * @brief レールシューティング敵AIステートの基底インターフェース
 * @details
 * State Pattern に基づき、敵キャラクターの各行動フェーズ（進入、交戦、急降下、離脱）の
 * ライフサイクルおよび更新処理をカプセル化・ポリモーフィズム化します。
 */
class IRailShooterEnemyState {
public:
    virtual ~IRailShooterEnemyState() = default;

    /**
     * @brief ステート開始時処理
     * @param[in] enemy 対象の敵コンポーネント
     */
    virtual void Enter(RailShooterEnemyComponent* enemy) = 0;

    /**
     * @brief 毎フレーム更新処理
     * @param[in] enemy 対象の敵コンポーネント
     * @param[in] dt デルタタイム（秒）
     */
    virtual void Update(RailShooterEnemyComponent* enemy, float dt) = 0;

    /**
     * @brief ステート終了時処理
     * @param[in] enemy 対象の敵コンポーネント
     */
    virtual void Exit(RailShooterEnemyComponent* enemy) = 0;

    /**
     * @brief 現在のステートタイプを取得する
     * @return 状態タイプ
     */
    virtual EnemyAIState GetStateType() const = 0;
};
