#pragma once
#include "Core/Utility/ErrorUtility.h"
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include "RHI/DirectX12/DirectXCommon.h"

/**
 * @class DynamicConstantBuffer
 * @brief エンジン全体で共有される巨大な定数バッファを管理するクラス
 * @details 数万オブジェクトの定数データを1つの ID3D12Resource で管理し、
 * 各オブジェクトにはインデックスのみを割り当てます。
 */
template <typename T> class DynamicConstantBuffer {
public:
    DynamicConstantBuffer() = default;
    ~DynamicConstantBuffer() {
        Unmap();
        if (dxCommon_) {
            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                if (resources_[i]) {
                    dxCommon_->ReleaseAfterFence(resources_[i]);
                }
            }
        }
    }

    DynamicConstantBuffer(const DynamicConstantBuffer&) = delete;
    DynamicConstantBuffer& operator=(const DynamicConstantBuffer&) = delete;

    /**
     * @brief バッファの初期化
     * @param dxCommon DirectX基盤
     * @param capacity 確保する最大要素数（例: 65536）
     */
    void Initialize(DirectXCommon* dxCommon, uint32_t capacity) {
        IRUFEMI_ASSERT(dxCommon != nullptr);
        dxCommon_ = dxCommon;
        capacity_ = capacity;
        inUse_.assign(capacity_, false);
        // 定数バッファは256バイトアライメント必須
        elementAlignedSize_ = (sizeof(T) + 255) & ~255;

        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            resources_[i] = dxCommon_->CreateBufferResource(elementAlignedSize_ * capacity_);
            HRESULT hr = resources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedRawData_[i]));
            ASSERT_IF_FAILED(hr);
        }
    }

    /**
     * @brief 完了したフェンスのインデックスを回収する
     */
    void GarbageCollect() {
        if (!dxCommon_ || !dxCommon_->GetFence() || pendingFree_.empty()) {
            return;
        }
        uint64_t completed = dxCommon_->GetFence()->GetCompletedValue();
        auto it = std::remove_if(pendingFree_.begin(), pendingFree_.end(), [this, completed](const PendingFree& p) {
            if (p.fence <= completed) {
                freeIndices_.push_back(p.index);
                return true;
            }
            return false;
        });
        pendingFree_.erase(it, pendingFree_.end());
    }

    /**
     * @brief 定数バッファの1ブロックを割り当てる
     * @return 割り当てられたインデックス
     */
    uint32_t Allocate() {
        GarbageCollect();
        if (!freeIndices_.empty()) {
            uint32_t index = freeIndices_.back();
            freeIndices_.pop_back();
            inUse_[index] = true;
            return index;
        }
        IRUFEMI_ASSERT(nextIndex_ < capacity_ && "DynamicConstantBuffer capacity exceeded!");
        uint32_t index = nextIndex_++;
        inUse_[index] = true;
        return index;
    }

    /**
     * @brief 使用終わったインデックスを返却する
     * @param index 返却するインデックス
     */
    void Free(uint32_t index) {
        if (index >= capacity_) {
            return;
        }
        IRUFEMI_ASSERT(inUse_[index] && "DynamicConstantBuffer double-free detected!");
        inUse_[index] = false;

        if (dxCommon_ && dxCommon_->GetFence()) {
            // GPU の実行完了を待ってから再利用する（インフライトデータ破壊防止）
            uint64_t safeFence = dxCommon_->GetCurrentFrameFenceValue();
            pendingFree_.push_back({safeFence, index});
        } else {
            freeIndices_.push_back(index);
        }
    }

    /**
     * @brief データを更新する
     * @param index 対象のインデックス
     * @param data 書き込むデータ
     * @param frameIndex 対象のフレームバッファ
     */
    void Update(uint32_t index, const T& data, uint32_t frameIndex) {
        if (index >= capacity_ || frameIndex >= kMaxFramesInFlight) {
            IRUFEMI_ASSERT(false && "DynamicConstantBuffer::Update: Index out of bounds or buffer not allocated");
            return;
        }
        T* ptr = reinterpret_cast<T*>(mappedRawData_[frameIndex] + index * elementAlignedSize_);
        *ptr = data;
    }

    /**
     * @brief GPU仮想アドレスを取得する
     * @param index 対象のインデックス
     * @param frameIndex 対象のフレームバッファ
     * @return D3D12_GPU_VIRTUAL_ADDRESS
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(uint32_t index, uint32_t frameIndex) const {
        if (index >= capacity_ || frameIndex >= kMaxFramesInFlight || !resources_[frameIndex]) {
            return 0;
        }
        return resources_[frameIndex]->GetGPUVirtualAddress() + index * elementAlignedSize_;
    }

private:
    /**
     * @brief Unmap を実行する。
     */
    void Unmap() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (resources_[i] && mappedRawData_[i]) {
                resources_[i]->Unmap(0, nullptr);
                mappedRawData_[i] = nullptr;
            }
        }
    }

private:
    struct PendingFree {
        uint64_t fence = 0;
        uint32_t index = 0;
    };

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> resources_;
    std::array<uint8_t*, kMaxFramesInFlight> mappedRawData_ = {nullptr};
    std::vector<uint32_t> freeIndices_;
    std::vector<PendingFree> pendingFree_;
    std::vector<bool> inUse_;
    uint32_t nextIndex_ = 0;
    uint32_t capacity_ = 0;
    size_t elementAlignedSize_ = 0;
    DirectXCommon* dxCommon_ = nullptr;
};
