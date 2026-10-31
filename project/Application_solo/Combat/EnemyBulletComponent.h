#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Utility/ObjectPool.h"

class GameObject;
class EnemyBulletManagerComponent;

/**
 * @class EnemyBulletComponent
 * @brief 敵が発射する自機狙い弾の移動・寿命・プレイヤー衝突判定を制御するコンポーネント
 */
class EnemyBulletComponent : public Component {
public:
    EnemyBulletComponent() = default;
    ~EnemyBulletComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnCollisionEnter(GameObject* other) override;

    std::string GetComponentName() const override {
        return "EnemyBulletComponent";
    }

    /**
     * @brief 弾の進行方向と速度、威力を設定して射出する
     * @param direction 射出方向の単位ベクトル
     * @param speed 移動速度（デフォルト 30.0f）
     * @param damage プレイヤーへの被弾ダメージ（デフォルト 10）
     */
    void Launch(const Irufemi::Vector3& direction, float speed = 30.0f, int damage = 10);

    /**
     * @brief オブジェクトプール再利用時の状態初期化
     */
    void ResetForPool();

    /**
     * @brief 管理元マネージャーを設定する
     * @param manager 所属する EnemyBulletManagerComponent のポインタ
     */
    void SetManager(EnemyBulletManagerComponent* manager) {
        manager_ = manager;
    }

    /**
     * @brief プール管理用のハンドルを設定する
     * @param handle オブジェクトプールのハンドル
     */
    void SetPoolHandle(ObjectPool<GameObject>::Handle handle) {
        poolHandle_ = handle;
    }

    /**
     * @brief プール管理用のハンドルを取得する
     * @return 保持しているオブジェクトプールハンドル
     */
    ObjectPool<GameObject>::Handle GetPoolHandle() const {
        return poolHandle_;
    }

    /**
     * @brief 弾を非アクティブ化し、マネージャーへ返却（または破棄）する
     */
    /**
     * @brief 着弾エフェクトマネージャーのGameObject参照を事前に設定する
     * @param effectManagerObj エフェクトマネージャーGameObjectの弱参照
     */
    void SetEffectManagerObj(std::weak_ptr<GameObject> effectManagerObj) {
        effectManagerObj_ = effectManagerObj;
    }

    void Deactivate();

private:
    EnemyBulletManagerComponent* manager_ = nullptr; //!< 管理元マネージャー
    ObjectPool<GameObject>::Handle poolHandle_;      //!< プール管理用ハンドル
    Irufemi::Vector3 velocity_{0.0f, 0.0f, 0.0f};    //!< 移動速度ベクトル
    int damage_ = 10;                                //!< 命中時のダメージ量
    float lifeTimer_ = 0.0f;                         //!< 生存タイマー
    float maxLifeTime_ = 5.0f;                       //!< 最大寿命（秒）
    mutable std::weak_ptr<GameObject> effectManagerObj_; //!< 着弾エフェクトマネージャーGameObjectへの弱参照キャッシュ
};
