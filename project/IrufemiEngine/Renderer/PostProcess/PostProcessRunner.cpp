#include "Renderer/PostProcess/PostProcessRunner.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include <algorithm>
#include <cassert>

using Mode = PostProcessManager::Mode;

namespace {
void SetupOutputViewport(IrufemiEngine* engine, DirectXCommon* dxCommon, bool isBackBufferTarget,
                         const D3D12_VIEWPORT* explicitViewport, const D3D12_RECT* explicitScissor,
                         D3D12_VIEWPORT& outViewport, D3D12_RECT& outScissor) {
    if (explicitViewport && explicitScissor) {
        outViewport = *explicitViewport;
        outScissor = *explicitScissor;
        return;
    }

    if (isBackBufferTarget) {
        float clientW = dxCommon ? static_cast<float>(dxCommon->GetClientWidth()) : 1280.0f;
        float clientH = dxCommon ? static_cast<float>(dxCommon->GetClientHeight()) : 720.0f;
        float gameW = engine ? static_cast<float>(engine->GetGameResolutionWidth()) : 1280.0f;
        float gameH = engine ? static_cast<float>(engine->GetGameResolutionHeight()) : 720.0f;
        Irufemi::Math::LetterboxInfo view = Irufemi::Math::CalculateLetterbox(clientW, clientH, gameW, gameH);

        outViewport.Width = view.viewW;
        outViewport.Height = view.viewH;
        outViewport.TopLeftX = view.viewX;
        outViewport.TopLeftY = view.viewY;
        outScissor.left = static_cast<LONG>(outViewport.TopLeftX);
        outScissor.right = static_cast<LONG>(outViewport.TopLeftX + outViewport.Width);
        outScissor.top = static_cast<LONG>(outViewport.TopLeftY);
        outScissor.bottom = static_cast<LONG>(outViewport.TopLeftY + outViewport.Height);
    } else {
        outViewport.Width = static_cast<FLOAT>(engine ? engine->GetGameResolutionWidth() : 1280);
        outViewport.Height = static_cast<FLOAT>(engine ? engine->GetGameResolutionHeight() : 720);
        outViewport.TopLeftX = 0;
        outViewport.TopLeftY = 0;
        outScissor.left = 0;
        outScissor.right = engine ? engine->GetGameResolutionWidth() : 1280;
        outScissor.top = 0;
        outScissor.bottom = engine ? engine->GetGameResolutionHeight() : 720;
    }
    outViewport.MinDepth = 0.0f;
    outViewport.MaxDepth = 1.0f;
}
} // namespace

bool PostProcessRunner::RequiresSeparatePass(Mode mode) const {
    return (mode == Mode::GaussianFilter || mode == Mode::DepthBasedOutline || mode == Mode::RadialBlur ||
            mode == Mode::Glitch || mode == Mode::DualKawaseBlur || mode == Mode::Pointillism ||
            mode == Mode::Kaleidoscope || mode == Mode::ChromaticAberration || mode == Mode::DisplacementMap ||
            mode == Mode::DirectionalBlur || mode == Mode::DepthOfField);
}

