#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <limits>

class DescriptorPool {
public:
    static constexpr uint32_t kMaxSRVCount = 16384;
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu; // std::numeric_limits<uint32_t>::max() の代用

    DescriptorPool() = default;
    /**
     * @brief CBV/SRV/UAV用デスクリプタヒープを初期化する
     * @param[in] device D3D12デバイス
     */
    void Initialize(ID3D12Device* device);

    /**
     * @brief デスクリプタヒープから連続するスロットを割り当てる
     * @param[in] count 確保するデスクリプタ数
     * @return 割り当てられた先頭インデックス（失敗時は kInvalid）
     */
    uint32_t Allocate(uint32_t count = 1);
    /**
     * @brief デスクリプタスロットを即時解放してフリーリストに返却する
     * @param[in] index 解放するインデックス
     */
    void Free(uint32_t index);
    /**
     * @brief 指定フレームのGPU実行完了後にデスクリプタスロットを解放する
     * @param[in] index 解放するインデックス
     * @param[in] safeFence 解放が可能になる完了目標フェンス値
     */
    void FreeAfterFence(uint32_t index, uint64_t safeFence);
    /**
     * @brief 完了したフェンス値に基づいて保留中のデスクリプタを回収する
     * @param[in] completedFence 現在完了しているGPUフェンス値
     */
    void GarbageCollect(uint64_t completedFence);

    /**
     * @brief 使用中インデックス集合(昇順ユニーク)以外の未使用スロットでフリーリストを再構築する
     * @param[in] usedSortedUnique 現在使用中のソート済みユニークインデックスリスト
     */
    void RebuildFreeListExcept(const std::vector<uint32_t>& usedSortedUnique);

    /**
     * @brief ヒープ先頭の特定スロット数（ImGui 等用）を予約確保する
     * @param[in] count 予約するスロット数
     */
    void ReservePrefix(uint32_t count);

    /**
     * @brief 指定インデックスの CPU デスクリプタハンドルを取得する
     * @param[in] index 取得対象インデックス
     * @return CPU デスクリプタハンドル
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    /**
     * @brief 指定インデックスの GPU デスクリプタハンドルを取得する
     * @param[in] index 取得対象インデックス
     * @return GPU デスクリプタハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;
    /**
     * @brief IndexFromGPUHandle を取得する。
     * @return 取得された IndexFromGPUHandle
     */
    uint32_t GetIndexFromGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) const;

    // SRV作成
    /**
     * @brief CreateSRVForTexture2D を実行する。
     */
    void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
    /**
     * @brief CreateSRVForStructuredBuffer を実行する。
     */
    void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
                                      UINT structureByteStride);

    /**
     * @brief Heap を取得する。
     * @return 取得された Heap
     */
    ID3D12DescriptorHeap* GetHeap() const {
        return heap_.Get();
    }
    /**
     * @brief Capacity を実行する。
     */
    uint32_t Capacity() const {
        return kMaxSRVCount;
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

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t nextIndex_ = 0;

    std::vector<uint32_t> freeList_;
    std::priority_queue<Pending> pending_;
    // NOTE: GPU 描画スレッドや非同期ロードスレッドからの SRV/UAV スロット割当・GC 回収を
    // 排他制御するための低レイヤー生ミューテックスです。
    mutable std::mutex mutex_;
};