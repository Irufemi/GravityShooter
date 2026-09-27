#pragma once
#include "Framework/Component/Component.h"
#include "Combat/IDamageable.h"
#include "Core/Math/Vector2.h"
#include "Renderer/Data/AOEParams.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include <functional>
#include <memory>

/**
 * @enum EnemyAIState
 * @brief 敵キャラクターのAI行動状態
 */
enum class EnemyAIState {
    Approach, //!< 前方定位置への進入
    Combat,   //!< 自機と一定距離を保って滞空・射撃
    Dive,     //!< 特攻急降下（DiveBomber専用: 自機へ向けて急加速突進）
    Disengage //!< 制限時間終了によるすれ違い離脱
};

/**
 * @enum EnemyBehaviorType
 * @brief 敵キャラクターの戦術行動タイプ
 */
enum class EnemyBehaviorType {
    StandardGunner = 0,  //!< 従来の滞空・自機狙い射撃
    DiveBomber = 1,      //!< 特攻急降下（射撃を行わず、高速で自機へ体当たり自爆）
    PredictiveSniper = 2 //!< 偏差射撃（自機の移動ベクトルから未来予測位置を計算して射撃）
};

/**
 * @enum DespawnReason
 * @brief 敵キャラクターがシーンから退場・消滅する明確な理由（AAA基準ライフサイクル管理）
 */
enum class DespawnReason {
    KilledByPlayer,  //!< プレイヤーの攻撃（ガレキ投擲・衝突）により撃破された
    OutOfBounds,     //!< 画面外（自機後方 -30m 等）へすれ違い離脱した
    Timeout,         //!< シーン遷移やウェーブ強制終了による消滅
    CollisionSuicide //!< プレイヤーへの直接体当たりによる自爆
};

class GameObject;
class EnemyBulletManagerComponent;
class SplineComponent;
class SplineFollowerComponent;

/// @brief 敵退場通知リスナー（オブジェクト、退場理由）
using EnemyDespawnListener = std::function<void(GameObject*, DespawnReason)>;

/**
 * @class RailShooterEnemyComponent
 * @brief レールシューティング用の敵キャラクター制御コンポーネント（AIステートマシン・自機狙い射撃対応）
 */
class RailShooterEnemyComponent : public Component, public IDamageable {
public:
    RailShooterEnemyComponent() = default;
    ~RailShooterEnemyComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void Draw() override;
    void OnDisable() override;
    void OnCollisionEnter(GameObject* other) override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "RailShooterEnemyComponent";
    }

    void TakeDamage(float damage) override;
    void TakeDamage(int damage);

    DamageableType GetDamageableType() const override {
        return DamageableType::Enemy;
    }

    bool IsAlive() const {
        return hp_ > 0;
    }

    /**
     * @brief 敵撃破・消滅時のコールバックを設定する（レガシー互換用）
     */
    void SetOnDeathCallback(std::function<void(GameObject*)> callback) {
        onDeathCallback_ = std::move(callback);
    }

    /**
     * @brief 退場理由付きのライフサイクルリスナーを登録する（推奨）
     */
    void SetOnDespawnListener(EnemyDespawnListener listener) {
        onDespawnListener_ = std::move(listener);
    }

    /**
     * @brief 退場イベントを通知する
     */
    void NotifyDespawn(DespawnReason reason);

    void SetScaleMultiplier(float scale) {
        scaleMultiplier_ = scale;
    }
    float GetScaleMultiplier() const {
        return scaleMultiplier_;
    }

    /**
     * @brief レール追従パラメータを一括設定する（WaveEventHandlers等から呼び出し）
     * @param spline レールスプライン
     * @param follower プレイヤーのスプライン追従コンポーネント
     * @param initialDistOffset 出現時のレール前方距離オフセット (m)
     * @param targetDistOffset 交戦時に維持する目標レール距離 (m)
     * @param formationOffset フォーメーションによるローカルXYオフセット
     */
    void SetRailTrackingParams(SplineComponent* spline, SplineFollowerComponent* follower, float initialDistOffset,
                               float targetDistOffset, const Irufemi::Vector2& formationOffset);

    void SetFormationOffset(const Irufemi::Vector2& offset) {
        baseFormationOffset_ = offset;
        currentLocalOffset_ = offset;
    }
    void SetDistanceOffset(float offset) {
        currentDistanceOffset_ = offset;
    }

    // パラメータ設定
    void SetSpeed(float speed) {
        speed_ = speed;
    }
    void SetCombatDuration(float duration) {
        combatDuration_ = duration;
    }
    void SetShootInterval(float interval) {
        shootInterval_ = interval;
    }
    void SetTargetDistance(float dist) {
        targetDistance_ = dist;
    }
    void SetBulletScale(float scale) {
        bulletScale_ = scale;
    }
    void SetBulletSpeed(float speed) {
        bulletSpeed_ = speed;
    }
    void SetBehaviorType(EnemyBehaviorType type) {
        behaviorType_ = static_cast<int>(type);
    }
    EnemyBehaviorType GetBehaviorType() const {
        return static_cast<EnemyBehaviorType>(behaviorType_);
    }