RenderTexture* PostProcessRunner::Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                                      const std::vector<Mode>& modes, RenderTexture* srcTexture,
                                      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                                      const PostProcessManager::PostProcessWorkspace& workspace, bool isFinalOutput,
                                      bool isBackBufferTarget, const D3D12_VIEWPORT* targetViewport,
                                      const D3D12_RECT* targetScissor) {
    RenderTexture* currentSource = srcTexture;

    bool needsFinalPass = isFinalOutput;

    size_t modeIdx = 0;
    int pingPongIdx = 0;
    RenderTexture* finalWrittenTexture = nullptr;

    while (modeIdx < modes.size()) {
        Mode mode = modes[modeIdx];
        bool isLastBatch = (modeIdx == modes.size() - 1);

        // Final Output なら rtvHandle に書き込むが、途中のレイヤーであれば pingpong などの RenderTexture に書く
        bool writeToScreen = isFinalOutput && isLastBatch;

        // 1) Bloom
        if (mode == Mode::Bloom) {
            writeToScreen = false;
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            RenderTexture* bloomExtract = workspace.bloomExtract;
            RenderTexture* blurH = workspace.bloomBlur;
            RenderTexture* blurV = workspace.bloomExtract;

            DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);
            manager->DrawSinglePass(commandList, Mode::Bloom, currentSource, bloomExtract->GetRtvHandle(), false,
                                    manager->bloomExtractPSO_.Get());
            DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            manager->bloomParams_.direction = {1.0f, 0.0f};
            if (manager->mappedBloom_) {
                *(manager->mappedBloom_) = manager->bloomParams_;
            }
            DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);
            manager->DrawSinglePass(commandList, Mode::Bloom, bloomExtract, blurH->GetRtvHandle(), false,
                                    manager->bloomBlurHPSO_.Get());
            DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            manager->bloomParams_.direction = {0.0f, 1.0f};
            if (manager->mappedBloom_) {
                *(manager->mappedBloom_) = manager->bloomParams_;
            }
            DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);
            manager->DrawSinglePass(commandList, Mode::Bloom, blurH, blurV->GetRtvHandle(), false,
                                    manager->bloomBlurVPSO_.Get());
            DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                pingPongIdx++;
                finalWrittenTexture = nextTarget;
            } else {
                finalWrittenTexture = nullptr;
            }
            currentSource = nextTarget;
            modeIdx++;
        }
        // 1-LS) Light Shafts (ゴッドレイ)
        else if (mode == Mode::LightShafts) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            RenderTexture* lsExtract = workspace.lsExtract;
            RenderTexture* lsBlur = workspace.lsBlur;

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

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                pingPongIdx++;
                finalWrittenTexture = nextTarget;
            } else {
                finalWrittenTexture = nullptr;
            }
            currentSource = nextTarget;
            modeIdx++;
        }
        // 1-B) Smoothing / GaussianFilter
        else if (mode == Mode::Smoothing || mode == Mode::GaussianFilter) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            RenderTexture* blurH = workspace.bloomBlur;
            DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);

            ID3D12PipelineState* psoH =
                (mode == Mode::Smoothing) ? manager->smoothingBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();
            ID3D12PipelineState* psoV = nullptr;
            if (mode == Mode::Smoothing) {
                psoV = writeToScreen ? manager->finalSmoothingBlurPSO_.Get() : manager->smoothingBlurPSO_.Get();
            } else {
                psoV = writeToScreen ? manager->finalGaussianBlurPSO_.Get() : manager->gaussianBlurPSO_.Get();
            }

            if (mode == Mode::Smoothing) {
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
            manager->DrawSinglePass(commandList, mode, currentSource, blurH->GetRtvHandle(), false, psoH);

            DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            if (mode == Mode::Smoothing) {
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
            manager->DrawSinglePass(commandList, mode, blurH, targetHandle, writeToScreen, psoV);

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                pingPongIdx++;
                finalWrittenTexture = nextTarget;
            } else {
                finalWrittenTexture = nullptr;
            }
            currentSource = nextTarget;
            modeIdx++;
        }
        // 1-C) DualKawaseBlur
        else if (mode == Mode::DualKawaseBlur) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            int32_t iterations = (std::min)(PostProcessManager::kMaxKawaseIterations,
                                            (std::max)(1, manager->dualKawaseParams_.iterationCount));
            RenderTexture* prevSource = currentSource;

            for (int i = 0; i < iterations; ++i) {
                RenderTexture* kwTex = workspace.kawaseTextures[i];
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
                manager->DrawSinglePass(commandList, Mode::DualKawaseBlur, prevSource, kwTex->GetRtvHandle(), false,
                                        manager->dualKawaseDownsamplePSO_.Get());

                DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                prevSource = kwTex;
            }

            for (int i = iterations - 2; i >= 0; --i) {
                RenderTexture* kwTex = workspace.kawaseTextures[i];
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
                } else if (!writeToScreen) {
                    DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
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

                manager->DrawSinglePass(commandList, Mode::DualKawaseBlur, prevSource, upHandle,
                                        (isFinalUp && writeToScreen), upPso);

                if (!isFinalUp) {
                    DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(),
                                                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                } else if (!writeToScreen) {
                    DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    pingPongIdx++;
                    finalWrittenTexture = nextTarget;
                } else {
                    finalWrittenTexture = nullptr;
                }
                prevSource = kwTex;
            }

            D3D12_VIEWPORT fullViewport{};
            D3D12_RECT fullScissorRect{};

            SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && isBackBufferTarget,
                                writeToScreen ? targetViewport : nullptr, writeToScreen ? targetScissor : nullptr,
                                fullViewport, fullScissorRect);
            commandList->RSSetViewports(1, &fullViewport);
            commandList->RSSetScissorRects(1, &fullScissorRect);

            currentSource = nextTarget;
            modeIdx++;
        }
        // 2) 統合バッチ
        else {
            std::vector<Mode> batch;
            size_t lookAhead = modeIdx;
            while (lookAhead < modes.size() && modes[lookAhead] != Mode::Bloom &&
                   modes[lookAhead] != Mode::LightShafts && modes[lookAhead] != Mode::Smoothing &&
                   modes[lookAhead] != Mode::GaussianFilter && modes[lookAhead] != Mode::DualKawaseBlur &&
                   batch.size() < 16) {

                bool needsSeparate = RequiresSeparatePass(modes[lookAhead]);

                if (!batch.empty() && needsSeparate) {
                    break;
                }

                batch.push_back(modes[lookAhead]);
                lookAhead++;

                if (needsSeparate) {
                    break;
                }
            }

            isLastBatch = (lookAhead == modes.size());
            writeToScreen = isFinalOutput && isLastBatch;
            if (writeToScreen) {
                needsFinalPass = false;
            }
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            manager->combinedParams_.effectCount = (int32_t)batch.size();
            for (int i = 0; i < (int)batch.size(); ++i) {
                manager->combinedParams_.effects[i] = (int32_t)batch[i];
            }
            if (manager->mappedCombined_) {
                manager->mappedCombined_[manager->combinedBufferOffset_] = manager->combinedParams_;
            }

            commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
            float clearColor[] = {0, 0, 0, 1};
            commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);

            commandList->SetPipelineState(writeToScreen ? manager->finalCombinedPSO_.Get()
                                                        : manager->combinedPSO_.Get());
            commandList->SetGraphicsRootSignature(manager->rootSig_);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // Viewport と Scissor の設定
            D3D12_VIEWPORT fullViewport{};
            D3D12_RECT fullScissorRect{};
            SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && isBackBufferTarget,
                                writeToScreen ? targetViewport : nullptr, writeToScreen ? targetScissor : nullptr,
                                fullViewport, fullScissorRect);
            commandList->RSSetViewports(1, &fullViewport);
            commandList->RSSetScissorRects(1, &fullScissorRect);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
                currentSource ? currentSource->GetSrvIndex() : 0;

            uint32_t extraIdx = manager->depthSrvIndex_;
            for (int i = 0; i < (int)batch.size(); ++i) {
                if (PostProcessManager::UsesDepthBuffer(batch[i])) {
                    extraIdx = manager->depthSrvIndex_;
                } else if (batch[i] == Mode::Dissolve) {
                    int noiseIdx = (manager->dissolveParams_.noiseType <= 0) ? 0 : 1;
                    extraIdx = manager->dissolveNoiseIndex_[noiseIdx];
                }
            }
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = extraIdx;
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

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                pingPongIdx++;
                finalWrittenTexture = nextTarget;
            } else {
                finalWrittenTexture = nullptr;
            }
            currentSource = nextTarget;
            modeIdx = lookAhead;
        }
    }

    if (needsFinalPass) {
        manager->combinedParams_.effectCount = 0;
        if (manager->mappedCombined_) {
            manager->mappedCombined_[manager->combinedBufferOffset_] = manager->combinedParams_;
        }

        commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        commandList->SetPipelineState(manager->finalCombinedPSO_.Get());
        commandList->SetGraphicsRootSignature(manager->rootSig_);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Viewport と Scissor の設定
        D3D12_VIEWPORT fullViewport{};
        D3D12_RECT fullScissorRect{};
        SetupOutputViewport(manager->engine_, manager->dxCommon_, isBackBufferTarget, targetViewport, targetScissor,
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

        commandList->SetGraphicsRootConstantBufferView(
            (UINT)RootSlot::Material, manager->combinedCB_->GetGPUVirtualAddress() +
                                          manager->combinedBufferOffset_ * sizeof(PostProcessManager::CombinedParams));
        manager->TryIncrementCombinedOffset();

        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::CustomEffectParams,
                                                       manager->customEffectParamsCB_->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);

        finalWrittenTexture = nullptr;
    }

    return finalWrittenTexture;
}
