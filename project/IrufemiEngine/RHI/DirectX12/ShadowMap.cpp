#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/ShadowMap.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Core/Math/Math.h"
#include <cassert>
#include <cmath>

namespace Irufemi {

namespace {
// シャドウマップのパラメータ定数
const float kLightDistance = 200.0f; // ライトの距離
const float kNearClip = 0.1f;        // ニアクリップ
const float kFarClip = 512.0f;       // ファークリップ
} // namespace

ShadowMap::~ShadowMap() {
    // SRV の解放
    if (dxCommon_ && dxCommon_->GetSrvPool()) {
        if (srvIndex_ != DescriptorPool::kInvalid) {
            dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        }
    }
    // DSV の解放
    if (dxCommon_ && dsvIndex_ != DescriptorPool::kInvalid) {
        dxCommon_->FreeDSVIndex(dsvIndex_);
    }
}

void ShadowMap::Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height) {
    IRUFEMI_ASSERT(dxCommon);

    // 既存リソース・ディスクリプタの解放（再初期化時のリーク防止）
    if (dxCommon_) {
        if (srvIndex_ != DescriptorPool::kInvalid && dxCommon_->GetSrvPool()) {
            dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
            srvIndex_ = DescriptorPool::kInvalid;
        }
        if (dsvIndex_ != DescriptorPool::kInvalid) {
            dxCommon_->FreeDSVIndex(dsvIndex_);
            dsvIndex_ = DescriptorPool::kInvalid;
        }
        if (resource_) {
            dxCommon_->ReleaseAfterFence(resource_);
            resource_.Reset();
        }
    }

    dxCommon_ = dxCommon;
    ID3D12Device* device = dxCommon_->GetDevice();

    // 1. リソース作成
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Width = width;
    resDesc.Height = height;
    resDesc.MipLevels = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // SRV と DSV で使い分けるため TYPELESS
    resDesc.SampleDesc.Count = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearVal{};
    clearVal.DepthStencil.Depth = 1.0f;
    clearVal.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr =
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 最初は読み取り可能状態で生成
                                        &clearVal, IID_PPV_ARGS(resource_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    // 2. DSV 作成
    dsvIndex_ = dxCommon_->AllocateDSVIndex();
    dsvHandleCPU_ = dxCommon_->GetDSVCPUDescriptorHandle(dsvIndex_);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(resource_.Get(), &dsvDesc, dsvHandleCPU_);

    // 3. SRV 作成
    srvIndex_ = dxCommon_->GetSrvPool()->Allocate();
    srvHandleCPU_ = dxCommon_->GetSrvPool()->GetCPUHandle(srvIndex_);
    srvHandleGPU_ = dxCommon_->GetSrvPool()->GetGPUHandle(srvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);

    // 4. ビューポート・シザーレクトの設定
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = static_cast<LONG>(width);
    scissorRect_.bottom = static_cast<LONG>(height);
}

void ShadowMap::Begin(ID3D12GraphicsCommandList* commandList) {
    // バリアは RenderGraph 側で自動発行されるためここでは行わない

    // 2. Clear
    Clear(commandList);

    // 3. Viewport & Scissor
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);

    // 4. Set Render Target (Depth only)
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandleCPU_);
}

void ShadowMap::End(ID3D12GraphicsCommandList* commandList) {
    // バリアは RenderGraph 側で自動発行されるためここでは行わない
}

void ShadowMap::Clear(ID3D12GraphicsCommandList* commandList) {
    commandList->ClearDepthStencilView(dsvHandleCPU_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void ShadowMap::UpdateMatrix(const Irufemi::Vector3& lightDir, const Irufemi::Vector3& targetPos, float orthoSize) {
    // 方向の正規化
    Irufemi::Vector3 lightDirNormalized = Irufemi::Math::Normalize(lightDir);

    // 方向が真上または真下の場合は Up ベクトルを変える
    Irufemi::Vector3 up = {0, 1, 0};
    if (std::abs(lightDirNormalized.y) > 0.999f) {
        up = {0, 0, 1};
    }

    // ライト位置の計算 (注視点から距離を離す)
    Irufemi::Vector3 eye =
        Irufemi::Math::Subtract(targetPos, Irufemi::Math::Multiply(kLightDistance, lightDirNormalized));

    // ビュー行列の作成 (手動計算版)
    Irufemi::Vector3 zaxis = Irufemi::Math::Normalize(Irufemi::Math::Subtract(targetPos, eye));
    Irufemi::Vector3 xaxis = Irufemi::Math::Normalize(Irufemi::Math::Cross(up, zaxis));
    Irufemi::Vector3 yaxis = Irufemi::Math::Cross(zaxis, xaxis);

    Irufemi::Matrix4x4 view{};
    view.m[0][0] = xaxis.x;
    view.m[0][1] = yaxis.x;
    view.m[0][2] = zaxis.x;
    view.m[0][3] = 0;
    view.m[1][0] = xaxis.y;
    view.m[1][1] = yaxis.y;
    view.m[1][2] = zaxis.y;
    view.m[1][3] = 0;
    view.m[2][0] = xaxis.z;
    view.m[2][1] = yaxis.z;
    view.m[2][2] = zaxis.z;
    view.m[2][3] = 0;
    view.m[3][0] = -Irufemi::Math::Dot(xaxis, eye);
    view.m[3][1] = -Irufemi::Math::Dot(yaxis, eye);
    view.m[3][2] = -Irufemi::Math::Dot(zaxis, eye);
    view.m[3][3] = 1;

    // 正投影行列の作成
    Irufemi::Matrix4x4 proj =
        Irufemi::Math::MakeOrthographicMatrix(-orthoSize, orthoSize, orthoSize, -orthoSize, kNearClip, kFarClip);

    // 合成
    viewProjection_ = Irufemi::Math::Multiply(view, proj);
}

} // namespace Irufemi
