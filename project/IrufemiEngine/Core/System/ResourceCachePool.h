#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <functional>
#include "Core/System/ResourceHandle.h"

namespace Irufemi {

/**
 * @struct ResourceSlotMeta
 * @brief 各リソーススロットの管理用メタデータ
 */
struct ResourceSlotMeta {
    uint32_t generation = 0;
    uint32_t refCount = 0;
    uint64_t lastAccessTime = 0;
    size_t memorySize = 0;
    bool isLoaded = false;
};

// パージ時に呼び出されるコールバック型
using ResourcePurgeCallback = std::function<void(uint32_t index)>;

/**
 * @class ResourceCachePool
 * @brief リソースのメタデータ管理、LRUパージ、メモリ予算の監視を行うプールクラス。
 * 実データ（テクスチャ等）の型には依存せず、スロットと世代のみを管理します。
 */
class ResourceCachePool {
public:
    /**
     * @brief コンストラクタ
     * @param maxMemoryBytes メモリ予算（0なら無制限）
     * @param initialCapacity プールの初期容量
     */
    ResourceCachePool(size_t maxMemoryBytes = 0, size_t initialCapacity = 256);
    ~ResourceCachePool();

    /**
     * @brief メモリ予算を設定する
     * @param maxBytes 最大バイト数
     */
    void SetMemoryBudget(size_t maxBytes);

    /**
     * @brief MemoryBudget を取得する。
     * @return 取得された MemoryBudget
     */
    size_t GetMemoryBudget() const {
        return maxMemoryBytes_;
    }
    /**
     * @brief CurrentMemoryUsage を取得する。
     * @return 取得された CurrentMemoryUsage
     */
    size_t GetCurrentMemoryUsage() const {
        return currentMemoryUsage_;
    }

    /**
     * @brief 新しいリソーススロットを確保する
     * @param memorySize リソースの予測サイズ
     * @return 割り当てられたハンドル
     */
    ResourceHandle AllocateSlot(size_t memorySize);

    /**
     * @brief リソースの参照カウントを減らす（0になっても即座にパージはされない）
     * @param handle 解放するハンドル
     */
    void ReleaseSlot(ResourceHandle handle);

    /**
     * @brief リソースの参照カウントを増やす（コンポーネントが保持する時などに呼ぶ）
     * @param handle 保持するハンドル
     */
    void RetainSlot(ResourceHandle handle);

    /**
     * @brief リソースにアクセスしたことを記録し、パージ対象から遠ざける
     * @param handle アクセスしたハンドル
     */
    void TouchSlot(ResourceHandle handle);

    /**
     * @brief ハンドルが有効（現在の世代と一致しているか）かチェックする
     */
    bool IsValid(ResourceHandle handle) const;

    /**
     * @brief リソースのサイズを後から更新する（ロード完了時などに使用）
     */
    void UpdateSlotSize(ResourceHandle handle, size_t newSize);

    /**
     * @brief リソースのロード完了状態を設定する
     */
    void SetLoaded(ResourceHandle handle, bool loaded);
    /**
     * @brief IsLoaded かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsLoaded(ResourceHandle handle) const;

    /**
     * @brief メモリ予算を超過している場合、古いもの（LRU）からコールバックを呼んでパージする
     * @param purgeCallback 破棄処理を行うマネージャ側のコールバック
     */
    void EnforceMemoryBudget(const ResourcePurgeCallback& purgeCallback);

    /**
     * @brief キャッシュを強制的にクリアする（シーン切り替え時など）
     */
    void ClearAll(const ResourcePurgeCallback& purgeCallback);

private:
    size_t maxMemoryBytes_ = 0;
    size_t currentMemoryUsage_ = 0;
    uint64_t currentTimeCounter_ = 0;

    std::vector<ResourceSlotMeta> slots_;
    std::vector<uint32_t> freeIndices_;
    mutable std::mutex mutex_;
};

} // namespace Irufemi
