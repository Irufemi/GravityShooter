#include "Renderer/Pipeline/RenderGraph/PostProcessPass.h"
#include "Core/Math/Math.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/PostProcessRunner.h"
#include "RHI/DirectX12/DirectXCommon.h"
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

    // 直前の PostProcessCopyPass で複製された一時テクスチャを取得し、SRV として要求
    preUiSrcHandle_ = builder.FindTransientResource("PP_PreUISrc");
    if (preUiSrcHandle_ != kInvalidHandle) {
        builder.RequireTransientState(preUiSrcHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

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
        D3D12_RESOURCE_DESC desc = mainRenderTex->GetResource()->GetDesc();
        if (auto* runner = ppMgr->GetRunner()) {
            runner->DeclareResources(builder, desc, activeModes, PostProcessManager::Layer::PreUI);
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
    const auto& activeModes = ppMgr->GetActiveModes(PostProcessManager::Layer::PreUI);

    if (!activeModes.empty()) {
        if (auto* runner = ppMgr->GetRunner()) {
            runner->ResolveResources(renderGraph, activeModes, PostProcessManager::Layer::PreUI);
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

    auto ppSrcTex = (preUiSrcHandle_ != kInvalidHandle)
                        ? renderGraph->GetTransientRenderTexture(preUiSrcHandle_)
                        : engine->GetMainRenderTexture();

    // 最終出力先は常に mainRenderTexture
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = engine->GetMainRenderTexture()->GetRtvHandle();
    ppMgr->Draw(cmdList, ppSrcTex, rtvHandle, PostProcessManager::Layer::PreUI, false);
}
