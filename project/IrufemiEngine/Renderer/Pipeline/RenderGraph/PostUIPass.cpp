#include "Renderer/Pipeline/RenderGraph/PostUIPass.h"
#include "Core/Math/Math.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/PostProcessRunner.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"

void PostUIPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!engine) {
        return;
    }
    auto ppMgr = engine->GetPostProcessManager();
    const auto& activeModes = ppMgr->GetActiveModes(PostProcessManager::Layer::PostUI);
    auto mainRenderTex = engine->GetMainRenderTexture();

    // 自身に書き戻す、またはバックバッファに書き込むための事前ステート
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
    postUiSrcHandle_ = builder.CreateTransientResource("PP_PostUISrc", desc);
    builder.RequireTransientState(postUiSrcHandle_, D3D12_RESOURCE_STATE_COPY_DEST);

    if (!activeModes.empty()) {
        if (auto* runner = ppMgr->GetRunner()) {
            runner->DeclareResources(builder, desc, activeModes);
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
    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();
    auto* renderGraph = drawManager->GetRenderGraph();
    const auto& activeModes = ppMgr->GetActiveModes(PostProcessManager::Layer::PostUI);

    if (!activeModes.empty()) {
        if (auto* runner = ppMgr->GetRunner()) {
            runner->ResolveResources(renderGraph, activeModes);
        }
    }

    // CopyResource (mainRenderTex -> ppSrcTex)
    auto ppSrcTex = renderGraph->GetTransientRenderTexture(postUiSrcHandle_);

    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(),
                                    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

    cmdList->CopyResource(ppSrcTex->GetResource(), engine->GetMainRenderTexture()->GetResource());

    DirectXUtils::TransitionBarrier(cmdList, ppSrcTex->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    renderGraph->SetInitialResourceState(ppSrcTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(),
                                    D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 最終出力先の決定 (RenderContextから取得)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rc.outputRTV;
    bool isBackBufferTarget = !rc.isEditorMode;

    ppMgr->Draw(cmdList, ppSrcTex, rtvHandle, PostProcessManager::Layer::PostUI, isBackBufferTarget,
                &rc.viewport, &rc.scissorRect);
}
