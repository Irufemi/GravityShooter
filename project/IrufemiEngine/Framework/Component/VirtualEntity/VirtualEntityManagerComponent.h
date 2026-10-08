#pragma once
#include "Framework/Component/Component.h"
#include "Core/Utility/ObjectPool.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <vector>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

class GameObject;
class ModelBatchRendererComponent;

/**
 * @struct VirtualInstance
 * @brief 仮想オブジェクトのデータ
 */
struct VirtualInstance {
    int id = -1;
    Irufemi::Vector3 position = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 rotation = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 scale = {1.0f, 1.0f, 1.0f};
    bool isPromoted = false;
    bool isDestroyed = false;
    ObjectPool<GameObject>::Handle promotedHandle;
};

/**
 * @class VirtualEntityManagerComponent
 * @brief 大量のインスタンスをデータとして管理し、必要に応じてGameObjectとして実体化（Promote）するコンポーネント
 */
class VirtualEntityManagerComponent : public Component {
public:
    VirtualEntityManagerComponent();
    ~VirtualEntityManagerComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（ModelBatchRendererComponentの取得とGPUカリング設定）を行います
     */
    void OnAwake() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief コンポーネント破棄時の処理。プール内の全GameObjectを安全にDestroyする。
     */
    void OnDestroy() override;
    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "VirtualEntityManagerComponent";
    }
    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief プールとファクトリの設定を行う
     * @param poolSize 実体化（GameObject）用プールの最大サイズ
     * @param maxVirtualInstances 仮想インスタンスの最大予約数
     * @param factory GameObjectを生成するファクトリ関数
     */
    void Setup(int poolSize, int maxVirtualInstances, std::function<std::shared_ptr<GameObject>()> factory);

    /**
     * @brief 仮想インスタンスを追加する
     * @return 割り当てられたID
     */
    int AddVirtualInstance(const Irufemi::Vector3& pos, const Irufemi::Vector3& rot = {0, 0, 0},
                           const Irufemi::Vector3& scale = {1, 1, 1});

    /**
     * @brief 仮想インスタンスを論理削除する
     */
    void RemoveVirtualInstance(int id);

    /**
     * @brief 仮想インスタンスをGameObjectに昇格させる
     * @return 昇格した実体（プールが枯渇した場合はnullptr）
     */
    std::shared_ptr<GameObject> Promote(int id);

    /**
     * @brief GameObjectを仮想インスタンスに降格させ、プールに返却する
     */
    void Demote(int id);

    /**
     * @brief データ配列（密配列・連続メモリ）を取得（読み取り専用）
     */
    const std::vector<VirtualInstance>& GetDenseInstances() const {
        return dense_;
    }

    /**
     * @brief 仮想IDからインスタンスを取得する（読み取り専用、存在しない場合はnullptr）
     * @param[in] virtualId 仮想ID
     * @return インスタンスポインタ
     */
    const VirtualInstance* GetInstance(int virtualId) const {
        int index = GetSparseIndex(virtualId);
        if (index >= 0 && index < static_cast<int>(dense_.size())) {
            return &dense_[index];
        }
        return nullptr;
    }

    /**
     * @brief 仮想IDからインスタンスを取得する（存在しない場合はnullptr）
     * @param[in] virtualId 仮想ID
     * @return インスタンスポインタ
     */
    VirtualInstance* GetInstance(int virtualId) {
        return const_cast<VirtualInstance*>(std::as_const(*this).GetInstance(virtualId));
    }

    /**
     * @brief プールから取得した実体（仮想インスタンスに紐付いていない場合など）を直接プールに返却する
     */
    void ReleaseGameObject(const std::shared_ptr<GameObject>& obj);

    /**
     * @brief 現在アクティブな自インスタンスの総数を取得する
     */
    int GetActiveInstanceCount() const {
        return static_cast<int>(dense_.size());
    }

    /**
     * @brief 仮想インスタンスの最大予約数を取得する
     */
    int GetMaxVirtualInstances() const {
        return maxVirtualInstances_;
    }

    /**
     * @brief 実体化用オブジェクトプールの最大サイズを取得する
     */
    int GetMaxPoolSize() const {
        return maxPoolSize_;
    }

    /**
     * @brief 描画用 ModelBatchRendererComponent を取得する
     */
    ModelBatchRendererComponent* GetBatchRenderer() const {
        return batchRenderer_;
    }

    /**
     * @brief 現在アクティブな全ての仮想インスタンスの総数を取得する
     */
    static int GetTotalActiveVirtualInstances();

private:
    /**
     * @brief 仮想IDから密配列のインデックスを取得（内部ヘルパー）
     */
    int GetSparseIndex(int virtualId) const {
        if (virtualId >= 0 && virtualId < static_cast<int>(sparse_.size())) {
            return sparse_[virtualId];
        }
        return -1;
    }

    /**
     * @brief プール内のオブジェクトを安全に解放・破棄する
     */
    void CleanUpPool();

    // --- 内部データ構造 ---
    static std::vector<VirtualEntityManagerComponent*> instances_;
    std::vector<VirtualInstance> dense_;
    std::vector<int> sparse_;
    std::queue<int> freeIds_;
    int activeInstanceCount_ = 0;
    int maxVirtualInstances_ = 0;

    std::unique_ptr<ObjectPool<GameObject>> pool_;
    int maxPoolSize_ = 0;

    ModelBatchRendererComponent* batchRenderer_ = nullptr;
    std::unordered_map<GameObject*, ObjectPool<GameObject>::Handle> activeHandles_;
};
