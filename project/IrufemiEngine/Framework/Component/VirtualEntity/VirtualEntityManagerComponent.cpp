#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Core/Math/MathFunction.h"
#include <algorithm>

std::vector<VirtualEntityManagerComponent*> VirtualEntityManagerComponent::instances_;

VirtualEntityManagerComponent::VirtualEntityManagerComponent() {
    instances_.push_back(this);
}

VirtualEntityManagerComponent::~VirtualEntityManagerComponent() {
    CleanUpPool();
    auto it = std::find(instances_.begin(), instances_.end(), this);
    if (it != instances_.end()) {
        instances_.erase(it);
    }
}

void VirtualEntityManagerComponent::OnDestroy() {
    CleanUpPool();
}

void VirtualEntityManagerComponent::CleanUpPool() {
    if (pool_) {
        pool_->ForEach([](const std::shared_ptr<GameObject>& obj) {
            if (obj && !obj->IsDestroyed()) {
                obj->Destroy();
            }
        });
        pool_.reset();
    }
    activeHandles_.clear();
}

int VirtualEntityManagerComponent::GetTotalActiveVirtualInstances() {
    int total = 0;
    for (auto* instance : instances_) {
        total += instance->activeInstanceCount_;
    }
    return total;
}

/**
 * @brief 初期化処理
 * @details JSONシリアライズ等で既にアタッチされている場合も考慮し、
 *          VirtualEntityManagerが必要とするGPUカリング設定を確実にオーバーライドします。
 */
void VirtualEntityManagerComponent::Initialize() {
    OnAwake();
}

void VirtualEntityManagerComponent::OnAwake() {
    batchRenderer_ = gameObject_->GetComponent<ModelBatchRendererComponent>();
    if (!batchRenderer_) {
        batchRenderer_ = gameObject_->AddComponent<ModelBatchRendererComponent>().get();
    }
    // 依存関係にあるレンダラに対して、インスタンス生成元に依存せず確実にGPUカリングを有効化する
    batchRenderer_->SetUseGPUCulling(true);
}

void VirtualEntityManagerComponent::Setup(int poolSize, int maxVirtualInstances,
                                          std::function<std::shared_ptr<GameObject>()> factory) {
    maxPoolSize_ = poolSize;
    maxVirtualInstances_ = maxVirtualInstances;
    pool_ = std::make_unique<ObjectPool<GameObject>>(poolSize, factory);

    dense_.reserve(maxVirtualInstances_);
    sparse_.resize(maxVirtualInstances_, -1);

    // IDを再利用可能なキューに積む
    std::queue<int> empty;
    std::swap(freeIds_, empty); // キューをリセット
    for (int i = 0; i < maxVirtualInstances_; ++i) {
        freeIds_.push(i);
    }
}

int VirtualEntityManagerComponent::AddVirtualInstance(const Irufemi::Vector3& pos, const Irufemi::Vector3& rot,
                                                      const Irufemi::Vector3& scale) {
    if (freeIds_.empty()) {
        return -1; // 上限到達
    }

    int id = freeIds_.front();
    freeIds_.pop();

    VirtualInstance vi;
    vi.id = id;
    vi.position = pos;
    vi.rotation = rot;
    vi.scale = scale;
    vi.isPromoted = false;
    vi.isDestroyed = false;
    vi.promotedHandle = ObjectPool<GameObject>::Handle();

    dense_.push_back(vi);
    sparse_[id] = static_cast<int>(dense_.size() - 1);

    return id;
}

void VirtualEntityManagerComponent::RemoveVirtualInstance(int id) {
    int denseIndex = GetSparseIndex(id);
    if (denseIndex == -1) {
        return; // 範囲外またはすでに存在しない
    }

    auto& vi = dense_[denseIndex];
    if (vi.isPromoted && vi.promotedHandle.IsValid()) {
        auto obj = pool_ ? pool_->Resolve(vi.promotedHandle) : nullptr;
        if (obj) {
            obj->SetIsActive(false);
            activeHandles_.erase(obj.get());
        }
        if (pool_) {
            pool_->Release(vi.promotedHandle);
        }
        vi.promotedHandle = ObjectPool<GameObject>::Handle();
    }

    // Sparse Setの実装：削除対象と末尾要素をスワップして削除（O(1)）
    int lastDenseIndex = static_cast<int>(dense_.size() - 1);
    if (denseIndex != lastDenseIndex) {
        // 末尾の要素を削除対象の位置に移動
        dense_[denseIndex] = dense_[lastDenseIndex];
        // 移動した要素のsparse_を更新
        sparse_[dense_[denseIndex].id] = denseIndex;
    }

    dense_.pop_back();
    sparse_[id] = -1;  // 削除済みマーク
    freeIds_.push(id); // IDを解放して再利用可能にする
}

