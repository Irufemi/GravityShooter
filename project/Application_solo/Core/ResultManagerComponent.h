#pragma once
#include "Core/GameApplication.h"
#include "Framework/Component/Component.h"
#include "Combat/ScoreManagerComponent.h"
#include <string>
#include <memory>
#include <vector>

class ResultManagerComponent : public Component {
public:
    ResultManagerComponent() = default;
    ~ResultManagerComponent() override;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "ResultManagerComponent";
    }

private:
    float timer_ = 0.0f;
    float resultDelayTime_ = 2.0f;
    float timeScaleResetDelayTime_ = 1.5f;   // スローモーションを解除するまでの時間
    float timeScaleRecoveryDuration_ = 0.5f; // スローモーションから等倍速に戻るまでの時間(イージング時間)
    bool canReturnToTitle_ = false;
    bool isExiting_ = false; // 多重入力・多重遷移ガード
    bool hasResetTimeScale_ = false;
    bool hasPlayedSnapSE_ = false;

    // イージング用状態変数
    bool isRecoveringTimeScale_ = false;
    float timeScaleRecoveryTimer_ = 0.0f;
    float startTimeScale_ = 0.0f;

    // プロパティ化してエディタで設定可能にする
    std::string nextSceneName_ = std::string(GameScenes::Title);
    float baseTitleScale_ = 48.0f;
    std::string fontId_ = "toro_glitch";

    void ApplyEntryPostProcess();
    void UpdateEntryPostProcess(float progress);
    void RemoveEntryPostProcess();

    bool isPostProcessActive_ = false;
    bool hasWhiteFlash_ = false;

    std::weak_ptr<GameObject> darkBackdropObj_;
    std::weak_ptr<GameObject> pressSpaceObj_;

    // --- リザルト集計・統計＆ランク演出用 ---
    enum class ResultPhase {
        TitleSnap,   //!< 0.0s〜0.45s: タイトル文字スナップ出現
        RollStats,   //!< 0.45s〜1.45s: 統計行が順次カウントアップ開示 (se_data_sync)
        PreStamp,    //!< 1.45s〜1.60s: 一瞬の静寂・タメ
        StampRank,   //!< 1.60s〜2.00s: ランク巨大スタンプ着弾 (se_rank_stamp)
        Ready        //!< 2.00s〜: リターン待機 (SPACE/Aでタイトルへ)
    };
    ResultPhase phase_ = ResultPhase::TitleSnap;
    float phaseTimer_ = 0.0f;

    GameplaySessionStats stats_{};
    std::wstring rankText_ = L"S";
    Irufemi::Vector4 rankColor_ = {1.0f, 0.85f, 0.2f, 1.0f};

    bool hasPlayedDataSyncSE_ = false;
    bool hasPlayedRankSE_ = false;

    // 統計行UIのGameObject保持
    struct StatRowUI {
        std::weak_ptr<GameObject> labelObj;
        std::weak_ptr<GameObject> valueObj;
        std::wstring targetValueStr;
    };
    std::vector<StatRowUI> statRows_;
    std::weak_ptr<GameObject> rankLabelObj_;
    std::weak_ptr<GameObject> rankValueObj_;

    void SetupStatsUI();
    void UpdateStatsRoll(float dt);
    void TriggerRankStamp();
    void SkipToReady();
};
