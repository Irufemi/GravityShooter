#include "Renderer/Pipeline/RenderGraph/PostProcessPass.h"
#include "Core/Math/Math.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/PostProcessRunner.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"

void PostProcessPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!engine) {
        return;
    }
    auto ppMgr = engine->GetPostProcessManager();
    const auto& activeModes = ppMgr->GetActiveModes();
    auto mainRenderTex = engine->GetMainRenderTexture();

    // 自身に書き戻すため、最終的な出力先として RENDER_TARGET を要求する
    builder.RequireState(mainRenderTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    // G-Bufferをシェーダーリソースとして要求する
    if (auto tex = engine->GetEffectMaskTexture()) {
        builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (auto tex = engine->GetNormalTexture()) {
        builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (auto tex = engine->GetMaterialTexture()) {
        builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (auto tex = engine->GetVelocityTexture()) {
        builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    D3D12_RESOURCE_DESC desc = mainRenderTex->GetResource()->GetDesc();

    // 入力ソース退避用テクスチャ (エフェクトの有無に関わらず常に必要)
    preUiSrcHandle_ = builder.CreateTransientResource("PP_PreUISrc", desc);
    builder.RequireTransientState(preUiSrcHandle_, D3D12_RESOURCE_STATE_COPY_DEST);

    if (!activeModes.empty()) {
        bool usesDepthBuffer = false;
        for (auto mode : activeModes) {
            if (PostProcessManager::UsesDepthBuffer(mode)) {
                usesDepthBuffer = true;
                break;
            }
        }
        if (usesDepthBuffer) {
            builder.RequireState(drawManager->GetDxCommon()->GetDepthStencilResource(),
                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        // Runner および各エフェクトへ中間リソース宣言を委譲
        if (auto* runner = ppMgr->GetRunner()) {
            runner->DeclareResources(builder, desc, activeModes);
        }
    }
}

void PostProcessPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine) {
        return;
    }
    auto ppMgr = engine->GetPostProcessManager();
    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();
    auto* renderGraph = drawManager->GetRenderGraph();
    const auto& activeModes = ppMgr->GetActiveModes();

    if (!activeModes.empty()) {
        if (auto* runner = ppMgr->GetRunner()) {
            runner->ResolveResources(renderGraph, activeModes);
        }
    }

    // 深度バッファを使用するエフェクトのための逆投影行列更新
    bool needsProjectionInverse = false;
    for (auto mode : activeModes) {
        if (PostProcessManager::UsesDepthBuffer(mode)) {
            needsProjectionInverse = true;
            break;
        }
    }
    if (needsProjectionInverse) {
        if (auto* perFrameData = drawManager->GetPerFrameData()) {
            ppMgr->GetOutlineParams().projectionInverse = Irufemi::Math::Inverse(perFrameData->camera.projection);
        }
    }

    // CopyResource (mainRenderTex -> ppSrcTex)
    auto ppSrcTex = renderGraph->GetTransientRenderTexture(preUiSrcHandle_);

    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(),
                                    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cmdList->CopyResource(ppSrcTex->GetResource(), engine->GetMainRenderTexture()->GetResource());

    DirectXUtils::TransitionBarrier(cmdList, ppSrcTex->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    renderGraph->SetInitialResourceState(ppSrcTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(),
                                    D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 最終出力先は常に mainRenderTexture
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = engine->GetMainRenderTexture()->GetRtvHandle();
    ppMgr->Draw(cmdList, ppSrcTex, rtvHandle, PostProcessManager::Layer::PreUI, false);

    // 深度バッファを元の DEPTH_WRITE に戻す
    if (needsProjectionInverse) {
        DirectXUtils::TransitionBarrier(cmdList, drawManager->GetDxCommon()->GetDepthStencilResource(),
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        renderGraph->SetInitialResourceState(drawManager->GetDxCommon()->GetDepthStencilResource(),
                                             D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
}
