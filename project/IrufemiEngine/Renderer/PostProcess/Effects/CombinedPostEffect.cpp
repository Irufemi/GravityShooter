#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"
#include "Renderer/PostProcess/Effects/PostProcessEffectUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/RootSignatureConfig.h"

namespace Irufemi {

void CombinedPostEffect::Render(const PostProcessRenderContext& context) {
    RenderWithBatch(context, batch_);
}

void CombinedPostEffect::RenderWithBatch(const PostProcessRenderContext& context,
                                         const std::vector<PostProcessManager::Mode>& batch) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    // 1. パラメータの詰め替えとGPU同期
    manager->combinedParams_.effectCount = static_cast<int32_t>(batch.size());
    for (size_t i = 0; i < batch.size(); ++i) {
        manager->combinedParams_.effects[i] = static_cast<int32_t>(batch[i]);
    }
    if (manager->mappedCombined_) {
        manager->mappedCombined_[manager->combinedBufferOffset_] = manager->combinedParams_;
    }

    // 2. レンダーターゲットの設定とクリア
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);

    // 3. パイプライン・シグネチャ設定
    commandList->SetPipelineState(writeToScreen ? manager->finalCombinedPSO_.Get() : manager->combinedPSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 4. Viewport と Scissor の設定
    D3D12_VIEWPORT fullViewport{};
    D3D12_RECT fullScissorRect{};
    SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && context.isBackBufferTarget,
                        writeToScreen ? context.targetViewport : nullptr,
                        writeToScreen ? context.targetScissor : nullptr, fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);

    // 5. バインドレステクスチャインデックスの設定
    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;

    uint32_t extraIdx = manager->depthSrvIndex_;
    for (size_t i = 0; i < batch.size(); ++i) {
        if (PostProcessManager::UsesDepthBuffer(batch[i])) {
            extraIdx = manager->depthSrvIndex_;
        } else if (batch[i] == PostProcessManager::Mode::Dissolve) {
            int noiseIdx = (manager->dissolveParams_.noiseType <= 0) ? 0 : 1;
            extraIdx = manager->dissolveNoiseIndex_[noiseIdx];
        }
    }
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = extraIdx;
    manager->mappedBindless_[manager->bindlessBufferOffset_].maskTextureIndex =
        manager->dxCommon_->GetEngine()->GetEffectMaskTexture()->GetSrvIndex();

    // 6. 定数バッファのバインド
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::Material, manager->combinedCB_->GetGPUVirtualAddress() +
                                      manager->combinedBufferOffset_ * sizeof(PostProcessManager::CombinedParams));
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
    SetupOutputViewport(manager->engine_, manager->dxCommon_, context.isBackBufferTarget, context.targetViewport,
                        context.targetScissor, fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = manager->depthSrvIndex_;
    manager->mappedBindless_[manager->bindlessBufferOffset_].maskTextureIndex =
        manager->dxCommon_->GetEngine()->GetEffectMaskTexture()->GetSrvIndex();

    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::Material, manager->combinedCB_->GetGPUVirtualAddress() +
                                      manager->combinedBufferOffset_ * sizeof(PostProcessManager::CombinedParams));
    manager->TryIncrementCombinedOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::CustomEffectParams,
                                                   manager->customEffectParamsCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}

void CombinedPostEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                        TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                        bool writeToScreen) {
    PopulatePasses(renderGraph, manager, inputHandle, outputHandle, writeToScreen, batch_, false);
}

void CombinedPostEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                        TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                        bool writeToScreen, const std::vector<PostProcessManager::Mode>& batch,
                                        bool useMask) {
    if (!renderGraph || !manager) {
        return;
    }

    // --- 一線級エンジンの設計: 出力先端末の決定ロジックを一元化 (Single Source of Truth) ---
    // バックバッファへの直接出力は、「画面書き出し要求」かつ「エディタ外」、かつ「PostUIレイヤー(!useMask)」のときのみ
    const bool isBackBufferTarget = writeToScreen && !useMask;

    renderGraph->AddPass(
        "PostProcess_Combined",
        [inputHandle, outputHandle, isBackBufferTarget](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            if (outputHandle != kInvalidHandle) {
                builder.RequireTransientState(outputHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            } else if (isBackBufferTarget && !rc.isEditorMode) {
                if (rc.outputResource) {
                    builder.RequireState(rc.outputResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
                }
            } else if (rc.engine && rc.engine->GetMainRenderTexture()) {
                builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(),
                                     D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
            if (rc.engine) {
                if (auto tex = rc.engine->GetEffectMaskTexture()) {
                    builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }
                if (auto tex = rc.engine->GetNormalTexture()) {
                    builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }
                if (auto tex = rc.engine->GetMaterialTexture()) {
                    builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }
                if (auto tex = rc.engine->GetVelocityTexture()) {
                    builder.RequireState(tex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }
            }
            if (rc.drawManager && rc.drawManager->GetDxCommon()) {
                builder.RequireState(rc.drawManager->GetDxCommon()->GetDepthStencilResource(),
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        },
        [this, manager, inputHandle, outputHandle, isBackBufferTarget, batch, useMask,
         writeToScreen](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);

            if (rc.drawManager && rc.drawManager->GetPerFrameData()) {
                manager->GetOutlineParams().projectionInverse =
                    Irufemi::Math::Inverse(rc.drawManager->GetPerFrameData()->camera.projection);
            }

            // 出力先RTVハンドルの取得 (Setup と 100% 同一の判定ロジック)
            bool isBackBuffer = isBackBufferTarget && !rc.isEditorMode;
            D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
            if (outputHandle != kInvalidHandle) {
                auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(outputHandle);
                if (!outTex) {
                    return;
                }
                targetRtv = outTex->GetRtvHandle();
            } else if (isBackBuffer) {
                targetRtv = rc.outputRTV;
            } else {
                targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
            }

            const D3D12_VIEWPORT* vp = isBackBuffer ? &rc.viewport : nullptr;
            const D3D12_RECT* sc = isBackBuffer ? &rc.scissorRect : nullptr;

            PostProcessRenderContext ctx{manager, cmdList, inputTex, targetRtv, writeToScreen, isBackBuffer, vp, sc};

            // 各レイヤーのマスク設定を反映
            manager->combinedParams_.useMask = useMask ? 1 : 0;
            RenderWithBatch(ctx, batch);
        });
}

} // namespace Irufemi
