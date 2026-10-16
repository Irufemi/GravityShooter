#include "Renderer/PostProcess/Effects/KawaseBlurEffect.h"
#include "Renderer/PostProcess/Effects/PostProcessEffectUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include <algorithm>
#include <string>

namespace Irufemi {

void KawaseBlurEffect::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) {
    D3D12_RESOURCE_DESC kwDesc = baseDesc;
    kwDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (int i = 0; i < PostProcessManager::kMaxKawaseIterations; ++i) {
        kwDesc.Width = (std::max<UINT64>)(1, kwDesc.Width / 2);
        kwDesc.Height = (std::max<UINT>)(1, kwDesc.Height / 2);
        std::string name = "PP_Kawase_" + std::to_string(i);
        handles_[i] = builder.CreateTransientResource(name, kwDesc);
        builder.RequireTransientState(handles_[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void KawaseBlurEffect::ResolveResources(RenderGraph* renderGraph) {
    if (renderGraph) {
        for (int i = 0; i < PostProcessManager::kMaxKawaseIterations; ++i) {
            textures_[i] = (handles_[i] != kInvalidHandle)
                               ? renderGraph->GetTransientRenderTexture(handles_[i])
                               : nullptr;
        }
    }
}

void KawaseBlurEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    int32_t iterations = (std::min)(PostProcessManager::kMaxKawaseIterations,
                                    (std::max)(1, manager->dualKawaseParams_.iterationCount));
    RenderTexture* prevSource = currentSource;

    // 1. Downsample Pass Loop
    for (int i = 0; i < iterations; ++i) {
        RenderTexture* kwTex = textures_[i];
        if (!kwTex) {
            break;
        }

        D3D12_VIEWPORT viewport{};
        viewport.Width = (FLOAT)kwTex->GetWidth();
        viewport.Height = (FLOAT)kwTex->GetHeight();
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        commandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorRect{};
        scissorRect.right = kwTex->GetWidth();
        scissorRect.bottom = kwTex->GetHeight();
        commandList->RSSetScissorRects(1, &scissorRect);

        DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(),
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_RENDER_TARGET);

        if (manager->mappedDualKawase_) {
            *(manager->mappedDualKawase_) = manager->dualKawaseParams_;
        }
        manager->DrawSinglePass(commandList, PostProcessManager::Mode::DualKawaseBlur, prevSource,
                                kwTex->GetRtvHandle(), false, manager->dualKawaseDownsamplePSO_.Get());

        DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        prevSource = kwTex;
    }

    // 2. Upsample Pass Loop
    for (int i = iterations - 2; i >= 0; --i) {
        RenderTexture* kwTex = textures_[i];
        if (!kwTex && i != 0) {
            continue;
        }

        bool isFinalUp = (i == 0);
        D3D12_CPU_DESCRIPTOR_HANDLE upHandle = isFinalUp ? targetHandle : kwTex->GetRtvHandle();

        uint32_t tw = isFinalUp ? manager->engine_->GetGameResolutionWidth() : kwTex->GetWidth();
        uint32_t th = isFinalUp ? manager->engine_->GetGameResolutionHeight() : kwTex->GetHeight();

        D3D12_VIEWPORT viewport{};
        viewport.Width = (FLOAT)tw;
        viewport.Height = (FLOAT)th;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        commandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorRect{};
        scissorRect.right = tw;
        scissorRect.bottom = th;
        commandList->RSSetScissorRects(1, &scissorRect);

        if (!isFinalUp) {
            DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        ID3D12PipelineState* upPso = (isFinalUp && writeToScreen) ? manager->finalDualKawaseUpsamplePSO_.Get()
                                                                  : manager->dualKawaseUpsamplePSO_.Get();

        if (isFinalUp) {
            commandList->OMSetRenderTargets(1, &upHandle, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            commandList->ClearRenderTargetView(upHandle, clearColor, 0, nullptr);
        }

        manager->DrawSinglePass(commandList, PostProcessManager::Mode::DualKawaseBlur, prevSource, upHandle,
                                (isFinalUp && writeToScreen), upPso);

        if (!isFinalUp) {
            DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(),
                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        prevSource = kwTex;
    }

    // 3. Restore Full Viewport and Scissor
    D3D12_VIEWPORT fullViewport{};
    D3D12_RECT fullScissorRect{};

    SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && context.isBackBufferTarget,
                        writeToScreen ? context.targetViewport : nullptr, writeToScreen ? context.targetScissor : nullptr,
                        fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);
}

void KawaseBlurEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                     TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                     bool writeToScreen) {
    if (!renderGraph || !manager) return;

    int32_t iterations = (std::min)(PostProcessManager::kMaxKawaseIterations,
                                    (std::max)(1, manager->dualKawaseParams_.iterationCount));

    // 1. Downsample Passes
    TransientResourceHandle prevHandle = inputHandle;
    for (int i = 0; i < iterations; ++i) {
        TransientResourceHandle currentTargetHandle = handles_[i];
        renderGraph->AddPass("Kawase_Downsample_" + std::to_string(i),
            [prevHandle, currentTargetHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
                builder.RequireTransientState(prevHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                builder.RequireTransientState(currentTargetHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            },
            [this, manager, prevHandle, i](const Irufemi::RenderContext& rc) {
                auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
                auto* prevTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(prevHandle);
                auto* kwTex = textures_[i];
                if (!prevTex || !kwTex) return;

                D3D12_VIEWPORT viewport{};
                viewport.Width = (FLOAT)kwTex->GetWidth();
                viewport.Height = (FLOAT)kwTex->GetHeight();
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                cmdList->RSSetViewports(1, &viewport);

                D3D12_RECT scissorRect{};
                scissorRect.right = kwTex->GetWidth();
                scissorRect.bottom = kwTex->GetHeight();
                cmdList->RSSetScissorRects(1, &scissorRect);

                if (manager->mappedDualKawase_) *(manager->mappedDualKawase_) = manager->dualKawaseParams_;
                manager->DrawSinglePass(cmdList, PostProcessManager::Mode::DualKawaseBlur, prevTex,
                                        kwTex->GetRtvHandle(), false, manager->dualKawaseDownsamplePSO_.Get());
            }
        );
        prevHandle = currentTargetHandle;
    }

    // 2. Upsample Passes
    for (int i = iterations - 2; i >= 0; --i) {
        bool isFinalUp = (i == 0);
        TransientResourceHandle upTargetHandle = isFinalUp ? outputHandle : handles_[i];

        renderGraph->AddPass("Kawase_Upsample_" + std::to_string(i),
            [prevHandle, upTargetHandle, isFinalUp](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
                builder.RequireTransientState(prevHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                if (isFinalUp && upTargetHandle == kInvalidHandle) {
                    if (rc.engine && rc.engine->GetMainRenderTexture()) {
                        builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
                    }
                } else if (upTargetHandle != kInvalidHandle) {
                    builder.RequireTransientState(upTargetHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
                }
            },
            [this, manager, prevHandle, upTargetHandle, i, isFinalUp, writeToScreen](const Irufemi::RenderContext& rc) {
                auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
                auto* prevTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(prevHandle);
                auto* kwTex = textures_[i];
                if (!prevTex) return;

                D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
                uint32_t tw = 0;
                uint32_t th = 0;

                if (isFinalUp) {
                    if (upTargetHandle != kInvalidHandle) {
                        auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(upTargetHandle);
                        if (!outTex) return;
                        targetRtv = outTex->GetRtvHandle();
                        tw = outTex->GetWidth();
                        th = outTex->GetHeight();
                    } else {
                        targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
                        tw = rc.engine->GetGameResolutionWidth();
                        th = rc.engine->GetGameResolutionHeight();
                    }
                    cmdList->OMSetRenderTargets(1, &targetRtv, false, nullptr);
                    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
                    cmdList->ClearRenderTargetView(targetRtv, clearColor, 0, nullptr);
                } else {
                    if (!kwTex) return;
                    targetRtv = kwTex->GetRtvHandle();
                    tw = kwTex->GetWidth();
                    th = kwTex->GetHeight();
                }

                D3D12_VIEWPORT viewport{};
                viewport.Width = (FLOAT)tw;
                viewport.Height = (FLOAT)th;
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                cmdList->RSSetViewports(1, &viewport);

                D3D12_RECT scissorRect{};
                scissorRect.right = tw;
                scissorRect.bottom = th;
                cmdList->RSSetScissorRects(1, &scissorRect);

                ID3D12PipelineState* upPso = (isFinalUp && writeToScreen) ? manager->finalDualKawaseUpsamplePSO_.Get()
                                                                          : manager->dualKawaseUpsamplePSO_.Get();
                manager->DrawSinglePass(cmdList, PostProcessManager::Mode::DualKawaseBlur, prevTex, targetRtv,
                                        (isFinalUp && writeToScreen), upPso);
            }
        );
        prevHandle = upTargetHandle;
    }
}

} // namespace Irufemi
