#include "Renderer/Pipeline/RenderGraph/PostProcessCopyPass.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RenderTexture.h"

PostProcessCopyPass::PostProcessCopyPass(const std::string& targetName, TransientResourceHandle dstHandle)
    : targetName_(targetName), dstHandle_(dstHandle) {}

void PostProcessCopyPass::Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
    auto* engine = rc.engine;
    if (!engine) {
        return;
    }
    auto mainRenderTex = engine->GetMainRenderTexture();
    if (!mainRenderTex || !mainRenderTex->GetResource()) {
        return;
    }

    // コピー元として COPY_SOURCE を要求（RenderGraph が自動でバリアを発行）
    builder.RequireState(mainRenderTex->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);

    if (dstHandle_ == kInvalidHandle) {
        D3D12_RESOURCE_DESC desc = mainRenderTex->GetResource()->GetDesc();
        dstHandle_ = builder.GetOrCreateTransientResource(targetName_, desc);
    }

    // コピー先として COPY_DEST を要求（RenderGraph が自動でバリアを発行）
    builder.RequireTransientState(dstHandle_, D3D12_RESOURCE_STATE_COPY_DEST);
}

void PostProcessCopyPass::Execute(const Irufemi::RenderContext& rc) {
    auto* drawManager = rc.drawManager;
    auto* engine = rc.engine;
    if (!drawManager || !engine || dstHandle_ == kInvalidHandle) {
        return;
    }

    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();
    auto* renderGraph = drawManager->GetRenderGraph();
    auto* mainRenderTex = engine->GetMainRenderTexture();
    auto* dstTex = renderGraph->GetTransientRenderTexture(dstHandle_);

    if (mainRenderTex && dstTex) {
        // バリアは RenderGraph によって直前に自動発行済みのため、純粋なコピーのみ実行
        cmdList->CopyResource(dstTex->GetResource(), mainRenderTex->GetResource());
    }
}
