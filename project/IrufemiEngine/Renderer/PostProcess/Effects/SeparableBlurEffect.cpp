#include "Renderer/PostProcess/Effects/SeparableBlurEffect.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"

namespace Irufemi {

SeparableBlurEffect::SeparableBlurEffect(PostProcessManager::Mode mode) : mode_(mode) {}

void SeparableBlurEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    const auto& workspace = *(context.workspace);
    bool writeToScreen = context.writeToScreen;

    RenderTexture* blurH = workspace.bloomBlur;
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

} // namespace Irufemi
