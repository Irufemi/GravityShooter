#include "Renderer/Pipeline/RenderGraph/TopMostPass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Data/RenderContext.h"

void TopMostPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    if (rc.outputResource) {
        builder.RequireState(rc.outputResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
}

void TopMostPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }
    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();

    // 深度バッファは無効化(TopMostのため)、出力先RTVとビューポート・シザーを適用
    cmdList->OMSetRenderTargets(1, &rc.outputRTV, false, nullptr);
    cmdList->RSSetViewports(1, &rc.viewport);
    cmdList->RSSetScissorRects(1, &rc.scissorRect);

    // キューの描画関数を定義
    auto DrawWithPSO = [&](const auto& queue, auto drawFunc, const char* psoName) {
        if (queue.empty()) {
            return;
        }

        Irufemi::BlendMode currentBlend = Irufemi::BlendMode::kBlendModeNormal;
        PSOManager::DepthWrite currentDepth = PSOManager::DepthWrite::Enable;
        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        bool psoApplied = false;
        bool first = true;

        for (const auto& p : queue) {
            bool stateChanged =
                first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull;

            if (stateChanged || !psoApplied) {
                engine->SetBlend(p.blendMode);
                engine->SetDepthWrite(p.depthWrite);
                engine->SetCull(p.cullMode);
                engine->ApplyPSO(psoName);

                currentBlend = p.blendMode;
                currentDepth = p.depthWrite;
                currentCull = p.cullMode;
                psoApplied = true;
                first = false;
            }
            drawFunc(p);
        }
    };

    // 最前面スプライト
    DrawWithPSO(
        drawManager->GetTopMostSpriteQueue(), [&](const auto& p) { drawManager->DrawSprite(p); },
        "SpriteForBackBuffer");

    // 最前面スプライトバッチ
    DrawWithPSO(
        drawManager->GetTopMostSpriteBatchQueue(), [&](const auto& p) { drawManager->DrawTopMostSpriteBatch(p); },
        "SpriteBatchForBackBuffer");

    // 最前面テキスト
    DrawWithPSO(
        drawManager->GetTopMostTextQueue(), [&](const auto& p) { drawManager->DrawText(p); }, "TextForBackBuffer");
}
