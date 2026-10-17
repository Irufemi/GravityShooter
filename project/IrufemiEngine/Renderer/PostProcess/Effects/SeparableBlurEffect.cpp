#include "Renderer/PostProcess/Effects/SeparableBlurEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"

namespace Irufemi {

SeparableBlurEffect::SeparableBlurEffect(PostProcessManager::Mode mode) : mode_(mode) {}

void SeparableBlurEffect::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) {
    D3D12_RESOURCE_DESC workDesc = baseDesc;
    workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    blurIntermediateHandle_ = builder.CreateTransientResource("SeparableBlurIntermediate", workDesc);
    builder.RequireTransientState(blurIntermediateHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SeparableBlurEffect::ResolveResources(RenderGraph* renderGraph) {
    if (renderGraph) {
        blurIntermediateTex_ = (blurIntermediateHandle_ != kInvalidHandle)
                                   ? renderGraph->GetTransientRenderTexture(blurIntermediateHandle_)
                                   : nullptr;
    }
}

void SeparableBlurEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    RenderTexture* blurH = blurIntermediateTex_;
    if (!blurH) {
        return;
    }

    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12PipelineState* psoH =
        (mode_ == PostProcessManager::Mode::Smoothing) ? manager->smoothingBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();
    ID3D12PipelineState* psoV = nullptr;
    if (mode_ == PostProcessManager::Mode::Smoothing) {
        psoV = writeToScreen ? manager->finalSmoothingBlurPSO_.Get() : manager->smoothingBlurPSO_.Get();
    } else {
        psoV = writeToScreen ? manager->finalGaussianBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();
    }

    // 1. Horizontal Blur Pass
    if (mode_ == PostProcessManager::Mode::Smoothing) {
        manager->smoothingParams_.direction = {1.0f, 0.0f};
        if (manager->mappedSmoothing_) {
            *(manager->mappedSmoothing_) = manager->smoothingParams_;
        }
    } else {
        manager->gaussianParams_.direction = {1.0f, 0.0f};
        if (manager->mappedGaussian_) {
            *(manager->mappedGaussian_) = manager->gaussianParams_;
        }
    }
    manager->DrawSinglePass(commandList, mode_, currentSource, blurH->GetRtvHandle(), false, psoH);

    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Vertical Blur Pass
    if (mode_ == PostProcessManager::Mode::Smoothing) {
        manager->smoothingParams_.direction = {0.0f, 1.0f};
        if (manager->mappedSmoothing_) {
            *(manager->mappedSmoothing_) = manager->smoothingParams_;
        }
    } else {
        manager->gaussianParams_.direction = {0.0f, 1.0f};
        if (manager->mappedGaussian_) {
            *(manager->mappedGaussian_) = manager->gaussianParams_;
        }
    }

    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
    manager->DrawSinglePass(commandList, mode_, blurH, targetHandle, writeToScreen, psoV);
}

void SeparableBlurEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                        TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                        bool writeToScreen) {
    if (!renderGraph || !manager) return;

    std::string passPrefix = (mode_ == PostProcessManager::Mode::Smoothing) ? "Smoothing" : "Gaussian";

    D3D12_RESOURCE_DESC desc{};
    if (auto* d = renderGraph->GetTransientDesc(inputHandle)) {
        desc = *d;
    }
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    blurIntermediateHandle_ = renderGraph->GetOrCreateTransientResource(passPrefix + "_Intermediate", desc);

    // 1. Horizontal Blur Pass
    renderGraph->AddPass(passPrefix + "_BlurH",
        [this, inputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(blurIntermediateHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, inputHandle](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* blurIntermediateTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurIntermediateHandle_);
            if (!inputTex || !blurIntermediateTex) return;

            ID3D12PipelineState* psoH =
                (mode_ == PostProcessManager::Mode::Smoothing) ? manager->smoothingBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();

            if (mode_ == PostProcessManager::Mode::Smoothing) {
                manager->smoothingParams_.direction = {1.0f, 0.0f};
                if (manager->mappedSmoothing_) *(manager->mappedSmoothing_) = manager->smoothingParams_;
            } else {
                manager->gaussianParams_.direction = {1.0f, 0.0f};
                if (manager->mappedGaussian_) *(manager->mappedGaussian_) = manager->gaussianParams_;
            }
            manager->DrawSinglePass(cmdList, mode_, inputTex, blurIntermediateTex->GetRtvHandle(), false, psoH);
        }
    );

    // 2. Vertical Blur Pass
    renderGraph->AddPass(passPrefix + "_BlurV",
        [this, outputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(blurIntermediateHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            if (outputHandle != kInvalidHandle) {
                builder.RequireTransientState(outputHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            } else if (rc.engine && rc.engine->GetMainRenderTexture()) {
                builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
        },
        [this, manager, outputHandle, writeToScreen](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* blurIntermediateTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurIntermediateHandle_);
            if (!blurIntermediateTex) return;

            D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
            if (outputHandle != kInvalidHandle) {
                auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(outputHandle);
                if (!outTex) return;
                targetRtv = outTex->GetRtvHandle();
            } else {
                targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
            }

            ID3D12PipelineState* psoV = nullptr;
            if (mode_ == PostProcessManager::Mode::Smoothing) {
                psoV = writeToScreen ? manager->finalSmoothingBlurPSO_.Get() : manager->smoothingBlurPSO_.Get();
            } else {
                psoV = writeToScreen ? manager->finalGaussianBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();
            }

            if (mode_ == PostProcessManager::Mode::Smoothing) {
                manager->smoothingParams_.direction = {0.0f, 1.0f};
                if (manager->mappedSmoothing_) *(manager->mappedSmoothing_) = manager->smoothingParams_;
            } else {
                manager->gaussianParams_.direction = {0.0f, 1.0f};
                if (manager->mappedGaussian_) *(manager->mappedGaussian_) = manager->gaussianParams_;
            }

            cmdList->OMSetRenderTargets(1, &targetRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(targetRtv, clearColor, 0, nullptr);
            manager->DrawSinglePass(cmdList, mode_, blurIntermediateTex, targetRtv, writeToScreen, psoV);
        }
    );
}

} // namespace Irufemi
