#include "Combat/EnemyBulletManagerComponent.h"
#include "Combat/EnemyBulletComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Physics/CollisionManager.h"
#include "Core/System/IrufemiEngine.h"

EnemyBulletManagerComponent::EnemyBulletManagerComponent() {
    s_instance_ = this;
}

void EnemyBulletManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Max Bullets", &maxBullets_);
    RegisterProperty("Bullet Model Path", &bulletModelPath_);
}

void EnemyBulletManagerComponent::Initialize() {
    // Editモード中（シーン編集時）の不要なプール生成を防止するため、Initializeではプレウォームを行わない
}

void EnemyBulletManagerComponent::Start() {
    WarmupPool();
}

void EnemyBulletManagerComponent::OnDestroy() {
    if (s_instance_ == this) {
        s_instance_ = nullptr;
    }
    bulletPool_.reset();
}

EnemyBulletManagerComponent* EnemyBulletManagerComponent::GetOrCreate(BaseScene* scene) {
    if (s_instance_) {
        return s_instance_;
    }
    if (!scene) {
        return nullptr;
    }

    if (auto obj = scene->FindGameObject("EnemyBulletManager")) {
        if (auto mgr = obj->GetComponent<EnemyBulletManagerComponent>()) {
            return mgr;
        }
    }

    // シーン上に存在しない場合は自動プロビジョニング
    auto managerObj = std::make_shared<GameObject>("EnemyBulletManager");
    managerObj->SetIsSerializable(false);
    auto mgr = managerObj->AddComponent<EnemyBulletManagerComponent>();
    scene->AddGameObject(managerObj);
    mgr->Initialize();
    return mgr.get();
}

void EnemyBulletManagerComponent::WarmupPool() {
    if (isWarmedUp_ || !gameObject_) {
        return;
    }

    auto initialScene = gameObject_->GetScene();
    if (!initialScene) {
        return;
    }

    // ラムダ式内では scene ポインタをキャプチャせず、gameObject_ から最新の有効なシーンを取得する（ダングリング防止）
    bulletPool_ = std::make_unique<ObjectPool<GameObject>>(maxBullets_, [this]() {
        auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
        if (!scene) {
            return std::make_shared<GameObject>("EnemyBullet");
        }

        auto bullet = std::make_shared<GameObject>("EnemyBullet");
        bullet->SetIsSerializable(false);
        bullet->SetHideInHierarchy(true); // エディタのヒエラルキーを汚染しないよう非表示設定

        // レンダラー設定
        auto meshRenderer = bullet->AddComponent<MeshRendererComponent>();
        meshRenderer->LoadModel(bulletModelPath_);
        meshRenderer->Initialize();

        // コライダー設定
        auto collider = bullet->AddComponent<SphereColliderComponent>();
        collider->Initialize();
        collider->SetTrigger(true);
        collider->SetLocalRadius(0.4f);

        if (auto engine = GetEngine()) {
            if (auto cm = engine->GetCollisionManager()) {
                collider->SetLayer(cm->GetLayerMask("Enemy"));
                uint32_t maskPlayer = cm->GetLayerMask("Player");
                uint32_t maskDebrisPlayer = cm->GetLayerMask("Debris_Player");
                uint32_t maskEnvironment = cm->GetLayerMask("Environment");
                // 自機シールドおよび建造物との衝突判定を有効化
                collider->SetMask(maskPlayer | maskDebrisPlayer | maskEnvironment);
            }
        }

        // 弾ロジック設定
        auto bulletComp = bullet->AddComponent<EnemyBulletComponent>();
        bulletComp->Initialize();
        bulletComp->SetManager(this);

        // マネージャーの子オブジェクトとして登録し、非アクティブにして待機（ルート直置きを解消）
        gameObject_->AddChild(bullet);
        bullet->SetIsActive(false);

        return bullet;
    });

    isWarmedUp_ = true;
}

GameObject* EnemyBulletManagerComponent::FireBullet(const Irufemi::Vector3& origin, const Irufemi::Vector3& direction,
                                                    float speed, int damage, float scale) {
    if (!isWarmedUp_) {
        WarmupPool();
    }
    if (!bulletPool_) {
        return nullptr;
    }

    auto handle = bulletPool_->Acquire();
    if (!handle.IsValid()) {
        return nullptr; // プール枯渇時は安全にスキップ
    }

    auto bullet = bulletPool_->Resolve(handle);
    if (!bullet) {
        return nullptr;
    }

    // トランスフォームとコライダーの設定
    if (auto transform = bullet->GetTransform()) {
        transform->SetWorldPosition(origin);
        transform->SetScale({scale, scale, scale});
    }

    if (auto collider = bullet->GetComponent<SphereColliderComponent>()) {
        collider->SetLocalRadius(scale);
    }

    if (auto bulletComp = bullet->GetComponent<EnemyBulletComponent>()) {
        bulletComp->SetManager(this);
        bulletComp->SetPoolHandle(handle); // O(1) 返却用ハンドルを弾自身に保持
        bulletComp->Launch(direction, speed, damage);
    }

    bullet->SetIsActive(true);
    return bullet.get();
}

void EnemyBulletManagerComponent::ReturnBullet(EnemyBulletComponent* bulletComp) {
    if (!bulletComp || !bulletPool_) {
        return;
    }

    bulletComp->ResetForPool();

    if (auto bulletGo = bulletComp->GetGameObject()) {
        bulletGo->SetIsActive(false);
    }

    // ハッシュマップ検索なしで O(1) 即時返却
    bulletPool_->Release(bulletComp->GetPoolHandle());
}

void EnemyBulletManagerComponent::ReturnBullet(GameObject* bullet) {
    if (!bullet) {
        return;
    }
    ReturnBullet(bullet->GetComponent<EnemyBulletComponent>());
}
