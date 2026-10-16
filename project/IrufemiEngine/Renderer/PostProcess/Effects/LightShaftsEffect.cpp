#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include <algorithm>

namespace Irufemi {

void LightShaftsEffect::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) {
    D3D12_RESOURCE_DESC lsDesc = baseDesc;
    lsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    lsDesc.Width = (std::max<UINT64>)(1, lsDesc.Width / 2);
    lsDesc.Height = (std::max<UINT>)(1, lsDesc.Height / 2);
    lsExtractHandle_ = builder.CreateTransientResource("LS_Extract", lsDesc);
    lsBlurHandle_ = builder.CreateTransientResource("LS_Blur", lsDesc);
    builder.RequireTransientState(lsExtractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    builder.RequireTransientState(lsBlurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LightShaftsEffect::ResolveResources(RenderGraph* renderGraph) {
    if (renderGraph) {
        lsExtract_ = (lsExtractHandle_ != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(lsExtractHandle_) : nullptr;
        lsBlur_ = (lsBlurHandle_ != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(lsBlurHandle_) : nullptr;
    }
}

void LightShaftsEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    RenderTexture* lsExtract = lsExtract_;
    RenderTexture* lsBlur = lsBlur_;

    if (!lsExtract || !lsBlur) {
        return;
    }

    // 1. Light Shafts Extract Pass
    DirectXUtils::TransitionBarrier(commandList, lsExtract->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE lsExtractRtv = lsExtract->GetRtvHandle();
    commandList->OMSetRenderTargets(1, &lsExtractRtv, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(lsExtractRtv, clearColor, 0, nullptr);
    commandList->SetPipelineState(manager->lsExtractPSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = manager->depthSrvIndex_;
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                   manager->lightShaftsCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
    DirectXUtils::TransitionBarrier(commandList, lsExtract->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Radial Blur Pass
    DirectXUtils::TransitionBarrier(commandList, lsBlur->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE lsBlurRtv = lsBlur->GetRtvHandle();
    commandList->OMSetRenderTargets(1, &lsBlurRtv, false, nullptr);
    commandList->ClearRenderTargetView(lsBlurRtv, clearColor, 0, nullptr);
    commandList->SetPipelineState(manager->lsRadialBlurPSO_.Get());

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex = lsExtract->GetSrvIndex();
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = 0;
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                   manager->lightShaftsCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
    DirectXUtils::TransitionBarrier(commandList, lsBlur->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 3. Combine Pass
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
    commandList->SetPipelineState(writeToScreen ? manager->finalLsCombinePSO_.Get()
                                                : manager->lsCombinePSO_.Get());

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = lsBlur->GetSrvIndex();
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace Irufemi
