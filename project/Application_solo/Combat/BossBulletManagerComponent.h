#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Shape/Sphere.h"
#include "Core/Math/Geometry/AABB.h"
#include <memory>
#include <vector>

class GameObject;
class VirtualEntityManagerComponent;
class ColliderComponent;
class PlayerHealthComponent;
class DestructibleEnvironmentComponent;
class DebrisComponent;

class BossBulletManagerComponent : public Component {
public:
    BossBulletManagerComponent();
    ~BossBulletManagerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;

    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "BossBulletManagerComponent";
    }

    void SetTargetPlayerID(uint64_t id) {
        targetPlayerID_ = id;
    }

    /**
     * @brief シーン読み込み時等にIDの付け替えが発生した際のコールバック
     * @param idMap 旧IDと新IDの対応マップ
     */
    void OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) override;

    /**
     * @brief 指定した座標と速度で弾を発射する
     */
    void SpawnBullet(const Irufemi::Vector3& position, const Irufemi::Vector3& velocity);

    /**
     * @brief 弾の回収（寿命や被弾など）
     */
    void ReleaseBullet(int virtualId);

    /**
     * @brief 自機撃破時、アクティブな全ボス弾を一括回収・消滅させる
     */
    void ClearAllBullets();

private:
    struct BossBulletData {
        Irufemi::Vector3 velocity = {0.0f, 0.0f, 0.0f};
        float lifeTimer = 0.0f;
    };

    int maxBullets_ = 2000;
    float defaultLifeTime_ = 5.0f;
    Irufemi::Vector3 bulletScale_ = {0.5f, 0.5f, 0.5f};
    float hitRadius_ = 0.5f;
    std::string hitEffectKey_ = "Dust";
    std::string bulletPrefabPath_ = "resources/prefabs/BossBullet.json";
    Irufemi::Vector4 bulletColor_ = {0.85f, 0.15f, 1.0f, 1.0f}; ///< ボス弾のカラー（鮮やかな紫色）
    /// @brief 攻撃対象となるプレイヤーのGameObject ID
    uint64_t targetPlayerID_ = 0;

    VirtualEntityManagerComponent* virtualManager_ = nullptr;
    std::vector<BossBulletData> bulletDataList_;
    std::vector<int> activeVirtualIds_;
    std::vector<ColliderComponent*> prefetchedColliders_;
    class EffectManagerComponent* effectManager_ = nullptr;

    struct CachedTargetProxy {
        ColliderComponent* collider = nullptr;
        GameObject* gameObject = nullptr;
        PlayerHealthComponent* playerHealth = nullptr;
        DestructibleEnvironmentComponent* destructible = nullptr;
        DebrisComponent* debris = nullptr;
        bool isEnvironment = false;
        bool isSphere = false;
        Irufemi::Sphere sphere{};
        Irufemi::AABB broadAABB{};
    };

    std::vector<CachedTargetProxy> targetProxies_;
};
