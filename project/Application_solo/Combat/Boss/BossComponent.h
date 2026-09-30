#pragma once
#include "Framework/Component/Component.h"
#include "Combat/IDamageable.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Matrix4x4.h"
#include "Combat/Boss/IBossState.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>

class DebrisManagerComponent;
class GameObject;
class EnemyBeamComponent;
class DroneManagerComponent;
class BossBulletManagerComponent;

class BossComponent : public Component, public IDamageable {
public:
    BossComponent();
    ~BossComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;

    /**
     * @brief ボス死亡時のコールバックを設定する
     * @param[in] callback 呼び出される関数オブジェクト
     */
    void SetOnBossDied(std::function<void()> callback) {
        onBossDied_ = std::move(callback);
    }

    /**
     * @brief 撃破演出シーケンス完了時のコールバックを設定する
     * @param[in] callback 呼び出される関数オブジェクト
     */
    void SetOnDeathSequenceFinished(std::function<void()> callback) {
        onDeathSequenceFinished_ = std::move(callback);
    }

    /**
     * @brief 被弾時のイベントリスナーを追加する
     */
    void AddOnDamageTakenListener(std::function<void(float damage)> callback) {
        onDamageTakenListeners_.push_back(std::move(callback));
    }

    /**
     * @brief 撃破時のイベントリスナーを追加する
     */
    void AddOnBossDiedListener(std::function<void()> callback) {
        onBossDiedListeners_.push_back(std::move(callback));
    }

    /**
     * @brief 被弾を通知する
     */
    void NotifyDamageTaken(float damage);

    /**
     * @brief 撃破を通知する
     */
    void NotifyBossDied();

    /**
     * @brief 撃破演出シーケンス完了を通知する
     */
    void NotifyDeathSequenceFinished();

    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "BossComponent";
    }

    void LoadStatusFromJson();

    std::string GetStatusDataPath() const {
        return statusDataPath_;
    }
    void SetStatusDataPath(const std::string& path) {
        statusDataPath_ = path;
    }

    /**
     * @brief シールド（ガレキ）を1つ剥がし、本物のガレキとしてスポーンさせる
     */
    std::shared_ptr<GameObject> ExtractDebris();

    /**
     * @brief 特定のシールドをリストから除外する（破壊時など）
     */
    void RemoveShield(std::shared_ptr<GameObject> shield);

    /**
     * @brief ボスにダメージを与える
     */
    void TakeDamage(float damage) override;

    DamageableType GetDamageableType() const override {
        return DamageableType::Boss;
    }

    /**
     * @brief ステートの切り替え
     */
    void ChangeState(std::unique_ptr<IBossState> newState);

    float GetHp() const {
        return hp_;
    }
    float GetMaxHp() const {
        return maxHp_;
    }

    /**
     * @brief コアが露出している（ターゲット可能）状態かどうかを返す
     */
    bool IsCoreExposed() const {
        return currentState_ ? currentState_->IsCoreExposed() : false;
    }

    /**
     * @brief ボス関連の動的オブジェクト（ドローン、弾など）をまとめる汎用コンテナを取得する
     */
    std::shared_ptr<GameObject> GetBossContainer() const {
        return bossContainer_.lock();
    }

private:
    friend class BossStateIdle;
    friend class BossStateCoreExposed;
    friend class BossStateDestroyed;

    std::string statusDataPath_ = "resources/GameData/BossStatus.json";
    float maxHp_ = 1000.0f;
    float hp_ = 0.0f;

    int maxShieldCount_ = 100;
    float shieldRadius_ = 8.0f;

    std::unique_ptr<IBossState> currentState_;

    std::weak_ptr<GameObject> bossContainer_;
    std::vector<std::shared_ptr<GameObject>> shields_;
    std::vector<std::function<void(float damage)>> onDamageTakenListeners_;
    std::vector<std::function<void()>> onBossDiedListeners_;
    std::function<void()> onBossDied_;
    std::function<void()> onDeathSequenceFinished_;

    DebrisManagerComponent* debrisManager_ = nullptr;
    DroneManagerComponent* droneManager_ = nullptr;
    BossBulletManagerComponent* bulletManager_ = nullptr;

    bool isShieldsInitialized_ = false;
    int initialShieldsSpawned_ = 0;

    // --- ビーム攻撃用 ---
    EnemyBeamComponent* beamComponent_ = nullptr;
    float beamTimer_ = 0.0f;
    float beamInterval_ = 10.0f;
    float beamOffsetZ_ = 4.0f;
    float beamOffsetY_ = -1.0f;
    float beamRange_ = 1000.0f;
};
