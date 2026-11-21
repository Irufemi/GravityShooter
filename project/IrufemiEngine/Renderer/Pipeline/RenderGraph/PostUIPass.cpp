#include "Renderer/Pipeline/RenderGraph/PostUIPass.h"
#include "Core/Math/Math.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Data/RenderContext.h"

using namespace Irufemi;

void PostUIPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    if (rc.isEditorMode) {
        if (rc.engine && rc.engine->GetMainRenderTexture()) {
            builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    } else {
        if (rc.outputResource) {
            builder.RequireState(rc.outputResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }
}

void PostUIPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }

    auto ppMgr = engine->GetPostProcessManager();
    if (!ppMgr) {
        return;
    }

    const auto& activeModes = ppMgr->GetActiveModes(PostProcessManager::Layer::PostUI);
    if (activeModes.empty()) {
        return;
    }

    auto* dxCommon = drawManager->GetDxCommon();
    if (!dxCommon) {
        return;
    }
    auto* cmdList = dxCommon->GetCommandList();

    // 画面遷移フェード（暗転）の描画
    for (auto mode : activeModes) {
        if (mode == PostProcessManager::Mode::Fade) {
            const auto& fade = ppMgr->GetFadeParams();
            if (fade.intensity <= 0.0001f) {
                continue;
            }

            // 定数バッファが未作成なら確保
            if (!fadeConstantBuffer_) {
                const uint32_t bufferSize = (sizeof(FadeConstantBuffer) + 255) & ~255;
                fadeConstantBuffer_ = dxCommon->CreateBufferResource(bufferSize);
                if (fadeConstantBuffer_) {
                    fadeConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedFadeBuffer_));
                }
            }

            if (!mappedFadeBuffer_) {
                continue;
            }

            // フェードパラメータの転送
            mappedFadeBuffer_->color = fade.color;
            mappedFadeBuffer_->intensity = fade.intensity;

            // ターゲットおよびビューポートの設定
            cmdList->OMSetRenderTargets(1, &rc.outputRTV, false, nullptr);
            cmdList->RSSetViewports(1, &rc.viewport);
            cmdList->RSSetScissorRects(1, &rc.scissorRect);

            // アルファブレンドでオーバーレイ描画
            engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
            engine->SetDepthWrite(PSOManager::DepthWrite::Disable);
            engine->SetCull(PSOManager::CullMode::None);
            engine->ApplyPSO("FadeOverlay");

            cmdList->SetGraphicsRootSignature(dxCommon->GetRootSignature());
            cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Material),
                                                      fadeConstantBuffer_->GetGPUVirtualAddress());

            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList->IASetVertexBuffers(0, 0, nullptr);
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    }
}
