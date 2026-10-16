#include "Renderer/PostProcess/PostProcessRunner.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
#include "Renderer/PostProcess/Effects/SeparableBlurEffect.h"
#include "Renderer/PostProcess/Effects/KawaseBlurEffect.h"
#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
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

void PostProcessRunner::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc,
                                         const std::vector<Mode>& modes) {
    if (modes.empty()) {
        return;
    }

    // 1. ピンポン作業用バッファ2枚の作成（UNORM）
    D3D12_RESOURCE_DESC workDesc = baseDesc;
    workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    workHandles_[0] = builder.CreateTransientResource("PP_Work0", workDesc);
    workHandles_[1] = builder.CreateTransientResource("PP_Work1", workDesc);
    builder.RequireTransientState(workHandles_[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    builder.RequireTransientState(workHandles_[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. 有効な各エフェクトに中間リソース宣言を委譲
    bool hasBloom = false;
    bool hasLightShafts = false;
    bool hasSmoothing = false;
    bool hasGaussian = false;
    bool hasKawase = false;

    for (auto mode : modes) {
        if (mode == Mode::Bloom) hasBloom = true;
        if (mode == Mode::LightShafts) hasLightShafts = true;
        if (mode == Mode::Smoothing) hasSmoothing = true;
        if (mode == Mode::GaussianFilter) hasGaussian = true;
        if (mode == Mode::DualKawaseBlur) hasKawase = true;
    }

    if (hasBloom && bloomEffect_) {
        bloomEffect_->DeclareResources(builder, baseDesc);
    }
    if (hasLightShafts && lightShaftsEffect_) {
        lightShaftsEffect_->DeclareResources(builder, baseDesc);
    }
    if (hasSmoothing && smoothingEffect_) {
        smoothingEffect_->DeclareResources(builder, baseDesc);
    }
    if (hasGaussian && gaussianEffect_) {
        gaussianEffect_->DeclareResources(builder, baseDesc);
    }
    if (hasKawase && kawaseBlurEffect_) {
        kawaseBlurEffect_->DeclareResources(builder, baseDesc);
    }
}

void PostProcessRunner::ResolveResources(RenderGraph* renderGraph, const std::vector<Mode>& modes) {
    if (!renderGraph || modes.empty()) {
        return;
    }

    workTextures_[0] = (workHandles_[0] != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(workHandles_[0]) : nullptr;
    workTextures_[1] = (workHandles_[1] != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(workHandles_[1]) : nullptr;

    for (auto mode : modes) {
        if (mode == Mode::Bloom && bloomEffect_) bloomEffect_->ResolveResources(renderGraph);
        if (mode == Mode::LightShafts && lightShaftsEffect_) lightShaftsEffect_->ResolveResources(renderGraph);
        if (mode == Mode::Smoothing && smoothingEffect_) smoothingEffect_->ResolveResources(renderGraph);
        if (mode == Mode::GaussianFilter && gaussianEffect_) gaussianEffect_->ResolveResources(renderGraph);
        if (mode == Mode::DualKawaseBlur && kawaseBlurEffect_) kawaseBlurEffect_->ResolveResources(renderGraph);
    }
}

RenderTexture* PostProcessRunner::Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                                      const std::vector<Mode>& modes, RenderTexture* srcTexture,
                                      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalOutput,
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
            RenderTexture* nextTarget = workTextures_[pingPongIdx % 2];
            if (!nextTarget) break;
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = nextTarget->GetRtvHandle();

            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle,
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            lightShaftsEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };

            if (mode == Mode::Smoothing) {
                smoothingEffect_->Render(ctx);
            } else {
                gaussianEffect_->Render(ctx);
            }

            if (!writeToScreen && nextTarget) {
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            kawaseBlurEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            combinedEffect_->SetBatch(batch);
            Irufemi::PostProcessRenderContext ctx{
                manager, commandList, currentSource, targetHandle,
                writeToScreen, isBackBufferTarget, targetViewport, targetScissor
            };
            combinedEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
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
            manager, commandList, currentSource, rtvHandle,
            true, isBackBufferTarget, targetViewport, targetScissor
        };
        Irufemi::CombinedPostEffect::RenderFinalCopyPass(ctx);
        finalWrittenTexture = nullptr;
    }

    return finalWrittenTexture;
}

void PostProcessRunner::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                      const std::vector<Mode>& modes,
                                      TransientResourceHandle srcHandle, TransientResourceHandle dstHandle,
                                      bool isFinalOutput) {
    if (!renderGraph || !manager) return;

    if (modes.empty()) {
        // エフェクトなしの場合、直接出力先へコピーパスを追加
        if (isFinalOutput && dstHandle != srcHandle) {
            combinedEffect_->ClearBatch();
            combinedEffect_->PopulatePasses(renderGraph, manager, srcHandle, dstHandle, true);
        }
        return;
    }

    TransientResourceHandle currentIn = srcHandle;
    size_t modeIdx = 0;
    int pingPongIdx = 0;

    while (modeIdx < modes.size()) {
        Mode mode = modes[modeIdx];
        bool isLastBatch = (modeIdx == modes.size() - 1);
        bool writeToScreen = isFinalOutput && isLastBatch;

        TransientResourceHandle nextOut = writeToScreen ? dstHandle : workHandles_[pingPongIdx % 2];

        if (mode == Mode::Bloom) {
            // Bloomは通常加算合成のため、途中の場合はピンポンバッファへ
            bloomEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        }
        else if (mode == Mode::LightShafts) {
            lightShaftsEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        }
        else if (mode == Mode::Smoothing) {
            smoothingEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        }
        else if (mode == Mode::GaussianFilter) {
            gaussianEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        }
        else if (mode == Mode::DualKawaseBlur) {
            kawaseBlurEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        }
        else {
            // 統合バッチ
            std::vector<Mode> batch;
            size_t lookAhead = modeIdx;
            while (lookAhead < modes.size() && modes[lookAhead] != Mode::Bloom &&
                   modes[lookAhead] != Mode::LightShafts && modes[lookAhead] != Mode::Smoothing &&
                   modes[lookAhead] != Mode::GaussianFilter && modes[lookAhead] != Mode::DualKawaseBlur &&
                   batch.size() < 16) {

                bool needsSeparate = RequiresSeparatePass(modes[lookAhead]);
                if (!batch.empty() && needsSeparate) break;
                batch.push_back(modes[lookAhead]);
                lookAhead++;
                if (needsSeparate) break;
            }

            isLastBatch = (lookAhead == modes.size());
            writeToScreen = isFinalOutput && isLastBatch;
            nextOut = writeToScreen ? dstHandle : workHandles_[pingPongIdx % 2];

            combinedEffect_->SetBatch(batch);
            combinedEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);

            currentIn = nextOut;
            pingPongIdx++;
            modeIdx = lookAhead;
        }
    }
}

