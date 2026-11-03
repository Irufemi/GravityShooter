#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Data/RenderContext.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "RHI/DirectX12/ShaderManager.h"
#include <algorithm>

namespace Irufemi {

void BloomEffect::SetParams(const BloomParams& params) {
    params_ = params;
    if (mappedParams_) {
        *mappedParams_ = params_;
    }
}

void BloomEffect::Initialize(ID3D12Device* device, DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                             D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat) {
    if (!device || !rootSig || !vsBytecode.pShaderBytecode) {
        return;
    }

    // 1. 専用定数バッファの生成 (256バイトアライメント)
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc{};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = (sizeof(BloomParams) + 255) & ~255;
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

    auto extractPS = shaderManager->GetOrCompile(L"HighLuminanceExtract.PS.hlsl", options);
    auto blurPS = shaderManager->GetOrCompile(L"GaussianBlur.PS.hlsl", options);
    auto combinePS = shaderManager->GetOrCompile(L"BloomCombine.PS.hlsl", options);

    if (extractPS && blurPS && combinePS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomDesc{};
        bloomDesc.pRootSignature = rootSig;
        bloomDesc.VS = vsBytecode;
        bloomDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        bloomDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        bloomDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        bloomDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        bloomDesc.DepthStencilState.DepthEnable = FALSE;
        bloomDesc.DepthStencilState.StencilEnable = FALSE;
        bloomDesc.InputLayout = {nullptr, 0};
        bloomDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bloomDesc.NumRenderTargets = 1;
        bloomDesc.RTVFormats[0] = rtvFormat;
        bloomDesc.SampleDesc.Count = 1;

        bloomDesc.PS = {extractPS->GetBufferPointer(), extractPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&extractPSO_));

        bloomDesc.PS = {blurPS->GetBufferPointer(), blurPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&blurHPSO_));
        device->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&blurVPSO_));

        bloomDesc.PS = {combinePS->GetBufferPointer(), combinePS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&combinePSO_));

        bloomDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&finalCombinePSO_));
    }
}

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

    if (!bloomExtract || !blurH || !blurV || !manager) {
        return;
    }

    const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = GetConstantBufferAddress();

    // 1. High Luminance Extract Pass
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(),
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, currentSource, bloomExtract->GetRtvHandle(),
                            false, extractPSO_.Get(), cbvAddress);
    DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Horizontal Blur Pass
    params_.direction = {1.0f, 0.0f};
    if (mappedParams_) {
        *mappedParams_ = params_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, bloomExtract, blurH->GetRtvHandle(), false,
                            blurHPSO_.Get(), cbvAddress);
    DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 3. Vertical Blur Pass
    params_.direction = {0.0f, 1.0f};
    if (mappedParams_) {
        *mappedParams_ = params_;
    }
    DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    manager->DrawSinglePass(commandList, PostProcessManager::Mode::Bloom, blurH, blurV->GetRtvHandle(), false,
                            blurVPSO_.Get(), cbvAddress);
    DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 4. Combine Pass
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
    commandList->SetPipelineState(writeToScreen ? finalCombinePSO_.Get() : combinePSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = blurV ? blurV->GetSrvIndex() : 0;
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();

    if (cbvAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }
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

    const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = GetConstantBufferAddress();

    // 1. High Luminance Extract Pass
    renderGraph->AddPass(
        "Bloom_Extract",
        [this, inputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, inputHandle, cbvAddress](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            if (!inputTex || !extractTex) {
                return;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, inputTex, extractTex->GetRtvHandle(),
                                    false, extractPSO_.Get(), cbvAddress);
        });

    // 2. Horizontal Blur Pass
    renderGraph->AddPass(
        "Bloom_BlurH",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(blurHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, cbvAddress](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            auto* blurTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurHandle_);
            if (!extractTex || !blurTex) {
                return;
            }
            params_.direction = {1.0f, 0.0f};
            if (mappedParams_) {
                *mappedParams_ = params_;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, extractTex, blurTex->GetRtvHandle(),
                                    false, blurHPSO_.Get(), cbvAddress);
        });

    // 3. Vertical Blur Pass
    renderGraph->AddPass(
        "Bloom_BlurV",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(blurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(extractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, cbvAddress](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* extractTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(extractHandle_);
            auto* blurTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(blurHandle_);
            if (!extractTex || !blurTex) {
                return;
            }
            params_.direction = {0.0f, 1.0f};
            if (mappedParams_) {
                *mappedParams_ = params_;
            }
            manager->DrawSinglePass(cmdList, PostProcessManager::Mode::Bloom, blurTex, extractTex->GetRtvHandle(),
                                    false, blurVPSO_.Get(), cbvAddress);
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
        [this, manager, inputHandle, outputHandle, writeToScreen, cbvAddress](const Irufemi::RenderContext& rc) {
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
            cmdList->SetPipelineState(writeToScreen ? finalCombinePSO_.Get() : combinePSO_.Get());
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

            if (cbvAddress != 0) {
                cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
            }
            cmdList->DrawInstanced(3, 1, 0, 0);
        });
}

} // namespace Irufemi
