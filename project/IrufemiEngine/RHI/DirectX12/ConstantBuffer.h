#pragma once

#include "Core/Utility/ErrorUtility.h"
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <cassert>
#include "RHI/DirectX12/DirectXCommon.h"

namespace Irufemi {

// マルチバッファリング対応 汎用定数バッファ管理クラステンプレート
template <typename T> class ConstantBuffer {
public:
    ConstantBuffer() = default;
    ~ConstantBuffer() {
        Unmap();
        if (dxCommon_) {
            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                if (resources_[i]) {
                    dxCommon_->ReleaseAfterFence(resources_[i]);
                }
            }
        }
    }

    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    ConstantBuffer(ConstantBuffer&& other) noexcept
        : resources_(std::move(other.resources_)), mappedData_(other.mappedData_), dxCommon_(other.dxCommon_) {
        other.mappedData_.fill(nullptr);
        other.dxCommon_ = nullptr;
    }

    ConstantBuffer& operator=(ConstantBuffer&& other) noexcept {
        if (this != &other) {
            Unmap();
            if (dxCommon_) {
                for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                    if (resources_[i]) {
                        dxCommon_->ReleaseAfterFence(resources_[i]);
                    }
                }
            }
            resources_ = std::move(other.resources_);
            mappedData_ = other.mappedData_;
            dxCommon_ = other.dxCommon_;
            other.mappedData_.fill(nullptr);
            other.dxCommon_ = nullptr;
        }
        return *this;
    }

    // バッファを生成しマッピングする（kMaxFramesInFlight分）
    /**
     * @brief 初期化済みかどうかを判定する
     */
    bool IsInitialized() const {
        return mappedData_[0] != nullptr;
    }

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(DirectXCommon* dxCommon) {
        IRUFEMI_ASSERT(dxCommon != nullptr);
        dxCommon_ = dxCommon;
        // 定数バッファの制約：サイズは256バイトの倍数
        size_t alignedSize = (sizeof(T) + 255) & ~255;
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            resources_[i] = dxCommon_->CreateBufferResource(alignedSize);
            HRESULT hr = resources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_[i]));
            ASSERT_IF_FAILED(hr);
        }
    }

    // frameIndex番目のバッファにデータを書き込む
    /**
     * @brief Update を実行する。
     */
    void Update(const T& data, uint32_t frameIndex) {
        if (mappedData_[frameIndex]) {
            *mappedData_[frameIndex] = data;
        }
    }

    // すべてのフレームバッファ（kMaxFramesInFlight分）に同じデータを書き込む（初期化用）
    /**
     * @brief UpdateAll を実行する。
     */
    void UpdateAll(const T& data) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (mappedData_[i]) {
                *mappedData_[i] = data;
            }
        }
    }

    // マッピングされたメモリへの直接アクセス
    T* operator[](uint32_t frameIndex) {
        return mappedData_[frameIndex];
    }
    const T* operator[](uint32_t frameIndex) const {
        return mappedData_[frameIndex];
    }

    // DirectX リソース用ゲッター
    /**
     * @brief Resource を取得する。
     * @return 取得された Resource
     */
    ID3D12Resource* GetResource(uint32_t frameIndex) const {
        return resources_[frameIndex].Get();
    }

    /**
     * @brief GPUVirtualAddress を取得する。
     * @return 取得された GPUVirtualAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(uint32_t frameIndex) const {
        return resources_[frameIndex]->GetGPUVirtualAddress();
    }

private:
    /**
     * @brief Unmap を実行する。
     */
    void Unmap() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (resources_[i] && mappedData_[i]) {
                resources_[i]->Unmap(0, nullptr);
                mappedData_[i] = nullptr;
            }
        }
    }

private:
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> resources_;
    std::array<T*, kMaxFramesInFlight> mappedData_ = {nullptr};
    DirectXCommon* dxCommon_ = nullptr;
};

} // namespace Irufemi
