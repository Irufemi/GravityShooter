#include "Renderer/Pipeline/RenderGraph/FogPass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/Data/FogParams.h"

void FogPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;

    if (drawManager) {
        if (auto dx = drawManager->GetDxCommon()) {
            builder.RequireState(dx->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }

    if (engine) {
        if (auto tex = engine->GetMainRenderTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }
}

void FogPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }

    const auto& fogParams = drawManager->GetFogParams();
    if (fogParams.enabled == 0) {
        return;
    }

    auto cmdList = engine->GetCommandList();
    auto dxCommon = engine->GetDirectXCommon();
    auto depthResource = dxCommon->GetDepthStencilResource();

    // 1. 深度バッファをシェーダー読み取り状態に遷移 (DEPTH_WRITE -> DEPTH_READ | PIXEL_SHADER_RESOURCE)
    if (depthResource) {
        DirectXUtils::TransitionBarrier(cmdList, depthResource, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // 2. レンダーターゲット（MainRenderTexture 1枚のみ、DSVなし）を設定
    auto mainTex = engine->GetMainRenderTexture();
    if (!mainTex) {
        return;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mainTex->GetRtvHandle();
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    // 3. ビューポート & シザー設定 (GameResolution 基準)
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(engine->GetGameResolutionWidth());
    viewport.Height = static_cast<float>(engine->GetGameResolutionHeight());
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.right = engine->GetGameResolutionWidth();
    scissor.top = 0;
    scissor.bottom = engine->GetGameResolutionHeight();

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    // 4. Fog PSO バインド (通常アルファブレンド、深度書き込み無効、カリング無効)
    engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
    engine->SetDepthWrite(PSOManager::DepthWrite::Disable);
    engine->SetCull(PSOManager::CullMode::None);
    engine->ApplyPSO("Fog");

    // 5. RootSignature & 定数バッファバインド
    cmdList->SetGraphicsRootSignature(dxCommon->GetRootSignature());
    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon->GetSrvDescriptorHeap()};
    cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // [Bindless] 全テクスチャテーブル (Slot 2)
    cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSlot::BindlessSRV),
                                            dxCommon->GetSrvPool()->GetGPUHandle(0));

    // カメラ定数バッファ (Slot 5: b2)
    cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Camera), drawManager->GetCameraCBVAddress());

    // フォグ定数バッファ (Slot 0: b0)
    cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Material), drawManager->GetFogCBVAddress());

    // 6. 全画面三角形描画 (SV_VertexID)
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->DrawInstanced(3, 1, 0, 0);

    // 7. 深度バッファを DEPTH_WRITE に戻す (後続パスのため)
    if (depthResource) {
        DirectXUtils::TransitionBarrier(cmdList, depthResource,
                                        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
}
