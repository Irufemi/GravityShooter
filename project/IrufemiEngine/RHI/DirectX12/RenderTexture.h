#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <string>
#include "Core/Math/Vector4.h"
#include "RHI/DirectX12/DescriptorAllocator.h"

class DirectXCommon;
class DrawManager;
class Camera;

class RenderTexture {
public:
    RenderTexture() = default;
    ~RenderTexture();
    // ---------------------------------------------------------
    // Initialize
    // srvFormat: 指定がない(DXGI_FORMAT_UNKNOWN)場合は format と同じ形式で SRV を作成する
    // ---------------------------------------------------------
    /**
     * @brief Initialize を実行する。
     */
    void Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height, DXGI_FORMAT format,
                    const Irufemi::Vector4& clearColor, DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN);

    /**
     * @brief InitializeFromResource を実行する。
     */
    void InitializeFromResource(DirectXCommon* dxCommon, ID3D12Resource* resource, DXGI_FORMAT format,
                                DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN);

    // スプライトの初期化 (廃止予定だが、互換性のために残すか?)
    // 今回は全画面コピーに移行するため、基本的には不要
    // void InitializeSprite(Camera* camera);

    // 現在のターゲットに対して自身を描画
    /**
     * @brief Draw を実行する。
     */
    void Draw(DrawManager* drawManager, ID3D12PipelineState* pso = nullptr, D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0,
              D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = {0});

    /**
     * @brief Resource を取得する。
     * @return 取得された Resource
     */
    ID3D12Resource* GetResource() const {
        return resource_.Get();
    }
    /**
     * @brief RtvHandle を取得する。
     * @return 取得された RtvHandle
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const {
        return rtvHandle_;
    }
    /**
     * @brief SrvHandleGPU を取得する。
     * @return 取得された SrvHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const {
        return srvHandleGPU_;
    }
    /**
     * @brief ImGuiSrvHandleGPU を取得する。
     * @return 取得された ImGuiSrvHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiSrvHandleGPU() const {
        return imGuiSrvHandleGPU_.ptr ? imGuiSrvHandleGPU_ : srvHandleGPU_;
    }

    /**
     * @brief SrvIndex を取得する。
     * @return 取得された SrvIndex
     */
    uint32_t GetSrvIndex() const {
        return srvIndex_;
    }
    /**
     * @brief Width を取得する。
     * @return 取得された Width
     */
    uint32_t GetWidth() const {
        return width_;
    }
    /**
     * @brief Height を取得する。
     * @return 取得された Height
     */
    uint32_t GetHeight() const {
        return height_;
    }
    /**
     * @brief Format を取得する。
     * @return 取得された Format
     */
    DXGI_FORMAT GetFormat() const {
        return format_;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE imGuiSrvHandleGPU_{};

    uint32_t rtvIndex_ = DescriptorAllocator::kInvalid;
    uint32_t srvIndex_ = DescriptorAllocator::kInvalid;
    uint32_t imGuiSrvIndex_ = DescriptorAllocator::kInvalid;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    DirectXCommon* dxCommon_ = nullptr;
};
