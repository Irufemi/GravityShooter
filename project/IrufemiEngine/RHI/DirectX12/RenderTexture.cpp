#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Renderer/DrawManager.h"
#include <cassert>

RenderTexture::~RenderTexture() {
    if (dxCommon_) {
        if (dxCommon_->GetSrvPool() && srvIndex_ != DescriptorAllocator::kInvalid) {
            dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        }
        if (rtvIndex_ != DescriptorAllocator::kInvalid) {
            dxCommon_->FreeRTVIndex(rtvIndex_);
        }
        if (dxCommon_->GetSrvPool() && imGuiSrvIndex_ != DescriptorAllocator::kInvalid) {
            dxCommon_->GetSrvPool()->FreeAfterFence(imGuiSrvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        }
    }
}

void RenderTexture::Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height, DXGI_FORMAT format,
                               const Irufemi::Vector4& clearColor, DXGI_FORMAT srvFormat) {
    if (dxCommon_ && srvIndex_ != DescriptorAllocator::kInvalid) {
        dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        srvIndex_ = DescriptorAllocator::kInvalid;
    }
    if (dxCommon_ && imGuiSrvIndex_ != DescriptorAllocator::kInvalid) {
        dxCommon_->GetSrvPool()->FreeAfterFence(imGuiSrvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        imGuiSrvIndex_ = DescriptorAllocator::kInvalid;
    }

    dxCommon_ = dxCommon;
    width_ = width;
    height_ = height;
    format_ = format;

    // リソースの作成
    // ポストプロセス用のバッファなどはクリアカラーが動的に変わる可能性があるため、
    // 最適化されたクリアカラーを無効（nullptr）にして生成する
    resource_ = dxCommon->CreateRenderTextureResource(dxCommon->GetDevice(), width, height, format, nullptr);

    // RTVの作成
    if (rtvIndex_ == DescriptorAllocator::kInvalid) {
        rtvIndex_ = dxCommon->AllocateRTVIndex();
    }
    rtvHandle_ = dxCommon->GetRTVCPUDescriptorHandle(rtvIndex_);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dxCommon->GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    // SRVの作成
    if (srvIndex_ == DescriptorAllocator::kInvalid) {
        srvIndex_ = dxCommon->GetSrvPool()->Allocate();
    }
    srvHandleGPU_ = dxCommon->GetSrvPool()->GetGPUHandle(srvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = (srvFormat == DXGI_FORMAT_UNKNOWN) ? format : srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc,
                                                    dxCommon->GetSrvPool()->GetCPUHandle(srvIndex_));

    // ImGui用のUNORM SRVの作成
    if (imGuiSrvIndex_ == DescriptorAllocator::kInvalid) {
        imGuiSrvIndex_ = dxCommon->GetSrvPool()->Allocate();
    }
    imGuiSrvHandleGPU_ = dxCommon->GetSrvPool()->GetGPUHandle(imGuiSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC imGuiSrvDesc = srvDesc;
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        imGuiSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &imGuiSrvDesc,
                                                    dxCommon->GetSrvPool()->GetCPUHandle(imGuiSrvIndex_));

    // 初期状態はレンダーターゲットだが、念のため SRV 状態へ即座に遷移させるなどの考慮は DrawManager 側で行う
    // (最初の BeginRenderTexture で StateBefore = PIXEL_SHADER_RESOURCE と矛盾しないようにするため)
}

void RenderTexture::InitializeFromResource(DirectXCommon* dxCommon, ID3D12Resource* resource, DXGI_FORMAT format,
                                           DXGI_FORMAT srvFormat) {
    if (dxCommon_ && srvIndex_ != DescriptorAllocator::kInvalid) {
        // 以前のフレームでGPUが参照している可能性があるため、フェンス解放キューに入れる
        dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        srvIndex_ = DescriptorAllocator::kInvalid;
    }
    if (dxCommon_ && imGuiSrvIndex_ != DescriptorAllocator::kInvalid) {
        dxCommon_->GetSrvPool()->FreeAfterFence(imGuiSrvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        imGuiSrvIndex_ = DescriptorAllocator::kInvalid;
    }

    dxCommon_ = dxCommon;
    resource_ = resource;
    format_ = format;

    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    width_ = static_cast<uint32_t>(desc.Width);
    height_ = static_cast<uint32_t>(desc.Height);

    // RTVの作成
    if (rtvIndex_ == DescriptorAllocator::kInvalid) {
        rtvIndex_ = dxCommon->AllocateRTVIndex();
    }
    rtvHandle_ = dxCommon->GetRTVCPUDescriptorHandle(rtvIndex_);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dxCommon->GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    // SRVの作成
    if (srvIndex_ == DescriptorAllocator::kInvalid) {
        srvIndex_ = dxCommon->GetSrvPool()->Allocate();
    }
    srvHandleGPU_ = dxCommon->GetSrvPool()->GetGPUHandle(srvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = (srvFormat == DXGI_FORMAT_UNKNOWN) ? format : srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc,
                                                    dxCommon->GetSrvPool()->GetCPUHandle(srvIndex_));
    // ImGui用のUNORM SRVの作成
    if (imGuiSrvIndex_ == DescriptorAllocator::kInvalid) {
        imGuiSrvIndex_ = dxCommon->GetSrvPool()->Allocate();
    }
    imGuiSrvHandleGPU_ = dxCommon->GetSrvPool()->GetGPUHandle(imGuiSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC imGuiSrvDesc = srvDesc;
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        imGuiSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &imGuiSrvDesc,
                                                    dxCommon->GetSrvPool()->GetCPUHandle(imGuiSrvIndex_));
}

// Draw メソッドは DrawManager を使用するように変更済み
void RenderTexture::Draw(DrawManager* drawManager, ID3D12PipelineState* pso, D3D12_GPU_VIRTUAL_ADDRESS cbvAddress,
                         D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle) {
    if (!drawManager) {
        return;
    }
    drawManager->DrawRenderTexture(this, pso, cbvAddress, depthSrvHandle);
}
