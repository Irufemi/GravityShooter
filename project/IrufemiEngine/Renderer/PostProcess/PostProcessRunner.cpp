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
            mode == Mode::DirectionalBlur || mode == Mode::DepthOfField || mode == Mode::GravityDistortion);
}

void PostProcessRunner::DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc,
                                         const std::vector<Mode>& modes, PostProcessManager::Layer layer) {
    if (modes.empty()) {
        return;
    }

    size_t layerIdx = static_cast<size_t>(layer);
    std::string prefix = (layer == PostProcessManager::Layer::PreUI) ? "PP_PreUI_" : "PP_PostUI_";

    // 1. ピンポン作業用バッファ2枚の作成（UNORM）
    D3D12_RESOURCE_DESC workDesc = baseDesc;
    workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    workHandles_[layerIdx][0] = builder.CreateTransientResource(prefix + "Work0", workDesc);
    workHandles_[layerIdx][1] = builder.CreateTransientResource(prefix + "Work1", workDesc);
    builder.RequireTransientState(workHandles_[layerIdx][0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    builder.RequireTransientState(workHandles_[layerIdx][1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. 有効な各エフェクトに中間リソース宣言を委譲
    bool hasBloom = false;
    bool hasLightShafts = false;
    bool hasSmoothing = false;
    bool hasGaussian = false;
    bool hasKawase = false;

    for (auto mode : modes) {
        if (mode == Mode::Bloom) {
            hasBloom = true;
        }
        if (mode == Mode::LightShafts) {
            hasLightShafts = true;
        }
        if (mode == Mode::Smoothing) {
            hasSmoothing = true;
        }
        if (mode == Mode::GaussianFilter) {
            hasGaussian = true;
        }
        if (mode == Mode::DualKawaseBlur) {
            hasKawase = true;
        }
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

void PostProcessRunner::ResolveResources(RenderGraph* renderGraph, const std::vector<Mode>& modes,
                                         PostProcessManager::Layer layer) {
    if (!renderGraph || modes.empty()) {
        return;
    }

    size_t layerIdx = static_cast<size_t>(layer);
    workTextures_[layerIdx][0] = (workHandles_[layerIdx][0] != kInvalidHandle)
                                     ? renderGraph->GetTransientRenderTexture(workHandles_[layerIdx][0])
                                     : nullptr;
    workTextures_[layerIdx][1] = (workHandles_[layerIdx][1] != kInvalidHandle)
                                     ? renderGraph->GetTransientRenderTexture(workHandles_[layerIdx][1])
                                     : nullptr;

    for (auto mode : modes) {
        if (mode == Mode::Bloom && bloomEffect_) {
            bloomEffect_->ResolveResources(renderGraph);
        }
        if (mode == Mode::LightShafts && lightShaftsEffect_) {
            lightShaftsEffect_->ResolveResources(renderGraph);
        }
        if (mode == Mode::Smoothing && smoothingEffect_) {
            smoothingEffect_->ResolveResources(renderGraph);
        }
        if (mode == Mode::GaussianFilter && gaussianEffect_) {
            gaussianEffect_->ResolveResources(renderGraph);
        }
        if (mode == Mode::DualKawaseBlur && kawaseBlurEffect_) {
            kawaseBlurEffect_->ResolveResources(renderGraph);
        }
    }
}

void PostProcessRunner::TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
                                           D3D12_RESOURCE_STATES targetState) {
    if (!resource || !commandList) {
        return;
    }
    auto it = localResourceStates_.find(resource);
    D3D12_RESOURCE_STATES currentState =
        (it != localResourceStates_.end()) ? it->second : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (currentState != targetState) {
        DirectXUtils::TransitionBarrier(commandList, resource, currentState, targetState);
        localResourceStates_[resource] = targetState;
    }
}

RenderTexture* PostProcessRunner::Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                                      const std::vector<Mode>& modes, RenderTexture* srcTexture,
                                      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, PostProcessManager::Layer layer,
                                      bool isFinalOutput, bool isBackBufferTarget, const D3D12_VIEWPORT* targetViewport,
                                      const D3D12_RECT* targetScissor) {
    localResourceStates_.clear();

    RenderTexture* currentSource = srcTexture;
    bool needsFinalPass = isFinalOutput;

    size_t layerIdx = static_cast<size_t>(layer);
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
            RenderTexture* nextTarget = workTextures_[layerIdx][pingPongIdx % 2];
            if (!nextTarget) {
                break;
            }
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = nextTarget->GetRtvHandle();

            TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);

            Irufemi::PostProcessRenderContext ctx{manager,       commandList,        currentSource,  targetHandle,
                                                  writeToScreen, isBackBufferTarget, targetViewport, targetScissor};
            bloomEffect_->Render(ctx);

            TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            pingPongIdx++;
            finalWrittenTexture = nextTarget;
            currentSource = nextTarget;
            modeIdx++;
        }
        // 2. Light Shafts (ゴッドレイ)
        else if (mode == Mode::LightShafts) {
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[layerIdx][pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle =
                writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{manager,       commandList,        currentSource,  targetHandle,
                                                  writeToScreen, isBackBufferTarget, targetViewport, targetScissor};
            lightShaftsEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[layerIdx][pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle =
                writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{manager,       commandList,        currentSource,  targetHandle,
                                                  writeToScreen, isBackBufferTarget, targetViewport, targetScissor};

            if (mode == Mode::Smoothing) {
                smoothingEffect_->Render(ctx);
            } else {
                gaussianEffect_->Render(ctx);
            }

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[layerIdx][pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle =
                writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            Irufemi::PostProcessRenderContext ctx{manager,       commandList,        currentSource,  targetHandle,
                                                  writeToScreen, isBackBufferTarget, targetViewport, targetScissor};
            kawaseBlurEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
            RenderTexture* nextTarget = writeToScreen ? nullptr : workTextures_[layerIdx][pingPongIdx % 2];
            D3D12_CPU_DESCRIPTOR_HANDLE targetHandle =
                writeToScreen ? rtvHandle : (nextTarget ? nextTarget->GetRtvHandle() : rtvHandle);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            combinedEffect_->SetBatch(batch);
            Irufemi::PostProcessRenderContext ctx{manager,       commandList,        currentSource,  targetHandle,
                                                  writeToScreen, isBackBufferTarget, targetViewport, targetScissor};
            combinedEffect_->Render(ctx);

            if (!writeToScreen && nextTarget) {
                TransitionResource(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
        // 入力ソースと出力先 RTV が同一の場合は、自己コピー（フィードバックループ）を防止するためスキップ
        if (currentSource && currentSource->GetRtvHandle().ptr == rtvHandle.ptr) {
            finalWrittenTexture = nullptr;
            return finalWrittenTexture;
        }

        Irufemi::PostProcessRenderContext ctx{manager, commandList,        currentSource,  rtvHandle,
                                              true,    isBackBufferTarget, targetViewport, targetScissor};
        Irufemi::CombinedPostEffect::RenderFinalCopyPass(ctx);
        finalWrittenTexture = nullptr;
    }

    return finalWrittenTexture;
}

void PostProcessRunner::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                       const std::vector<Mode>& modes, TransientResourceHandle srcHandle,
                                       TransientResourceHandle dstHandle, PostProcessManager::Layer layer,
                                       bool isFinalOutput) {
    if (!renderGraph || !manager) {
        return;
    }

    if (modes.empty()) {
        // エフェクトなしの場合、直接出力先へコピーパスを追加
        if (isFinalOutput && dstHandle != srcHandle) {
            bool useMask = (layer == PostProcessManager::Layer::PreUI);
            combinedEffect_->PopulatePasses(renderGraph, manager, srcHandle, dstHandle, true, {}, useMask);
        }
        return;
    }

    size_t layerIdx = static_cast<size_t>(layer);
    std::string prefix = (layer == PostProcessManager::Layer::PreUI) ? "PP_PreUI_" : "PP_PostUI_";
    D3D12_RESOURCE_DESC workDesc{};
    if (auto* d = renderGraph->GetTransientDesc(srcHandle)) {
        workDesc = *d;
    }
    workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    workHandles_[layerIdx][0] = renderGraph->GetOrCreateTransientResource(prefix + "Work0", workDesc);
    workHandles_[layerIdx][1] = renderGraph->GetOrCreateTransientResource(prefix + "Work1", workDesc);

    TransientResourceHandle currentIn = srcHandle;
    size_t modeIdx = 0;
    int pingPongIdx = 0;

    while (modeIdx < modes.size()) {
        Mode mode = modes[modeIdx];
        bool isLastBatch = (modeIdx == modes.size() - 1);
        bool writeToScreen = isFinalOutput && isLastBatch;

        TransientResourceHandle nextOut = writeToScreen ? dstHandle : workHandles_[layerIdx][pingPongIdx % 2];

        if (mode == Mode::Bloom) {
            // Bloomは通常加算合成のため、途中の場合はピンポンバッファへ
            bloomEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        } else if (mode == Mode::LightShafts) {
            lightShaftsEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        } else if (mode == Mode::Smoothing) {
            smoothingEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        } else if (mode == Mode::GaussianFilter) {
            gaussianEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        } else if (mode == Mode::DualKawaseBlur) {
            kawaseBlurEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen);
            currentIn = nextOut;
            pingPongIdx++;
            modeIdx++;
        } else {
            // 統合バッチ
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
            nextOut = writeToScreen ? dstHandle : workHandles_[layerIdx][pingPongIdx % 2];

            bool useMask = (layer == PostProcessManager::Layer::PreUI);
            combinedEffect_->PopulatePasses(renderGraph, manager, currentIn, nextOut, writeToScreen, batch, useMask);

            currentIn = nextOut;
            pingPongIdx++;
            modeIdx = lookAhead;
        }
    }
}