std::shared_ptr<GameObject> VirtualEntityManagerComponent::Promote(int id) {
    if (!pool_) {
        return nullptr;
    }
    int denseIndex = GetSparseIndex(id);
    if (denseIndex == -1) {
        return nullptr;
    }

    auto& vi = dense_[denseIndex];
    if (!vi.isDestroyed && !vi.isPromoted) {
        auto handle = pool_->Acquire();
        if (handle.IsValid()) {
            auto obj = pool_->Resolve(handle);
            if (obj) {
                obj->SetIsActive(true);
                auto t = obj->GetComponent<TransformComponent>();
                if (t) {
                    t->SetPosition(vi.position);
                    t->SetRotation(vi.rotation);
                    t->SetScale(vi.scale);
                }
                vi.isPromoted = true;
                vi.promotedHandle = handle;
                activeHandles_[obj.get()] = handle;
                gameObject_->AddChild(obj);
                return obj;
            }
        }
    }
    return nullptr;
}

void VirtualEntityManagerComponent::OnRegisterProperties() {
    RegisterProperty("Active Instances (Batch)", &activeInstanceCount_);
}

void VirtualEntityManagerComponent::Demote(int id) {
    int denseIndex = GetSparseIndex(id);
    if (denseIndex == -1) {
        return;
    }

    auto& vi = dense_[denseIndex];
    if (vi.isPromoted && vi.promotedHandle.IsValid()) {
        auto obj = pool_ ? pool_->Resolve(vi.promotedHandle) : nullptr;
        if (obj) {
            // 現在のTransformをVirtualに書き戻す
            auto t = obj->GetComponent<TransformComponent>();
            if (t) {
                vi.position = t->GetPosition();
                vi.rotation = t->GetRotation();
                vi.scale = t->GetScale();
            }

            obj->SetIsActive(false);
            activeHandles_.erase(obj.get());
            gameObject_->RemoveChild(obj);
        }

        if (pool_) {
            pool_->Release(vi.promotedHandle);
        }

        vi.promotedHandle = ObjectPool<GameObject>::Handle();
        vi.isPromoted = false;
    }
}

void VirtualEntityManagerComponent::ReleaseGameObject(const std::shared_ptr<GameObject>& obj) {
    if (obj) {
        obj->SetIsActive(false);
        if (pool_) {
            auto it = activeHandles_.find(obj.get());
            if (it != activeHandles_.end()) {
                pool_->Release(it->second);
                activeHandles_.erase(it);
                gameObject_->RemoveChild(obj);
            }
        }
    }
}

void VirtualEntityManagerComponent::Update() {
    if (!batchRenderer_) {
        return;
    }

    activeInstanceCount_ = static_cast<int>(dense_.size());

    batchRenderer_->ClearInstances();

    // 仮想インスタンス（未昇格）の描画
    for (auto& vi : dense_) {
        if (!vi.isPromoted) {
            Irufemi::Transform t;
            t.translate = vi.position;
            t.rotate = vi.rotation;
            t.scale = vi.scale;
            batchRenderer_->AddInstance(t);
        }
    }

    // 実体化済みのインスタンスの描画（Promote時にAddChildされているためGetChildrenで走査可能）
    for (auto& child : gameObject_->GetChildren()) {
        if (child && child->GetIsActive() && !child->IsDestroyed()) {
            auto t = child->GetComponent<TransformComponent>();
            if (t) {
                batchRenderer_->AddInstanceWorld(t->GetWorldMatrix());
            }
        }
    }
}
