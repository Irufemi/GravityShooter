#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
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

void LightShaftsEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                      TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                      bool writeToScreen) {
    if (!renderGraph || !manager) return;

    D3D12_RESOURCE_DESC desc{};
    if (auto* d = renderGraph->GetTransientDesc(inputHandle)) {
        desc = *d;
    }
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    lsExtractHandle_ = renderGraph->GetOrCreateTransientResource("LS_Extract", desc);
    lsBlurHandle_ = renderGraph->GetOrCreateTransientResource("LS_Blur", desc);

    // 1. Light Shafts Extract Pass
    renderGraph->AddPass("LS_Extract",
        [this, inputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsExtractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, inputHandle](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* lsExtract = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsExtractHandle_);
            if (!inputTex || !lsExtract) return;

            D3D12_CPU_DESCRIPTOR_HANDLE lsExtractRtv = lsExtract->GetRtvHandle();
            cmdList->OMSetRenderTargets(1, &lsExtractRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(lsExtractRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(manager->lsExtractPSO_.Get());
            cmdList->SetGraphicsRootSignature(manager->rootSig_);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
                inputTex ? inputTex->GetSrvIndex() : 0;
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = manager->depthSrvIndex_;
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                       manager->bindlessCB_->GetGPUVirtualAddress() +
                                                           manager->bindlessBufferOffset_ *
                                                               sizeof(PostProcessManager::BindlessParams));
            manager->TryIncrementBindlessOffset();
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                       manager->lightShaftsCB_->GetGPUVirtualAddress());
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    );

    // 2. Radial Blur Pass
    renderGraph->AddPass("LS_RadialBlur",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(lsExtractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsBlurHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* lsExtract = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsExtractHandle_);
            auto* lsBlur = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsBlurHandle_);
            if (!lsExtract || !lsBlur) return;

            D3D12_CPU_DESCRIPTOR_HANDLE lsBlurRtv = lsBlur->GetRtvHandle();
            cmdList->OMSetRenderTargets(1, &lsBlurRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(lsBlurRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(manager->lsRadialBlurPSO_.Get());
            cmdList->SetGraphicsRootSignature(manager->rootSig_);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex = lsExtract->GetSrvIndex();
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = 0;
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                       manager->bindlessCB_->GetGPUVirtualAddress() +
                                                           manager->bindlessBufferOffset_ *
                                                               sizeof(PostProcessManager::BindlessParams));
            manager->TryIncrementBindlessOffset();
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material,
                                                       manager->lightShaftsCB_->GetGPUVirtualAddress());
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    );

    // 3. Combine Pass
    renderGraph->AddPass("LS_Combine",
        [this, inputHandle, outputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsBlurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            if (outputHandle != kInvalidHandle) {
                builder.RequireTransientState(outputHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            } else if (rc.engine && rc.engine->GetMainRenderTexture()) {
                builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
        },
        [this, manager, inputHandle, outputHandle, writeToScreen](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* lsBlur = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsBlurHandle_);
            if (!lsBlur) return;

            D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
            if (outputHandle != kInvalidHandle) {
                auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(outputHandle);
                if (!outTex) return;
                targetRtv = outTex->GetRtvHandle();
            } else {
                targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
            }

            cmdList->OMSetRenderTargets(1, &targetRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(targetRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(writeToScreen ? manager->finalLsCombinePSO_.Get()
                                                    : manager->lsCombinePSO_.Get());
            cmdList->SetGraphicsRootSignature(manager->rootSig_);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
                inputTex ? inputTex->GetSrvIndex() : 0;
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = lsBlur->GetSrvIndex();
            cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                       manager->bindlessCB_->GetGPUVirtualAddress() +
                                                           manager->bindlessBufferOffset_ *
                                                               sizeof(PostProcessManager::BindlessParams));
            manager->TryIncrementBindlessOffset();
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    );
}

} // namespace Irufemi
