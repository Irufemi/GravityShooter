#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <string>

/// @brief セッション全体のゲームプレイ統計（ResultSceneへの引き渡しデータ）
struct GameplaySessionStats {
    int totalScore = 0;
    int maxMultiKill = 0;     //!< 最大同時撃破数 (1〜5)
    int multiKillCount = 0;   //!< マルチキル(2機以上一斉)を達成した総回数
    int enemiesKilled = 0;    //!< 撃破総数
    int bulletsBlocked = 0;   //!< ガレキで防御した敵弾数
    int damageTakenCount = 0; //!< 被弾回数
    float clearTimeSeconds = 0.0f;
};

/// @brief 戦闘ポップアップ通知の種類
enum class CombatPopupType {
    None,
    Kill,          //!< 撃破 (HIT! / MULTI KILL)
    Guard,         //!< ガレキ防御 (GUARD! +50 PTS)
    DamagePenalty, //!< 被弾減点 (DAMAGE! -500 PTS)
    NoDebris,      //!< 弾切れ空撃ち警告 (NO DEBRIS - PULL!)
    MaxStock       //!< ガレキ満タン (MAX STOCK!)
};

/// @brief 戦闘ポップアップ通知データ
struct CombatPopupData {
    CombatPopupType type = CombatPopupType::None;
    int killCount = 0;                 //!< 1〜5 (Kill用)
    int scoreGained = 0;               //!< 今回の獲得スコア / 変動量
    float remainingDisplayTime = 0.0f; //!< ポップ表示残時間（秒）
    bool isNew = false;
};

/// @brief 画面中央の特大バナー通知データ（ボスシールド破壊等）
struct CenterBannerData {
    std::wstring title;
    std::wstring subTitle;
    float remainingDisplayTime = 0.0f;
    float maxDisplayTime = 1.2f;
    Irufemi::Vector4 color = {1.0f, 0.85f, 0.2f, 1.0f};
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
    int GetCurrentScore() const {
        return currentScore_;
    }
    int GetTotalKills() const {
        return stats_.enemiesKilled;
    }
    int GetMaxMultiKill() const {
        return stats_.maxMultiKill;
    }
    const GameplaySessionStats& GetStats() const {
        return stats_;
    }

    /// @brief 直近の戦闘ポップアップデータを取得
    const CombatPopupData& GetPopupData() const {
        return currentPopup_;
    }

    /// @brief 画面中央バナー通知データを取得
    const CenterBannerData& GetCenterBanner() const {
        return centerBanner_;
    }

    /// @brief 汎用戦闘ポップアップを発生させる
    void TriggerPopup(CombatPopupType type, int scoreDelta = 0, float duration = 0.65f);

    /// @brief 画面中央の特大バナー通知を発生させる
    void TriggerCenterBanner(const std::wstring& title, const std::wstring& subTitle = L"",
                             const Irufemi::Vector4& color = {1.0f, 0.85f, 0.2f, 1.0f}, float duration = 1.2f);

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

    CombatPopupData currentPopup_{};
    CenterBannerData centerBanner_{};
    GameplaySessionStats stats_{};
};
