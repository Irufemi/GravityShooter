#pragma once

class TitleSceneDirectorComponent;

/**
 * @enum TitleLaunchStateType
 * @brief タイトル出撃シーケンスの動作状態を表すステート列挙体
 */
enum class TitleLaunchStateType {
    Idle,       //!< 待機中（自機ホバリング・ガレキ公転・カメラ呼吸）
    Charge,     //!< [Phase 1: 蓄勢] 0.00s〜0.50s (タメ・重力収束・整流)
    Accelerate, //!< [Phase 2: 咆哮] 0.50s〜1.60s (アフターバーナー点火・急加速・動的FOV・微細振動)
    Break,      //!< [Phase 3: 突破] 1.60s〜2.20s (超光速離脱・光の点に消滅・FOV復帰)
    Afterglow //!< [Phase 4: 余韻] 2.20s〜3.20s (自機消失後の静寂・残光・風の抜け・暗転発火)
};

/**
 * @class ITitleLaunchState
 * @brief タイトル出撃シーケンス制御のステート基底インターフェース
 * @details
 * State Pattern に基づき、蓄勢（Charge）、急加速（Accelerate）、突破（Break）、余韻（Afterglow）の
 * 各演出フェーズのライフサイクルおよびパラメータ更新をカプセル化・ポリモーフィズム化します。
 */
class ITitleLaunchState {
public:
    virtual ~ITitleLaunchState() = default;

    /**
     * @brief ステート開始時処理
     * @param[in] director 対象のディレクターコンポーネント
     */
    virtual void Enter(TitleSceneDirectorComponent* director) = 0;

    /**
     * @brief 毎フレーム更新処理
     * @param[in] director 対象のディレクターコンポーネント
     * @param[in] dt デルタタイム（秒）
     */
    virtual void Update(TitleSceneDirectorComponent* director, float dt) = 0;

    /**
     * @brief ステート終了時処理
     * @param[in] director 対象のディレクターコンポーネント
     */
    virtual void Exit(TitleSceneDirectorComponent* director) {
        (void)director;
    }

    /**
     * @brief 現在のステートタイプを取得する
     * @return 状態タイプ
     */
    virtual TitleLaunchStateType GetStateType() const = 0;
};
