#include "Renderer/Pipeline/RenderGraph/UIPass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Data/RenderContext.h"

using namespace Irufemi;

void UIPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
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

void UIPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }

    auto cmdList = engine->GetCommandList();

    // 出力先RTVとビューポート・シザーの設定
    // エディタモード時はMainRenderTexture、スタンドアロン実行時はバックバッファ（実機ネイティブ解像度）へ直接描画
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rc.outputRTV;
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    cmdList->RSSetViewports(1, &rc.viewport);
    cmdList->RSSetScissorRects(1, &rc.scissorRect);

    // PSO名のプレフィックス/サフィックスの決定
    // バックバッファ描画時（非エディタモード）は ForBackBuffer PSO を使用して最高精度でラスタライズ
    const char* spritePso = rc.isEditorMode ? "Sprite" : "SpriteForBackBuffer";
    const char* spriteBatchPso = rc.isEditorMode ? "SpriteBatch" : "SpriteBatchForBackBuffer";
    const char* textPso = rc.isEditorMode ? "Text" : "TextForBackBuffer";

    auto DrawWithPSO = [&](const auto& queue, auto drawFunc, const char* psoName) {
        if (queue.empty()) {
            return;
        }

        Irufemi::BlendMode currentBlend = Irufemi::BlendMode::kBlendModeNormal;
        PSOManager::DepthWrite currentDepth = PSOManager::DepthWrite::Enable;
        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        ID3D12PipelineState* currentCustomPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS currentCustomCBV = 0;
        bool psoApplied = false;
        bool first = true;

        for (const auto& p : queue) {
            bool stateChanged =
                first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull;
            bool psoChanged = (p.customPSO != currentCustomPSO);

            if (stateChanged || psoChanged || !psoApplied) {
                engine->SetBlend(p.blendMode);
                engine->SetDepthWrite(p.depthWrite);
                engine->SetCull(p.cullMode);

                if (p.customPSO) {
                    engine->GetCommandList()->SetPipelineState(p.customPSO);
                } else {
                    engine->ApplyPSO(psoName);
                }

                currentBlend = p.blendMode;
                currentDepth = p.depthWrite;
                currentCull = p.cullMode;
                currentCustomPSO = p.customPSO;
                currentCustomCBV = 0;
                psoApplied = true;
                first = false;
            }

            if (p.customCBVAddress != 0 && p.customCBVAddress != currentCustomCBV) {
                engine->BindCustomConstantBuffer(p.customCBVAddress);
                currentCustomCBV = p.customCBVAddress;
            }

            drawFunc(p);
        }
    };

    // 8. Sprites
    DrawWithPSO(drawManager->GetSpriteQueue(), [&](const auto& p) { drawManager->DrawSprite(p); }, spritePso);

    // 8.01 SpriteBatch
    DrawWithPSO(
        drawManager->GetSpriteBatchQueue(), [&](const auto& p) { drawManager->DrawSpriteBatch(p); }, spriteBatchPso);

    // 8.02 Primitive2DBatch
    DrawWithPSO(
        drawManager->GetPrimitive2DBatchQueue(), [&](const auto& p) { drawManager->DrawPrimitive2DBatch(p); },
        spriteBatchPso);

    // 8.1 Texts (実機ネイティブ解像度で最高精度のMSDFラスタライズ)
    DrawWithPSO(drawManager->GetTextQueue(), [&](const auto& p) { drawManager->DrawText(p); }, textPso);

    // 8.5 UI 3D Objects (Always drawn on top of Sprites)
    DrawWithPSO(drawManager->GetUI3DQueue(), [&](const auto& p) { drawManager->DrawStandard3D(p); }, "Object3D");

    // 9. Post Custom Draws / Stage AfterUI (全UI描画完了後の最前面カスタム描画)
    const auto& afterUIQueue = drawManager->GetCustomPassQueue(Irufemi::RenderStage::AfterUI);
    for (auto& func : afterUIQueue) {
        func();
    }
}
