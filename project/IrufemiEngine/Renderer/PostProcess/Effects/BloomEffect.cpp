#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/RootSignatureConfig.h"

namespace Irufemi {

void BloomEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    const auto& workspace = *(context.workspace);
    bool writeToScreen = context.writeToScreen;

    RenderTexture* bloomExtract = workspace.bloomExtract;
    RenderTexture* blurH = workspace.bloomBlur;
    RenderTexture* blurV = workspace.bloomExtract;

    if (!bloomExtract || !blurH || !blurV) {
        return;
    }

    // 1. High Luminance Extract Pass
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, currentSource,
                            bloomExtract->GetRtvHandle(), false, manager->bloomExtractPSO_.Get());
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Horizontal Blur Pass
    manager->bloomParams_.direction = {1.0f, 0.0f};
    if (manager->mappedBloom_) {
        *(manager->mappedBloom_) = manager->bloomParams_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, bloomExtract,
                            blurH->GetRtvHandle(), false, manager->bloomBlurHPSO_.Get());
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 3. Vertical Blur Pass
    manager->bloomParams_.direction = {0.0f, 1.0f};
    if (manager->mappedBloom_) {
        *(manager->mappedBloom_) = manager->bloomParams_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, blurH,
                            blurV->GetRtvHandle(), false, manager->bloomBlurVPSO_.Get());
    DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 4. Combine Pass
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
    commandList->SetPipelineState(writeToScreen ? manager->finalBloomCombinePSO_.Get()
                                                : manager->bloomCombinePSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex =
        blurV ? blurV->GetSrvIndex() : 0;
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                   manager->bloomCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace Irufemi
