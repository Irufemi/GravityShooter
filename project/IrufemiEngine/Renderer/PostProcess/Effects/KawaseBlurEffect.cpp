#include "Renderer/PostProcess/Effects/KawaseBlurEffect.h"
#include "Renderer/PostProcess/Effects/PostProcessEffectUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "RHI/DirectX12/ShaderManager.h"
#include "Renderer/Data/RenderContext.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include <algorithm>
#include <string>

namespace Irufemi {

void KawaseBlurEffect::Initialize(ID3D12Device* device, DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                                  D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat) {
    if (!device || !rootSig || !vsBytecode.pShaderBytecode) {
        return;
    }

    // 1. 専用定数バッファの生成 (256バイトアライメント)
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc{};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = (sizeof(DualKawaseBlurParams) + 255) & ~255;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&constantBuffer_));
    if (constantBuffer_) {
        constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParams_));
        if (mappedParams_) {
            *mappedParams_ = params_;
        }
    }

    // 2. 専用PSOの生成
    auto* shaderManager = dxCommon ? dxCommon->GetShaderManager() : nullptr;
    if (!shaderManager) {
        return;
    }

    ShaderCompileOptions options;
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    options.isDebug = true;
#endif
    auto kawaseDownPS = shaderManager->GetOrCompile(L"DualKawaseDownsample.PS.hlsl", options);
    auto kawaseUpPS = shaderManager->GetOrCompile(L"DualKawaseUpsample.PS.hlsl", options);

    if (kawaseDownPS && kawaseUpPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC kwDesc{};
        kwDesc.pRootSignature = rootSig;
        kwDesc.VS = vsBytecode;
        kwDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        kwDesc.SampleMask = UINT_MAX;
        kwDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        kwDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        kwDesc.RasterizerState.DepthClipEnable = TRUE;
        kwDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        kwDesc.NumRenderTargets = 1;
        kwDesc.SampleDesc.Count = 1;

        kwDesc.RTVFormats[0] = rtvFormat;
        kwDesc.PS = {kawaseDownPS->GetBufferPointer(), kawaseDownPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&downsamplePSO_));

        kwDesc.PS = {kawaseUpPS->GetBufferPointer(), kawaseUpPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&upsamplePSO_));

        kwDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&finalUpsamplePSO_));
    }
}

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
            textures_[i] =
                (handles_[i] != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(handles_[i]) : nullptr;
        }
    }
}

