#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"

class DebrisManagerComponent;

enum class DebrisState {
    Idle,         ///< 漂流中（自然な浮遊）
    Pulled,       ///< プレイヤーに引き寄せられている
    Orbiting,     ///< プレイヤーの周囲を回転浮遊中
    BossOrbiting, ///< ボスの周囲を回転浮遊中
    Thrown        ///< 敵へ向かってホーミング中
};

/**
 * @class DebrisComponent
 * @brief ガレキの振る舞い（状態遷移と位置の補間）を制御するコンポーネント
 */
class DebrisComponent : public Component {
public:
    DebrisComponent() = default;
    ~DebrisComponent() override = default;

    void Initialize() override;
    void OnEnable() override;
    void OnDisable() override;
    void OnCollisionEnter(GameObject* hitObject) override;

    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "DebrisComponent";
    }

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief ガレキの状態を設定する
     * @param newState 遷移先のステート
     * @param forceVisualUpdate オーラ演出を強制更新するかどうか
     */
    void SetState(DebrisState newState, bool forceVisualUpdate = false);

    /**
     * @brief 現在のガレキ状態を取得する
     * @return ガレキステート
     */
    DebrisState GetState() const {
        return state_;
    }

    /**
     * @brief プールへの返却時または再利用時に状態・オーラを初期化する
     */
    void ResetForPool();

    /**
     * @brief 接触権限をアトミックに消費する（AAA基準: 多重ダメージの完全防止）
     * @return 権限消費に成功した場合は true（初回のみ）、既にヒット済みの場合は false
     */
    bool ConsumeHitAuthority() {
        if (hasConsumedHit_) {
            return false;
        }
        hasConsumedHit_ = true;
        return true;
    }

    /**
     * @brief 接触権限をリセットする
     */
    void ResetHitAuthority() {
        hasConsumedHit_ = false;
    }

    /**
     * @brief 既に接触済みかどうかを取得する
     */
    bool HasConsumedHit() const {
        return hasConsumedHit_;
    }

    /**
     * @brief 現在のステートに基づいてオーラの表示状態および色を更新する
     */
    void UpdateAuraVisuals();

    /**
     * @brief 追従対象のゲームオブジェクトを設定する
     * @param target 目標オブジェクトの弱参照
     */
    void SetTarget(std::weak_ptr<GameObject> target) {
        targetObject_ = target;
    }

    /**
     * @brief 追従対象のゲームオブジェクトを取得する
     * @return 目標オブジェクトの弱参照
     */
    std::weak_ptr<GameObject> GetTarget() const {
        return targetObject_;
    }

    /**
     * @brief 周回軌道パラメータを設定する
     * @param angle 周回角度（ラジアン）
     * @param radius 周回半径 (m)
     */
    void SetOrbitParams(float angle, float radius) {
        orbitAngle_ = angle;
        orbitRadius_ = radius;
    }

    /**
     * @brief 投擲方向ベクトルを設定する
     * @param dir 正規化された投擲方向ベクトル
     */
    void SetThrowDirection(const Irufemi::Vector3& dir) {
        throwDirection_ = dir;
    }

    /**
     * @brief ボスシールドとして破壊された際の解除・消滅処理を行う
     */
    void DestroyAsShield();

    /**
     * @brief 仮想管理IDを設定する
     * @param id ガレキ固有のID
     */
    void SetVirtualId(int id) {
        virtualId_ = id;
    }

    /**
     * @brief 仮想管理IDを取得する
     * @return ガレキ固有のID
     */
    int GetVirtualId() const {
        return virtualId_;
    }

    /**
     * @brief 親マネージャーの参照を設定する
     * @param manager DebrisManagerComponentへのポインタ
     */
    void SetManager(DebrisManagerComponent* manager) {
        manager_ = manager;
    }

    /**
     * @brief バリエーションモデルのインデックスを設定する
     * @param index バリエーションインデックス
     */
    void SetVariationIndex(int index) {
        variationIndex_ = index;
    }

    /**
     * @brief バリエーションモデルのインデックスを取得する
     * @return バリエーションインデックス
     */
    int GetVariationIndex() const {
        return variationIndex_;
    }

    /**
     * @brief ボスに対する衝突ダメージ量を取得する
     * @return ダメージ値
     */
    float GetBossDamage() const;

    /**
     * @brief 通常敵に対する衝突ダメージ量を取得する
     * @return ダメージ値
     */
    float GetEnemyDamage() const;

private:
    friend class DebrisManagerComponent;
    DebrisState state_ = DebrisState::Idle;
    bool hasConsumedHit_ = false;

    int virtualId_ = -1;
    int variationIndex_ = -1;
    DebrisManagerComponent* manager_ = nullptr;

    // 追従・目標用の対象
    std::weak_ptr<GameObject> targetObject_;

    // パラメータ取得用ヘルパー（Managerから取得）
    float GetPullSpeed() const;
    float GetThrowSpeed() const;
    float GetOrbitSpeed() const;
    float GetCameraShakeIntensity() const;
    int GetCameraShakeDurationFrames() const;
    Irufemi::Vector4 GetPlayerAuraColor() const;
    Irufemi::Vector4 GetBossAuraColor() const;
    float GetCatchDistanceSq() const;
    float GetBossShieldRadius() const;
    float GetPullYOffset() const;

    // Orbit Radiusは動的に設定されるためローカルに保持
    float orbitRadius_ = 2.0f;

    // 内部状態
    float baseIdleY_ = 0.0f;
    float idleTimeY_ = 0.0f;
    float orbitAngle_ = 0.0f;
    Irufemi::Vector3 throwDirection_ = {0, 0, 0};
    Irufemi::Vector3 throwOrigin_ = {0, 0, 0};

    // ボス用Orbitパラメータ
    float bossOrbitAngleX_ = 0.0f;
    float bossOrbitAngleY_ = 0.0f;
    float bossOrbitAngleZ_ = 0.0f;
    float bossOrbitSpeedX_ = 0.0f;
    float bossOrbitSpeedY_ = 0.0f;
    float bossOrbitSpeedZ_ = 0.0f;
    float bossOrbitRadiusOffset_ = 0.0f;

    std::string hitEffectKey_ = "Dust";
    std::string explosionModelPath_ = "resources/model/Debris/Generic/Debris_Generic.obj";
    std::weak_ptr<GameObject> effectManagerObj_;
};
