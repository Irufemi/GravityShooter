#include "Renderer/Pipeline/RenderGraph/UIPass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Data/RenderContext.h"

void UIPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    if (rc.engine && rc.engine->GetMainRenderTexture()) {
        builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
}

void UIPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }
    if (auto scm = engine->GetScreenCaptureManager()) {
        scm->OnPreUIDraw(engine->GetCommandList(), engine->GetMainRenderTexture());
    }

    auto cmdList = engine->GetCommandList();

    // UI の描画先を設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = engine->GetMainRenderTexture()->GetRtvHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = drawManager->GetDxCommon()->GetDSVCPUDescriptorHandle(0);
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    // Viewport と Scissor を明示的に設定 (PreUIパスなどから引き継いだ意図しないViewportで描画されるのを防ぐため)
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    viewport.Width = static_cast<float>(engine->GetGameResolutionWidth());
    viewport.Height = static_cast<float>(engine->GetGameResolutionHeight());
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    scissor.left = 0;
    scissor.right = engine->GetGameResolutionWidth();
    scissor.top = 0;
    scissor.bottom = engine->GetGameResolutionHeight();
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

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
                engine->BindLightningParams(p.customCBVAddress);
                currentCustomCBV = p.customCBVAddress;
            }

            drawFunc(p);
        }
    };

    // 8. Sprites
    DrawWithPSO(drawManager->GetSpriteQueue(), [&](const auto& p) { drawManager->DrawSprite(p); }, "Sprite");

    // 8.01 SpriteBatch
    DrawWithPSO(
        drawManager->GetSpriteBatchQueue(), [&](const auto& p) { drawManager->DrawSpriteBatch(p); }, "SpriteBatch");

    // 8.02 Primitive2DBatch
    DrawWithPSO(
        drawManager->GetPrimitive2DBatchQueue(), [&](const auto& p) { drawManager->DrawPrimitive2DBatch(p); },
        "SpriteBatch");

    // 8.1 Texts
    DrawWithPSO(drawManager->GetTextQueue(), [&](const auto& p) { drawManager->DrawText(p); }, "Text");

    // 8.5 UI 3D Objects (Always drawn on top of Sprites)
    DrawWithPSO(drawManager->GetUI3DQueue(), [&](const auto& p) { drawManager->DrawStandard3D(p); }, "Object3D");

    // 9. Post Custom Draws / Stage AfterUI (全UI描画完了後の最前面カスタム描画)
    const auto& afterUIQueue = drawManager->GetCustomPassQueue(Irufemi::RenderStage::AfterUI);
    for (auto& func : afterUIQueue) {
        func();
    }
}
