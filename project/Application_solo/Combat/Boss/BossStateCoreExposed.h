#pragma once
#include "Combat/Boss/IBossState.h"
#include "Core/Math/Vector4.h"

class EffectMaskComponent;

/**
 * @class BossStateCoreExposed
 * @brief ボスのコア露出（弱点露出・ダメージ受付）ステート
 * @details 全てのシールドが破壊された後に遷移し、プレイヤーからのダメージ受付と被弾時カメラシェイク演出を制御します。
 */
class BossStateCoreExposed : public IBossState {
public:
    /**
     * @brief ステート開始処理。コア露出ログを出力し、コアのアウトラインを高輝度ゴールド・太線に切り替えます。
     * @param boss 対象のボスコンポーネント
     */
    void Enter(BossComponent* boss) override;

    /**
     * @brief 毎フレーム更新処理。被弾ヒットフラッシュの減衰を処理します。
     * @param boss 対象のボスコンポーネント
     */
    void Update(BossComponent* boss) override;

    /**
     * @brief ステート終了処理。コアのアウトラインを通常の深紅・標準太さに復元します。
     * @param boss 対象のボスコンポーネント
     */
    void Exit(BossComponent* boss) override;

    /**
     * @brief 被ダメージ処理。HPを減算しカメラシェイクを再生、HPが0以下で撃破ステートへ遷移します。
     * @param boss 対象のボスコンポーネント
     * @param damage ダメージ量
     */
    void OnTakeDamage(BossComponent* boss, float damage) override;

    /**
     * @brief コア露出中フラグを返す
     * @return 常に true
     */
    bool IsCoreExposed() const override {
        return true;
    }

private:
    float hitFlashTimer_ = 0.0f; //!< コア被弾時の白熱閃光タイマー（秒）
    Irufemi::Vector4 originalCoreColor_ = {1.0f, 0.05f, 0.15f, 1.0f}; //!< 通常時の深紅アウトライン色
    float originalThickness_ = 2.0f;                                  //!< 通常時のアウトライン線幅
    bool hasCachedOriginal_ = false;                                  //!< 通常設定のキャッシュ完了フラグ

    /**
     * @brief ボスコアのEffectMaskComponentを取得する
     */
    EffectMaskComponent* GetCoreEffectMask(BossComponent* boss);
};
