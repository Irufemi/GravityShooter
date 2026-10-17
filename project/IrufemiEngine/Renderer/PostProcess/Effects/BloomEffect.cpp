#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/RootSignatureConfig.h"

namespace Irufemi {

void BloomEffect::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) {
    D3D12_RESOURCE_DESC workDesc = baseDesc;
    workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    extractHandle_ = builder.CreateTransientResource("BloomExtract", workDesc);
    blurHandle_ = builder.CreateTransientResource("BloomBlur", workDesc);
    builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    builder.RequireTransientState(blurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void BloomEffect::ResolveResources(RenderGraph* renderGraph) {
    if (renderGraph) {
        extractTex_ =
            (extractHandle_ != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(extractHandle_) : nullptr;
        blurTex_ = (blurHandle_ != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(blurHandle_) : nullptr;
    }
}

void BloomEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    RenderTexture* bloomExtract = extractTex_;
    RenderTexture* blurH = blurTex_;
    RenderTexture* blurV = extractTex_;

    if (!bloomExtract || !blurH || !blurV) {
        return;
    }

    // 1. High Luminance Extract Pass
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, currentSource, bloomExtract->GetRtvHandle(),
                            false, manager->bloomExtractPSO_.Get());
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Horizontal Blur Pass
    manager->bloomParams_.direction = {1.0f, 0.0f};
    if (manager->mappedBloom_) {
        *(manager->mappedBloom_) = manager->bloomParams_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, bloomExtract, blurH->GetRtvHandle(), false,
                            manager->bloomBlurHPSO_.Get());
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 3. Vertical Blur Pass
    manager->bloomParams_.direction = {0.0f, 1.0f};
    if (manager->mappedBloom_) {
        *(manager->mappedBloom_) = manager->bloomParams_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, blurH, blurV->GetRtvHandle(), false,
                            manager->bloomBlurVPSO_.Get());
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
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = blurV ? blurV->GetSrvIndex() : 0;
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, manager->bloomCB_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}

void BloomEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                 TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                 bool writeToScreen) {
    if (!renderGraph || !manager) {
        return;
    }

    D3D12_RESOURCE_DESC desc{};
    if (auto* d = renderGraph->GetTransientDesc(inputHandle)) {
        desc = *d;
    }
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    extractHandle_ = renderGraph->GetOrCreateTransientResource("Bloom_Extract", desc);
    blurHandle_ = renderGraph->GetOrCreateTransientResource("Bloom_Blur", desc);

    // 1. High Luminance Extract Pass
    renderGraph->AddPass(
        "Bloom_Extract",
        [this, inputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, inputHandle](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            if (!inputTex || !extractTex) {
                return;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, inputTex, extractTex->GetRtvHandle(),
                                    false, manager->bloomExtractPSO_.Get());
        });

    // 2. Horizontal Blur Pass
    renderGraph->AddPass(
        "Bloom_BlurH",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(blurHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            auto* blurTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurHandle_);
            if (!extractTex || !blurTex) {
                return;
            }
            manager->bloomParams_.direction = {1.0f, 0.0f};
            if (manager->mappedBloom_) {
                *(manager->mappedBloom_) = manager->bloomParams_;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, extractTex, blurTex->GetRtvHandle(),
                                    false, manager->bloomBlurHPSO_.Get());
        });

    // 3. Vertical Blur Pass
    renderGraph->AddPass(
        "Bloom_BlurV",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(blurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            auto* blurTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurHandle_);
            if (!extractTex || !blurTex) {
                return;
            }
            manager->bloomParams_.direction = {0.0f, 1.0f};
            if (manager->mappedBloom_) {
                *(manager->mappedBloom_) = manager->bloomParams_;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, blurTex, extractTex->GetRtvHandle(),
                                    false, manager->bloomBlurVPSO_.Get());
        });

    // 4. Combine Pass
    renderGraph->AddPass(
        "Bloom_Combine",
        [this, inputHandle, outputHandle, writeToScreen](RenderGraphBuilder& builder,
                                                         const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            if (outputHandle != kInvalidHandle) {
                builder.RequireTransientState(outputHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            } else if (rc.engine && rc.engine->GetMainRenderTexture()) {
                builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(),
                                     D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
        },
        [this, manager, inputHandle, outputHandle, writeToScreen](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
            if (outputHandle != kInvalidHandle) {
                auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(outputHandle);
                if (!outTex) {
                    return;
                }
                targetRtv = outTex->GetRtvHandle();
            } else {
                targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
            }

            cmdList->OMSetRenderTargets(1, &targetRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(targetRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(writeToScreen ? manager->finalBloomCombinePSO_.Get()
                                                    : manager->bloomCombinePSO_.Get());
            cmdList->SetGraphicsRootSignature(manager->rootSig_);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
                inputTex ? inputTex->GetSrvIndex() : 0;
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex =
                extractTex ? extractTex->GetSrvIndex() : 0;
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                       manager->bindlessCB_->GetGPUVirtualAddress() +
                                                           manager->bindlessBufferOffset_ *
                                                               sizeof(PostProcessManager::BindlessParams));
            manager->TryIncrementBindlessOffset();

            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                       manager->bloomCB_->GetGPUVirtualAddress());
            cmdList->DrawInstanced(3, 1, 0, 0);
        });
}

} // namespace Irufemi