private:
    void ShootAtPlayer(const Irufemi::Vector3& playerPos);
    void ShootPredictiveAtPlayer(const Irufemi::Vector3& playerPos, const Irufemi::Vector3& playerVel);
    GameObject* GetPlayerObject();

    void EnsureTelegraphResources();
    void ResetTelegraph();

private:
    EnemyBulletManagerComponent* bulletManager_ = nullptr; //!< キャッシュされた弾マネージャー
    SplineComponent* cachedSpline_ = nullptr;              //!< キャッシュされたレールスプライン
    SplineFollowerComponent* playerFollower_ = nullptr;    //!< プレイヤーのスプライン追従情報
    float currentDistanceOffset_ = 85.0f;                  //!< レール上の自機からの現在相対距離 (m)
    Irufemi::Vector2 baseFormationOffset_ = {0.0f, 0.0f};  //!< フォーメーションによる基本XYオフセット
    Irufemi::Vector2 currentLocalOffset_ = {0.0f, 0.0f}; //!< 浮遊サイン波が付加された現在XYオフセット

    // スナイパー用AOE予兆（Telegraphing）パラメータ
    float sniperTelegraphDuration_ = 1.0f; //!< 射撃前の予兆レーザー照射時間（秒）
    float sniperLockLeadTime_ = 0.25f;     //!< 射撃前の射線固定（ロック）時間（秒）
    float sniperLaserRadius_ = 0.12f;      //!< 予兆レーザーの半径（細いレーザーサイト）
    float sniperLaserLength_ = 200.0f;     //!< 予兆レーザーの最大到達長
    Irufemi::Vector4 sniperLaserColor_ = {1.0f, 0.15f, 0.15f, 0.85f}; //!< レーザー基本色

    // 内部予兆ステート
    bool isAimLocked_ = false;                              //!< 射線が固定されているか
    Irufemi::Vector3 lockedAimDir_ = {0.0f, 0.0f, -1.0f};   //!< 固定された射撃ベクトル
    Irufemi::Vector3 lockedTargetPos_ = {0.0f, 0.0f, 0.0f}; //!< 固定された目標座標

    // 描画リソース（スナイパー時のみ遅延初期化）
    std::unique_ptr<Primitive3DObject> telegraphCylinder_ = nullptr;
    ConstantBuffer<AOEParams> aoeParamsBuffer_;
    AOEParams aoeParamsData_{};

    int behaviorType_ = 0; //!< 戦術行動タイプ (0: Standard, 1: DiveBomber, 2: PredictiveSniper)
    EnemyAIState state_ = EnemyAIState::Approach; //!< 現在のAIステート
    float stateTimer_ = 0.0f;                     //!< ステート内タイマー
    float combatDuration_ = 7.5f;                 //!< 滞空交戦の制限時間（秒）
    float shootInterval_ = 1.8f;                  //!< 射撃インターバル（秒）
    float shootTimer_ = 0.6f;                     //!< 射撃タイマー
    float targetDistance_ = 65.0f;                //!< 自機前方との維持距離
    float hoverTimer_ = 0.0f;                     //!< 浮遊サイン波タイマー
    int bodyDamage_ = 20;                         //!< 体当たり衝突ダメージ
    float bulletScale_ = 0.3f;                    //!< 敵弾のスケール・コライダー半径
    float bulletSpeed_ = 32.0f;                   //!< 敵弾の飛翔速度

    Irufemi::Vector3 lastPlayerPos_ = {0.0f, 0.0f, 0.0f};  //!< 前フレームのプレイヤー座標
    Irufemi::Vector3 playerVelocity_ = {0.0f, 0.0f, 0.0f}; //!< プレイヤーの推定実効移動速度
    bool hasLastPlayerPos_ = false;                        //!< 前フレーム座標の有効フラグ
    float diveRollAngle_ = 0.0f;                           //!< 特攻急降下時のロール角

    float spawnProgress_ = 0.5f; //!< プレイヤーがどの進行度に達したらアクティブになるか (0.0 ~ 1.0)
    bool isActive_ = false;      //!< 現在活動中かどうか
    float speed_ = 15.0f;        //!< 敵の進入・離脱速度
    int hp_ = 100;               //!< 耐久力

    std::function<void(GameObject*)> onDeathCallback_;
    EnemyDespawnListener onDespawnListener_; //!< 退場理由付きライフサイクルリスナー
    float scaleMultiplier_ = 1.0f;           //!< 敵のサイズ倍率（Lootドロップ量等の算出基準）
};
