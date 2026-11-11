#include "Combat/ScoreManagerComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include <algorithm>

ScoreManagerComponent::ScoreManagerComponent() {
    instance_ = this;
}

ScoreManagerComponent::~ScoreManagerComponent() {
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void ScoreManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Current Score", &currentScore_);
    RegisterProperty("Total Kills", &stats_.enemiesKilled);
    RegisterProperty("Max Multi Kill", &stats_.maxMultiKill);
}

void ScoreManagerComponent::Initialize() {
    currentScore_ = 0;
    multiKillWindowTimer_ = 0.0f;
    pendingBatchKills_ = 0;
    currentPopup_ = CombatPopupData{};
    centerBanner_ = CenterBannerData{};
    stats_ = GameplaySessionStats{};
}

void ScoreManagerComponent::Start() {
    instance_ = this;
}

ScoreManagerComponent* ScoreManagerComponent::GetInstance() {
    return instance_;
}

ScoreManagerComponent* ScoreManagerComponent::GetOrCreate(BaseScene* scene) {
    if (instance_) {
        return instance_;
    }
    if (!scene) {
        return nullptr;
    }

    if (auto obj = scene->FindGameObject("ScoreManager")) {
        if (auto comp = obj->GetComponent<ScoreManagerComponent>()) {
            return comp;
        }
    }

    auto scoreObj = std::make_shared<GameObject>("ScoreManager");
    scoreObj->SetIsSerializable(false);
    auto comp = scoreObj->AddComponent<ScoreManagerComponent>();
    scene->AddGameObject(scoreObj);
    comp->Initialize();
    return comp.get();
}

int ScoreManagerComponent::CalculateMultiKillScore(int count) const {
    switch (count) {
    case 1:
        return 100;
    case 2:
        return 300;
    case 3:
        return 700;
    case 4:
        return 1500;
    default:
        // 5機以上 (MAX)
        return 3500;
    }
}

void ScoreManagerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    stats_.clearTimeSeconds += dt;

    // ポップアップ残時間の減衰
    if (currentPopup_.remainingDisplayTime > 0.0f) {
        currentPopup_.remainingDisplayTime -= dt;
        if (currentPopup_.remainingDisplayTime <= 0.0f) {
            currentPopup_.remainingDisplayTime = 0.0f;
            currentPopup_.type = CombatPopupType::None;
        }
    }

    // 画面中央バナー残時間の減衰
    if (centerBanner_.remainingDisplayTime > 0.0f) {
        centerBanner_.remainingDisplayTime -= dt;
        if (centerBanner_.remainingDisplayTime <= 0.0f) {
            centerBanner_.remainingDisplayTime = 0.0f;
        }
    }

    // マルチキル集計ウィンドウの更新
    if (multiKillWindowTimer_ > 0.0f) {
        multiKillWindowTimer_ -= dt;
        if (multiKillWindowTimer_ <= 0.0f) {
            FinalizeMultiKillBatch();
        }
    }
}

void ScoreManagerComponent::NotifyEnemyKilled(int baseScore, bool isMultiLock, int batchCount) {
    (void)baseScore;
    (void)isMultiLock;
    (void)batchCount;
    stats_.enemiesKilled++;
    pendingBatchKills_++;

    // 0.5秒の集計ウィンドウをリセット（連鎖中キープ）
    multiKillWindowTimer_ = kMultiKillWindowDuration_;

    // GDD仕様上の最大所持数5機に達した場合は即座にバッチ確定！
    if (pendingBatchKills_ >= 5) {
        FinalizeMultiKillBatch();
    } else {
        // 撃破の瞬間にリアルタイムで暫定ポップを表示（即時フィードバック）
        int currentGained = CalculateMultiKillScore(pendingBatchKills_);
        currentPopup_.type = CombatPopupType::Kill;
        currentPopup_.killCount = pendingBatchKills_;
        currentPopup_.scoreGained = currentGained;
        currentPopup_.remainingDisplayTime = 0.85f;
        currentPopup_.isNew = true;
    }
}

void ScoreManagerComponent::FinalizeMultiKillBatch() {
    if (pendingBatchKills_ <= 0) {
        multiKillWindowTimer_ = 0.0f;
        return;
    }

    int finalCount = pendingBatchKills_;
    pendingBatchKills_ = 0;
    multiKillWindowTimer_ = 0.0f;

    int scoreGained = CalculateMultiKillScore(finalCount);
    currentScore_ += scoreGained;
    stats_.totalScore = currentScore_;

    if (finalCount > stats_.maxMultiKill) {
        stats_.maxMultiKill = finalCount;
    }
    if (finalCount >= 2) {
        stats_.multiKillCount++;
    }

    // 確定ポップアップ（0.85秒表示）
    currentPopup_.type = CombatPopupType::Kill;
    currentPopup_.killCount = finalCount;
    currentPopup_.scoreGained = scoreGained;
    currentPopup_.remainingDisplayTime = 0.85f;
    currentPopup_.isNew = true;
}

void ScoreManagerComponent::NotifyBulletBlocked() {
    stats_.bulletsBlocked++;
    currentScore_ += 50; // ガレキシールド防御ボーナス
    stats_.totalScore = currentScore_;

    // キル演出中（残り時間が十分ある場合）でなければ GUARD! ポップを発生
    if (currentPopup_.type != CombatPopupType::Kill || currentPopup_.remainingDisplayTime <= 0.35f) {
        currentPopup_.type = CombatPopupType::Guard;
        currentPopup_.killCount = 0;
        currentPopup_.scoreGained = 50;
        currentPopup_.remainingDisplayTime = 0.65f;
        currentPopup_.isNew = true;
    }
}

void ScoreManagerComponent::NotifyBossCoreDamaged(float damage) {
    if (damage <= 0.0f) {
        return;
    }
    int gained = static_cast<int>(damage * 10.0f);
    currentScore_ += gained;
    stats_.totalScore = currentScore_;
}

void ScoreManagerComponent::NotifyPlayerDamaged() {
    stats_.damageTakenCount++;
    currentScore_ = (std::max)(0, currentScore_ - 500); // 被弾ペナルティ
    stats_.totalScore = currentScore_;

    // 被弾時は即座に DAMAGE! ポップを表示
    currentPopup_.type = CombatPopupType::DamagePenalty;
    currentPopup_.killCount = 0;
    currentPopup_.scoreGained = -500;
    currentPopup_.remainingDisplayTime = 0.85f;
    currentPopup_.isNew = true;
}

void ScoreManagerComponent::TriggerPopup(CombatPopupType type, int scoreDelta, float duration) {
    // 撃破演出中（残り時間0.3秒超）の場合は撃破演出を優先保護
    if (currentPopup_.type == CombatPopupType::Kill && currentPopup_.remainingDisplayTime > 0.30f) {
        return;
    }

    currentPopup_.type = type;
    currentPopup_.killCount = 0;
    currentPopup_.scoreGained = scoreDelta;
    currentPopup_.remainingDisplayTime = duration;
    currentPopup_.isNew = true;
}

void ScoreManagerComponent::TriggerCenterBanner(const std::wstring& title, const std::wstring& subTitle,
                                                const Irufemi::Vector4& color, float duration) {
    centerBanner_.title = title;
    centerBanner_.subTitle = subTitle;
    centerBanner_.remainingDisplayTime = duration;
    centerBanner_.maxDisplayTime = duration;
    centerBanner_.color = color;
    centerBanner_.isNew = true;
}
