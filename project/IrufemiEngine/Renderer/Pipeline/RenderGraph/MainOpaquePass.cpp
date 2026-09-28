#include "Renderer/Pipeline/RenderGraph/MainOpaquePass.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/ShadowMap.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Data/RenderContext.h"

void MainOpaquePass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;

    if (drawManager) {
        if (auto shadowMap = drawManager->GetShadowMap()) {
            builder.RequireState(shadowMap->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (auto dx = drawManager->GetDxCommon()) {
            builder.RequireState(dx->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }

    if (engine) {
        // G-Bufferをレンダーターゲットとして要求
        if (auto tex = engine->GetMainRenderTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (auto tex = engine->GetEffectMaskTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (auto tex = engine->GetNormalTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (auto tex = engine->GetMaterialTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (auto tex = engine->GetVelocityTexture()) {
            builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }
}

void MainOpaquePass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }

    // 1. Skybox / Skydome
    const auto& skyboxQueue = drawManager->GetSkyboxQueue();
    if (!skyboxQueue.empty()) {
        engine->ApplyPSO("Skybox");
        for (const auto& p : skyboxQueue) {
            drawManager->DrawSkybox(p);
        }
    }

    const auto& skydomeQueue = drawManager->GetSkydomeQueue();
    if (!skydomeQueue.empty()) {
        engine->ApplyPSO("Skydome");
        engine->SetBlend(Irufemi::BlendMode::kBlendModeNone);
        engine->SetDepthWrite(PSOManager::DepthWrite::Disable);
        engine->SetCull(PSOManager::CullMode::None);
        for (const auto& p : skydomeQueue) {
            drawManager->DrawSkydome(p);
        }
    }

    // Helper lambda to apply PSO efficiently
    auto DrawWithPSO = [&](const auto& queue, auto applyPSOFunc, auto drawFunc) {
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
                    drawManager->BindPSO(p.customPSO);
                } else {
                    applyPSOFunc();
                }

                currentBlend = p.blendMode;
                currentDepth = p.depthWrite;
                currentCull = p.cullMode;
                currentCustomPSO = p.customPSO;
                currentCustomCBV = 0; // Force re-bind on state/PSO change
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

    // 2. Standard 3D (Opaque and Alpha blend)
    DrawWithPSO(
        drawManager->GetStandard3DQueue(), [&]() { engine->ApplyPSO("Object3D"); },
        [&](const auto& p) { drawManager->DrawStandard3D(p); });

    // PrimitiveBatch
    DrawWithPSO(
        drawManager->GetPrimitiveBatchQueue(), [&]() { engine->ApplyPSO("Batch"); },
        [&](const auto& p) { drawManager->DrawPrimitiveBatch(p); });

    // ModelBatch
    DrawWithPSO(
        drawManager->GetModelBatchQueue(), [&]() { engine->ApplyPSO("Batch"); },
        [&](const auto& p) { drawManager->DrawModelBatch(p); });
}
