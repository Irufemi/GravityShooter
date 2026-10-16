#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"
#include "Renderer/PostProcess/Effects/PostProcessEffectUtils.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/RootSignatureConfig.h"

namespace Irufemi {

void CombinedPostEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    // 1. パラメータの詰め替えとGPU同期
    manager->combinedParams_.effectCount = static_cast<int32_t>(batch_.size());
    for (size_t i = 0; i < batch_.size(); ++i) {
        manager->combinedParams_.effects[i] = static_cast<int32_t>(batch_[i]);
    }
    if (manager->mappedCombined_) {
        manager->mappedCombined_[manager->combinedBufferOffset_] = manager->combinedParams_;
    }

    // 2. レンダーターゲットの設定とクリア
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);

    // 3. パイプライン・シグネチャ設定
    commandList->SetPipelineState(writeToScreen ? manager->finalCombinedPSO_.Get()
                                                : manager->combinedPSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 4. Viewport と Scissor の設定
    D3D12_VIEWPORT fullViewport{};
    D3D12_RECT fullScissorRect{};
    SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && context.isBackBufferTarget,
                        writeToScreen ? context.targetViewport : nullptr,
                        writeToScreen ? context.targetScissor : nullptr,
                        fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);

    // 5. バインドレステクスチャインデックスの設定
    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;

    uint32_t extraIdx = manager->depthSrvIndex_;
    for (size_t i = 0; i < batch_.size(); ++i) {
        if (PostProcessManager::UsesDepthBuffer(batch_[i])) {
            extraIdx = manager->depthSrvIndex_;
        } else if (batch_[i] == PostProcessManager::Mode::Dissolve) {
            int noiseIdx = (manager->dissolveParams_.noiseType <= 0) ? 0 : 1;
            extraIdx = manager->dissolveNoiseIndex_[noiseIdx];
        }
    }
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = extraIdx;
    manager->mappedBindless_[manager->bindlessBufferOffset_].maskTextureIndex =
        manager->dxCommon_->GetEngine()->GetEffectMaskTexture()->GetSrvIndex();

    // 6. 定数バッファのバインド
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                   manager->combinedCB_->GetGPUVirtualAddress() +
                                                       manager->combinedBufferOffset_ *
                                                           sizeof(PostProcessManager::CombinedParams));
    manager->TryIncrementCombinedOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::CustomEffectParams,
                                                   manager->customEffectParamsCB_->GetGPUVirtualAddress());

    // 7. 描画
    commandList->DrawInstanced(3, 1, 0, 0);
}

void CombinedPostEffect::RenderFinalCopyPass(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;

    manager->combinedParams_.effectCount = 0;
    if (manager->mappedCombined_) {
        manager->mappedCombined_[manager->combinedBufferOffset_] = manager->combinedParams_;
    }

    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);

    commandList->SetPipelineState(manager->finalCombinedPSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VIEWPORT fullViewport{};
    D3D12_RECT fullScissorRect{};
    SetupOutputViewport(manager->engine_, manager->dxCommon_, context.isBackBufferTarget,
                        context.targetViewport, context.targetScissor,
                        fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = manager->depthSrvIndex_;
    manager->mappedBindless_[manager->bindlessBufferOffset_].maskTextureIndex =
        manager->dxCommon_->GetEngine()->GetEffectMaskTexture()->GetSrvIndex();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   manager->bindlessCB_->GetGPUVirtualAddress() +
                                                       manager->bindlessBufferOffset_ *
                                                           sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                   manager->combinedCB_->GetGPUVirtualAddress() +
                                                       manager->combinedBufferOffset_ *
                                                           sizeof(PostProcessManager::CombinedParams));
    manager->TryIncrementCombinedOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::CustomEffectParams,
                                                   manager->customEffectParamsCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace Irufemi
