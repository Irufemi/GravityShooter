#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <string>

/// @brief セッション全体のゲームプレイ統計（ResultSceneへの引き渡しデータ）
struct GameplaySessionStats {
    int totalScore = 0;
    int maxMultiKill = 0;       //!< 最大同時撃破数 (1〜5)
    int multiKillCount = 0;     //!< マルチキル(2機以上一斉)を達成した総回数
    int enemiesKilled = 0;      //!< 撃破総数
    int bulletsBlocked = 0;     //!< ガレキで防御した敵弾数
    int damageTakenCount = 0;   //!< 被弾回数
    float clearTimeSeconds = 0.0f;
};

/// @brief 撃破時のポップアップ通知データ
struct MultiKillPopupData {
    int killCount = 0;          //!< 1〜5
    int scoreGained = 0;        //!< 今回の獲得スコア
    float remainingDisplayTime = 0.0f; //!< ポップ表示残時間（秒）
    bool isNew = false;
};

/**
 * @class ScoreManagerComponent
 * @brief マルチロック一斉撃破ボーナス（Rez思想）とスコア統計を一元管理するモデルコンポーネント
 */
class ScoreManagerComponent : public Component {
public:
    ScoreManagerComponent();
    ~ScoreManagerComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;

    // --- イベントリスナー通知関数（戦闘アクターから呼ばれる疎結合API） ---
    void NotifyEnemyKilled(int baseScore = 100, bool isMultiLock = false, int batchCount = 1);
    void NotifyBulletBlocked();
    void NotifyBossCoreDamaged(float damage);
    void NotifyPlayerDamaged();

    // --- ゲッター（HUD表示およびリザルト集計用） ---
    int GetCurrentScore() const { return currentScore_; }
    int GetTotalKills() const { return stats_.enemiesKilled; }
    int GetMaxMultiKill() const { return stats_.maxMultiKill; }
    const GameplaySessionStats& GetStats() const { return stats_; }

    /// @brief 直近の撃破ポップアップデータを取得
    const MultiKillPopupData& GetPopupData() const { return currentPopup_; }

    static ScoreManagerComponent* GetInstance();
    static ScoreManagerComponent* GetOrCreate(class BaseScene* scene);

private:
    void FinalizeMultiKillBatch();
    int CalculateMultiKillScore(int count) const;

    inline static ScoreManagerComponent* instance_ = nullptr;

    int currentScore_ = 0;

    // 時間差投擲（0.15秒間隔）を吸収する0.5秒のマルチキル集計ウィンドウ
    float multiKillWindowTimer_ = 0.0f;
    int pendingBatchKills_ = 0;
    const float kMultiKillWindowDuration_ = 0.50f;

    MultiKillPopupData currentPopup_{};
    GameplaySessionStats stats_{};
};