void KawaseBlurEffect::Render(const PostProcessRenderContext& context) {
    auto* manager = context.manager;
    auto* commandList = context.commandList;
    auto* currentSource = context.sourceTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = context.targetRtv;
    bool writeToScreen = context.writeToScreen;

    int32_t iterations =
        (std::min)(PostProcessManager::kMaxKawaseIterations, (std::max)(1, params_.iterationCount));
    RenderTexture* prevSource = currentSource;

    // パラメータを自身の定数バッファへコピー
    if (mappedParams_) {
        *mappedParams_ = params_;
    }

    D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;

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

        DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_RENDER_TARGET);

        manager->DrawSinglePass(commandList, PostProcessManager::Mode::DualKawaseBlur, prevSource,
                                kwTex->GetRtvHandle(), false, downsamplePSO_.Get(), cbvGpuAddress);

        DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        prevSource = kwTex;
    }

    // 2. Upsample Pass Loop (iterations == 1 の場合も最終アップサンプルを実行して出力先へ確実に書き込む)
    for (int i = (iterations > 1) ? (iterations - 2) : 0; i >= 0; --i) {
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

        ID3D12PipelineState* upPso = (isFinalUp && writeToScreen) ? finalUpsamplePSO_.Get()
                                                                  : upsamplePSO_.Get();

        // フルスクリーン上書き描画を行うため、直前のClearRenderTargetViewは不要（GPU帯域節約）
        manager->DrawSinglePass(commandList, PostProcessManager::Mode::DualKawaseBlur, prevSource, upHandle,
                                (isFinalUp && writeToScreen), upPso, cbvGpuAddress);

        if (!isFinalUp) {
            DirectXUtils::TransitionBarrier(commandList, kwTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        prevSource = kwTex;
    }

    // 3. Restore Full Viewport and Scissor
    D3D12_VIEWPORT fullViewport{};
    D3D12_RECT fullScissorRect{};

    SetupOutputViewport(manager->engine_, manager->dxCommon_, writeToScreen && context.isBackBufferTarget,
                        writeToScreen ? context.targetViewport : nullptr,
                        writeToScreen ? context.targetScissor : nullptr, fullViewport, fullScissorRect);
    commandList->RSSetViewports(1, &fullViewport);
    commandList->RSSetScissorRects(1, &fullScissorRect);
}

void KawaseBlurEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                      TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                      bool writeToScreen) {
    if (!renderGraph || !manager) {
        return;
    }

    D3D12_RESOURCE_DESC kwDesc{};
    if (auto* d = renderGraph->GetTransientDesc(inputHandle)) {
        kwDesc = *d;
    }
    kwDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (int i = 0; i < PostProcessManager::kMaxKawaseIterations; ++i) {
        kwDesc.Width = (std::max<UINT64>)(1, kwDesc.Width / 2);
        kwDesc.Height = (std::max<UINT>)(1, kwDesc.Height / 2);
        std::string name = "PP_Kawase_" + std::to_string(i);
        handles_[i] = renderGraph->GetOrCreateTransientResource(name, kwDesc);
    }

    int32_t iterations =
        (std::min)(PostProcessManager::kMaxKawaseIterations, (std::max)(1, params_.iterationCount));

    // 1. Downsample Passes
    TransientResourceHandle prevHandle = inputHandle;
    for (int i = 0; i < iterations; ++i) {
        TransientResourceHandle currentTargetHandle = handles_[i];
        renderGraph->AddPass(
            "Kawase_Downsample_" + std::to_string(i),
            [prevHandle, currentTargetHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
                builder.RequireTransientState(prevHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                builder.RequireTransientState(currentTargetHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
            },
            [this, manager, prevHandle, currentTargetHandle, i](const Irufemi::RenderContext& rc) {
                auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
                auto* prevTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(prevHandle);
                auto* kwTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(currentTargetHandle);
                if (!prevTex || !kwTex) {
                    return;
                }

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

                if (mappedParams_) {
                    *mappedParams_ = params_;
                }
                D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress =
                    constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;
                manager->DrawSinglePass(cmdList, PostProcessManager::Mode::DualKawaseBlur, prevTex,
                                        kwTex->GetRtvHandle(), false, downsamplePSO_.Get(), cbvGpuAddress);
            });
        prevHandle = currentTargetHandle;
    }

    // 2. Upsample Passes (iterations == 1 の場合も最終アップサンプルを実行して出力先へ確実に書き込む)
    for (int i = (iterations > 1) ? (iterations - 2) : 0; i >= 0; --i) {
        bool isFinalUp = (i == 0);
        TransientResourceHandle upTargetHandle = isFinalUp ? outputHandle : handles_[i];

        renderGraph->AddPass(
            "Kawase_Upsample_" + std::to_string(i),
            [prevHandle, upTargetHandle, isFinalUp](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
                builder.RequireTransientState(prevHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                if (isFinalUp && upTargetHandle == kInvalidHandle) {
                    if (rc.engine && rc.engine->GetMainRenderTexture()) {
                        builder.RequireState(rc.engine->GetMainRenderTexture()->GetResource(),
                                             D3D12_RESOURCE_STATE_RENDER_TARGET);
                    }
                } else if (upTargetHandle != kInvalidHandle) {
                    builder.RequireTransientState(upTargetHandle, D3D12_RESOURCE_STATE_RENDER_TARGET);
                }
            },
            [this, manager, prevHandle, upTargetHandle, i, isFinalUp, writeToScreen](const Irufemi::RenderContext& rc) {
                auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
                auto* prevTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(prevHandle);
                auto* kwTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(handles_[i]);
                if (!prevTex) {
                    return;
                }

                D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
                uint32_t tw = 0;
                uint32_t th = 0;

                if (isFinalUp) {
                    if (upTargetHandle != kInvalidHandle) {
                        auto* outTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(upTargetHandle);
                        if (!outTex) {
                            return;
                        }
                        targetRtv = outTex->GetRtvHandle();
                        tw = outTex->GetWidth();
                        th = outTex->GetHeight();
                    } else {
                        targetRtv = rc.engine->GetMainRenderTexture()->GetRtvHandle();
                        tw = rc.engine->GetGameResolutionWidth();
                        th = rc.engine->GetGameResolutionHeight();
                    }
                    cmdList->OMSetRenderTargets(1, &targetRtv, false, nullptr);
                } else {
                    if (!kwTex) {
                        return;
                    }
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

                if (mappedParams_) {
                    *mappedParams_ = params_;
                }
                D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress =
                    constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;
                ID3D12PipelineState* upPso = (isFinalUp && writeToScreen) ? finalUpsamplePSO_.Get()
                                                                          : upsamplePSO_.Get();
                manager->DrawSinglePass(cmdList, PostProcessManager::Mode::DualKawaseBlur, prevTex, targetRtv,
                                        (isFinalUp && writeToScreen), upPso, cbvGpuAddress);
            });
        prevHandle = upTargetHandle;
    }
}

} // namespace Irufemi
