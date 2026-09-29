#include "Effects/EffectManagerComponent.h"
#include <Windows.h>
#include <cstdio>
#include <iostream>
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/Component/Utility/LifetimeComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/Utility/Log.h"
#include <iostream>

EffectManagerComponent::EffectManagerComponent() {
    s_instance_ = this;
}

void EffectManagerComponent::OnDestroy() {
    if (s_instance_ == this) {
        s_instance_ = nullptr;
    }
}

void EffectManagerComponent::OnRegisterProperties() {
    RegisterProperty("Hit Effect Path", &hitEffectPath_);
    RegisterProperty("Dust Effect Path", &dustEffectPath_);
    RegisterProperty("Thruster Effect Path", &thrusterEffectPath_);
}

void EffectManagerComponent::Initialize() {
    effectDictionary_["Hit"] = hitEffectPath_;
    effectDictionary_["Dust"] = dustEffectPath_;
    effectDictionary_["debris_dust_effect"] = dustEffectPath_;
    effectDictionary_["Thruster"] = thrusterEffectPath_;
}

void EffectManagerComponent::Start() {
    for (const auto& [key, path] : effectDictionary_) {
        GetOrCreatePool(key, path);
    }
}

ObjectPool<GameObject>* EffectManagerComponent::GetOrCreatePool(const std::string& effectKey,
                                                                const std::string& prefabPath) {
    auto it = effectPools_.find(effectKey);
    if (it != effectPools_.end()) {
        return it->second.get();
    }
    if (prefabPath.empty()) {
        return nullptr;
    }

    int poolCapacity = (effectKey == "Hit") ? maxHitEffects_ : maxDustEffects_;
    auto pool = std::make_unique<ObjectPool<GameObject>>(poolCapacity, [this, prefabPath]() {
        // makeChild = true でマネージャーの子として生成し、ルート直置きを解消
        auto obj = gameObject_->Instantiate(prefabPath, {0.0f, 0.0f, 0.0f}, true);
        if (obj) {
            obj->SetIsSerializable(false); // セーブデータへの混入を確実に防止
            obj->SetHideInHierarchy(true); // エディタのヒエラルキーを汚染しないよう非表示設定
            obj->SetIsActive(false);       // Removeせずに非アクティブ状態で休眠させる

            // 寿命コンポーネントがあれば、プール運用のためにDestroyではなくDisableに変更する
            if (auto lifetime = obj->GetComponent<LifetimeComponent>()) {
                lifetime->SetTimeoutAction(TimeoutAction::Disable);
            }
        }
        return obj;
    });

    auto* rawPool = pool.get();
    effectPools_[effectKey] = std::move(pool);
    return rawPool;
}

void EffectManagerComponent::Update() {
    for (size_t i = 0; i < activeEffects_.size();) {
        auto& active = activeEffects_[i];
        auto poolIt = effectPools_.find(active.effectKey);
        if (poolIt == effectPools_.end() || !poolIt->second) {
            // プールがないか不明なエフェクト: Swap-and-Pop で O(1) 削除
            active = std::move(activeEffects_.back());
            activeEffects_.pop_back();
            continue;
        }

        auto* pool = poolIt->second.get();
        auto obj = pool->Resolve(active.handle);
        if (obj && !obj->GetIsActive()) {
            pool->Release(active.handle);
            active = std::move(activeEffects_.back());
            activeEffects_.pop_back();
            continue;
        }

        ++i;
    }
}

void EffectManagerComponent::PlayEffect(const std::string& effectKey, const Irufemi::Vector3& worldPosition) {
    if (!gameObject_) {
        return;
    }

    auto it = effectDictionary_.find(effectKey);
    if (it == effectDictionary_.end() || it->second.empty()) {
        return;
    }

    ObjectPool<GameObject>* targetPool = GetOrCreatePool(effectKey, it->second);
    if (targetPool) {
        auto handle = targetPool->Acquire();

        // プールが枯渇した場合、一番古い同じ種類のエフェクトを強制終了して再利用する
        if (!handle.IsValid() && !activeEffects_.empty()) {
            auto oldestIt = activeEffects_.begin();
            while (oldestIt != activeEffects_.end() && oldestIt->effectKey != effectKey) {
                ++oldestIt;
            }

            if (oldestIt != activeEffects_.end()) {
                auto obj = targetPool->Resolve(oldestIt->handle);
                if (obj) {
                    obj->SetIsActive(false);
                }
                targetPool->Release(oldestIt->handle);
                activeEffects_.erase(oldestIt);
                handle = targetPool->Acquire();
            }
        }

        if (handle.IsValid()) {
            auto obj = targetPool->Resolve(handle);
            if (obj) {
                if (auto t = obj->GetComponent<TransformComponent>()) {
                    t->SetWorldPosition(worldPosition);
                }

                // アクティブ化して LifetimeComponent のタイマーをリセットする
                obj->SetIsActive(true);
                if (auto lifetime = obj->GetComponent<LifetimeComponent>()) {
                    lifetime->Initialize();
                }

                // ツリー全体からすべての ParticleEmitterComponent を取得して再発火させる
                auto emitters = obj->GetComponentsInChildren<ParticleEmitterComponent>();
                for (auto pe : emitters) {
                    pe->Restart(false);
                }

                // ツリー全体からすべての VoxelParticleComponent を取得して爆発させる
                auto voxelEmitters = obj->GetComponentsInChildren<VoxelParticleComponent>();
                for (auto ve : voxelEmitters) {
                    ve->Explode();
                }

                activeEffects_.push_back({handle, 0.0f, effectKey}); // timerはもう使わないが構造体互換のため0をセット
            }
        }
    }
}

std::shared_ptr<GameObject> EffectManagerComponent::PlayAttachedEffect(
    const std::string& effectKey,
    std::shared_ptr<GameObject> parent,
    const Irufemi::Vector3& localOffset
) {
    if (!parent) {
        return nullptr;
    }

    auto it = effectDictionary_.find(effectKey);
    if (it == effectDictionary_.end() || it->second.empty()) {
        return nullptr;
    }

    // 親GameObjectの子ノードとしてプレハブをインスタンス化（Transform階層が自動結合）
    auto effectObj = parent->Instantiate(it->second, localOffset, true);
    if (effectObj) {
        effectObj->SetIsSerializable(false); // シーンセーブデータへの混入を防止
        effectObj->SetHideInHierarchy(true); // エディタHierarchyの汚染を防止
        effectObj->SetIsActive(true);

        // すべてのパーティクルエミッターを起動
        auto emitters = effectObj->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto pe : emitters) {
            pe->Play();
        }
    }
    return effectObj;
}

