#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <limits>

namespace Irufemi {

class DescriptorAllocator {
public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu; // std::numeric_limits<uint32_t>::max() の代用

    DescriptorAllocator(ID3D12DescriptorHeap* heap, uint32_t descriptorSize, uint32_t baseIndex = 0);

    /**
     * @brief Allocate を実行する。
     */
    uint32_t Allocate();
    /**
     * @brief Free を実行する。
     */
    void Free(uint32_t index);
    /**
     * @brief FreeAfterFence を実行する。
     */
    void FreeAfterFence(uint32_t index, uint64_t safeFence);
    /**
     * @brief GarbageCollect を実行する。
     */
    void GarbageCollect(uint64_t completedFence);

    // 使用中インデックス集合(昇順ユニーク)を渡してフリーリストを再構築
    /**
     * @brief RebuildFreeListExcept を実行する。
     */
    void RebuildFreeListExcept(const std::vector<uint32_t>& usedSortedUnique);

    // 先頭の予約(ImGui 等)
    /**
     * @brief ReservePrefix を実行する。
     */
    void ReservePrefix(uint32_t count);

    /**
     * @brief CPUHandle を取得する。
     * @return 取得された CPUHandle
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    /**
     * @brief GPUHandle を取得する。
     * @return 取得された GPUHandle
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

    /**
     * @brief Capacity を実行する。
     */
    uint32_t Capacity() const {
        return capacity_;
    }
    /**
     * @brief BaseIndex を実行する。
     */
    uint32_t BaseIndex() const {
        return baseIndex_;
    }

private:
    struct Pending {
        uint64_t fence;
        uint32_t index;
        bool operator<(const Pending& rhs) const {
            return fence > rhs.fence;
        } // フェンス小→大
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t capacity_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t nextIndex_ = 0;

    std::vector<uint32_t> freeList_;
    std::vector<bool> inUse_;
    std::priority_queue<Pending> pending_;
    mutable std::mutex mutex_;
};

} // namespace Irufemi