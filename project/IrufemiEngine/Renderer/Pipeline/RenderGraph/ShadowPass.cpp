#include "Renderer/Pipeline/RenderGraph/ShadowPass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/ShadowMap.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Data/RenderContext.h"

using namespace Irufemi;

void ShadowPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    if (rc.drawManager) {
        if (auto shadowMap = rc.drawManager->GetShadowMap()) {
            builder.RequireState(shadowMap->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }
}

void ShadowPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }
    drawManager->BeginShadowPass();

    auto DrawShadowsWithPSO = [&](const auto& queue, const std::string& psoName, auto drawFunc) {
        if (queue.empty()) {
            return;
        }

        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        bool first = true;

        for (const auto& p : queue) {
            if (!p.castShadows) {
                continue;
            }

            if (first || p.cullMode != currentCull) {
                engine->SetCull(p.cullMode);
                engine->ApplyPSO(psoName);
                currentCull = p.cullMode;
                first = false;
            }
            drawFunc(p);
        }
    };

    DrawShadowsWithPSO(drawManager->GetStandard3DQueue(), "Object3D",
                       [&](const auto& p) { drawManager->DrawStandard3D(p); });
    DrawShadowsWithPSO(drawManager->GetPrimitiveBatchQueue(), "Batch",
                       [&](const auto& p) { drawManager->DrawPrimitiveBatch(p); });
    DrawShadowsWithPSO(drawManager->GetModelBatchQueue(), "Batch",
                       [&](const auto& p) { drawManager->DrawModelBatch(p); });

    drawManager->EndShadowPass();
}
