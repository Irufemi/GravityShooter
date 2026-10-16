#include "Renderer/PostProcess/PostProcessRunner.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
#include "Renderer/PostProcess/Effects/SeparableBlurEffect.h"
#include "Renderer/PostProcess/Effects/KawaseBlurEffect.h"
#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include <algorithm>

using Mode = PostProcessManager::Mode;

PostProcessRunner::PostProcessRunner()
    : bloomEffect_(std::make_unique<Irufemi::BloomEffect>()),
      lightShaftsEffect_(std::make_unique<Irufemi::LightShaftsEffect>()),
      smoothingEffect_(std::make_unique<Irufemi::SeparableBlurEffect>(Mode::Smoothing)),
      gaussianEffect_(std::make_unique<Irufemi::SeparableBlurEffect>(Mode::GaussianFilter)),
      kawaseBlurEffect_(std::make_unique<Irufemi::KawaseBlurEffect>()),
      combinedEffect_(std::make_unique<Irufemi::CombinedPostEffect>()) {}

PostProcessRunner::~PostProcessRunner() = default;

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
        bool writeToScreen = isFinalOutput && isLastBatch;

        // 1. Bloom
        if (mode == Mode::Bloom) {
            writeToScreen = false;
            RenderTexture* nextTarget = workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = nextTarget->GetRtvHandle();

            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle, &workspace,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            bloomEffect_->Render(ctx);

            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            pingPongIdx++;
            finalWrittenTexture = nextTarget;
            currentSource = nextTarget;
            modeIdx++;
        }
        // 2. Light Shafts (ゴッドレイ)
        else if (mode == Mode::LightShafts) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle, &workspace,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            lightShaftsEffect_->Render(ctx);

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
        // 3. Smoothing / GaussianFilter
        else if (mode == Mode::Smoothing || mode == Mode::GaussianFilter) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            if (!writeToScreen) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle, &workspace,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };

            if (mode == Mode::Smoothing) {
                smoothingEffect_->Render(ctx);
            } else {
                gaussianEffect_->Render(ctx);
            }

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
        // 4. DualKawaseBlur
        else if (mode == Mode::DualKawaseBlur) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workspace.workTextures[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : nextTarget->GetRtvHandle();

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle, &workspace,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            kawaseBlurEffect_->Render(ctx);

            if (!writeToScreen) {
                pingPongIdx++;
                finalWrittenTexture = nextTarget;
            } else {
                finalWrittenTexture = nullptr;
            }
            currentSource = nextTarget;
            modeIdx++;
        }
        // 5. 統合バッチ / 単一パス
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

            combinedEffect_->SetBatch(batch);
            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle, &workspace,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            combinedEffect_->Render(ctx);

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

    // 最終出力パス（中間テクスチャからバックバッファへの転送が必要な場合）
    if (needsFinalPass) {
        Irufemi::PostProcessRenderContext ctx{
            manager, commandList, currentSource, rtvHandle, &workspace,
            true, isBackBufferTarget, targetViewport, targetScissor
        };
        Irufemi::CombinedPostEffect::RenderFinalCopyPass(ctx);
        finalWrittenTexture = nullptr;
    }

    return finalWrittenTexture;
}
