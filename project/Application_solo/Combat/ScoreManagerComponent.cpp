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
    currentPopup_ = MultiKillPopupData{};
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
    currentPopup_.killCount = finalCount;
    currentPopup_.scoreGained = scoreGained;
    currentPopup_.remainingDisplayTime = 0.85f;
    currentPopup_.isNew = true;
}

void ScoreManagerComponent::NotifyBulletBlocked() {
    stats_.bulletsBlocked++;
    currentScore_ += 50; // ガレキシールド防御ボーナス
    stats_.totalScore = currentScore_;
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
}
