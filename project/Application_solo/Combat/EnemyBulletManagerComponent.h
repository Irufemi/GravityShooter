#pragma once
#include "Framework/Component/Component.h"
#include "Core/Utility/ObjectPool.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>

class GameObject;
class BaseScene;
class EnemyBulletComponent;

/**
 * @class EnemyBulletManagerComponent
 * @brief 雑魚敵が発射する弾を一元プール管理（ゼロアロケーション）するマネージャーコンポーネント
 */
class EnemyBulletManagerComponent : public Component {
public:
    EnemyBulletManagerComponent();
    ~EnemyBulletManagerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void OnDestroy() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "EnemyBulletManagerComponent";
    }

    /**
     * @brief アクティブな EnemyBulletManagerComponent の静的インスタンスを取得する（O(1)）
     * @return EnemyBulletManagerComponent のポインタ（未登録時は nullptr）
     */
    static EnemyBulletManagerComponent* GetInstance() {
        return s_instance_;
    }

    /**
     * @brief シーンから EnemyBulletManagerComponent を取得し、存在しない場合は自動生成して返す
     * @param scene 検索・生成先のシーン
     * @return EnemyBulletManagerComponent のポインタ（失敗時は nullptr）
     */
    static EnemyBulletManagerComponent* GetOrCreate(BaseScene* scene);

    /**
     * @brief プールから弾を取得し、指定のパラメータで射出する
     * @param origin 発射起点座標
     * @param direction 飛翔方向の単位ベクトル
     * @param speed 飛翔速度
     * @param damage 被弾ダメージ
     * @param scale 弾のスケール・コライダー半径
     * @return 射出された弾の GameObject ポインタ（プール枯渇時は nullptr）
     */
    GameObject* FireBullet(const Irufemi::Vector3& origin, const Irufemi::Vector3& direction, float speed = 30.0f,
                           int damage = 10, float scale = 0.4f);

    /**
     * @brief 寿命終了または衝突した弾コンポーネントを受け取り、O(1) でプールへ返却する
     * @param bulletComp 返却する弾の EnemyBulletComponent
     */
    void ReturnBullet(EnemyBulletComponent* bulletComp);

    /**
     * @brief 寿命終了または衝突した弾オブジェクトを受け取り、プールへ返却する（互換用）
     * @param bullet 返却する弾の GameObject
     */
    void ReturnBullet(GameObject* bullet);

private:
    void WarmupPool();

    static inline EnemyBulletManagerComponent* s_instance_ = nullptr; //!< 静的サービスロケータインスタンス

    int maxBullets_ = 64;                                                         //!< プール最大容量
    std::string bulletModelPath_ = "resources/model/EnemyBullet/EnemyBullet.obj"; //!< 弾の3Dモデルパス
    std::unique_ptr<ObjectPool<GameObject>> bulletPool_;                          //!< 弾のオブジェクトプール
    bool isWarmedUp_ = false; //!< 事前ウォームアップ完了フラグ
};
