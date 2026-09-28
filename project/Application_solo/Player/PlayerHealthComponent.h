#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include <functional>
#include <string>
#include <vector>

/**
 * @class PlayerHealthComponent
 * @brief プレイヤーの体力、被弾ダメージ、無敵時間、死亡状態を管理するコンポーネント
 */
class PlayerHealthComponent : public Component {
public:
    PlayerHealthComponent() = default;
    ~PlayerHealthComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "PlayerHealthComponent";
    }

    /**
     * @brief プレイヤー死亡時のプライマリコールバック（ゲーム進行制御用）を設定する
     * @details GameLoopManagerなどの単一オーナーがシーン遷移やリスタート制御を行うために使用します。
     * @param[in] callback 呼び出される関数オブジェクト
     */
    void SetOnPlayerDied(std::function<void()> callback) {
        onPlayerDied_ = std::move(callback);
    }

    /**
     * @brief 死亡演出シーケンス完了時のコールバックを設定する
     * @param[in] callback 呼び出される関数オブジェクト
     */
    void SetOnDeathSequenceFinished(std::function<void()> callback) {
        onDeathSequenceFinished_ = std::move(callback);
    }

    /**
     * @brief プレイヤー死亡を通知する（プライマリハンドラおよび全登録リスナーを実行）
     */
    void NotifyPlayerDied();

    /**
     * @brief 死亡シーケンス完了を通知する
     */
    void NotifyDeathSequenceFinished();

    /**
     * @brief 被弾時のイベントリスナーを追加する
     */
    void AddOnDamageTakenListener(std::function<void(int damage)> callback) {
        onDamageTakenListeners_.push_back(std::move(callback));
    }

    /**
     * @brief 死亡時のオブザーバーリスナー（演出・UI・サウンド等）を追加する
     * @details 演出コンポーネント（PlayerDamageVisualizer等）が死亡時のエフェクトトリガーを購読するために使用します。
     * @param[in] callback 呼び出される関数オブジェクト
     */
    void AddOnPlayerDiedListener(std::function<void()> callback) {
        onPlayerDiedListeners_.push_back(std::move(callback));
    }

    void LoadStatusFromJson();

    std::string GetStatusDataPath() const {
        return statusDataPath_;
    }
    void SetStatusDataPath(const std::string& path) {
        statusDataPath_ = path;
    }

    void TakeDamage(int damage);
    bool IsInvincible() const {
        return invincibilityTimer_ > 0.0f;
    }
    float GetInvincibilityTimer() const {
        return invincibilityTimer_;
    }
    float GetMaxInvincibilityTime() const {
        return maxInvincibilityTime_;
    }
    void SetGodMode(bool godMode) {
        isGodMode_ = godMode;
    }

    int GetHp() const {
        return hp_;
    }
    int GetMaxHp() const {
        return maxHp_;
    }
    float GetDeathSequenceDuration() const {
        return deathSequenceDuration_;
    }
    void SetDeathSequenceDuration(float duration) {
        deathSequenceDuration_ = duration;
    }
    bool IsDead() const {
        return isDead_;
    }
    bool IsGodMode() const {
        return isGodMode_;
    }

private:
    std::string statusDataPath_ = "resources/GameData/PlayerStatus.json";
    int hp_ = 100;
    int maxHp_ = 100;
    bool isDead_ = false;
    bool isGodMode_ = false;

    static constexpr float kDefaultDeathSequenceDuration = 2.0f;
    float deathSequenceDuration_ = kDefaultDeathSequenceDuration;
    float deathTimer_ = 0.0f;
    bool hasTriggeredDeathSequenceFinished_ = false;

    // 被弾・無敵時間
    float invincibilityTimer_ = 0.0f;
    float maxInvincibilityTime_ = 1.0f;

    std::vector<std::function<void(int damage)>> onDamageTakenListeners_;
    std::vector<std::function<void()>> onPlayerDiedListeners_;
    std::function<void()> onPlayerDied_;
    std::function<void()> onDeathSequenceFinished_;

    class ColliderComponent* collider_ = nullptr;

    // 無敵ブリンク演出 (Juice)
    Irufemi::Vector4 normalOutlineColor_ = {0.15f, 0.9f, 1.0f, 1.0f}; //!< 通常時の自機ネオンシアン色
    Irufemi::Vector4 warningOutlineColor_ = {1.0f, 0.1f, 0.1f, 1.0f}; //!< 被弾無敵時の警告レッド色
    bool hasCachedNormalOutline_ = false; //!< 通常アウトライン色のキャッシュ完了フラグ
};
