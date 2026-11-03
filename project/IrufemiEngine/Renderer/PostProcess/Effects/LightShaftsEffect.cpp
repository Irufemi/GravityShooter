#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
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

void LightShaftsEffect::SetParams(const LightShaftsParams& params) {
    params_ = params;
    if (mappedParams_) {
        *mappedParams_ = params_;
    }
}

void LightShaftsEffect::Initialize(ID3D12Device* device, DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                                   D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat) {
    if (!device || !rootSig || !vsBytecode.pShaderBytecode) {
        return;
    }

    // 1. 専用定数バッファの生成 (256バイトアライメント)
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc{};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = (sizeof(LightShaftsParams) + 255) & ~255;
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

    auto lsExtractPS = shaderManager->GetOrCompile(L"LightShaftsExtract.PS.hlsl", options);
    auto lsRadialBlurPS = shaderManager->GetOrCompile(L"LightShaftsRadialBlur.PS.hlsl", options);
    auto lsCombinePS = shaderManager->GetOrCompile(L"LightShaftsCombine.PS.hlsl", options);

    if (lsExtractPS && lsRadialBlurPS && lsCombinePS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC lsDesc{};
        lsDesc.pRootSignature = rootSig;
        lsDesc.VS = vsBytecode;
        lsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        lsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        lsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        lsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        lsDesc.DepthStencilState.DepthEnable = FALSE;
        lsDesc.DepthStencilState.StencilEnable = FALSE;
        lsDesc.InputLayout = {nullptr, 0};
        lsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        lsDesc.NumRenderTargets = 1;
        lsDesc.RTVFormats[0] = rtvFormat;
        lsDesc.SampleDesc.Count = 1;

        lsDesc.PS = {lsExtractPS->GetBufferPointer(), lsExtractPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&extractPSO_));

        lsDesc.PS = {lsRadialBlurPS->GetBufferPointer(), lsRadialBlurPS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&radialBlurPSO_));

        lsDesc.PS = {lsCombinePS->GetBufferPointer(), lsCombinePS->GetBufferSize()};
        device->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&combinePSO_));

        lsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&finalCombinePSO_));
    }
}

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
        lsExtract_ =
            (lsExtractHandle_ != kInvalidHandle) ? renderGraph->GetTransientRenderTexture(lsExtractHandle_) : nullptr;
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

    if (!lsExtract || !lsBlur || !manager) {
        return;
    }

    const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = GetConstantBufferAddress();

    // 1. Light Shafts Extract Pass
    DirectXUtils::TransitionBarrier(commandList, lsExtract->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE lsExtractRtv = lsExtract->GetRtvHandle();
    commandList->OMSetRenderTargets(1, &lsExtractRtv, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(lsExtractRtv, clearColor, 0, nullptr);
    commandList->SetPipelineState(extractPSO_.Get());
    commandList->SetGraphicsRootSignature(manager->rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = manager->depthSrvIndex_;
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    if (cbvAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }
    commandList->DrawInstanced(3, 1, 0, 0);
    DirectXUtils::TransitionBarrier(commandList, lsExtract->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. Radial Blur Pass
    DirectXUtils::TransitionBarrier(commandList, lsBlur->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE lsBlurRtv = lsBlur->GetRtvHandle();
    commandList->OMSetRenderTargets(1, &lsBlurRtv, false, nullptr);
    commandList->ClearRenderTargetView(lsBlurRtv, clearColor, 0, nullptr);
    commandList->SetPipelineState(radialBlurPSO_.Get());

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex = lsExtract->GetSrvIndex();
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = 0;
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    if (cbvAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }
    commandList->DrawInstanced(3, 1, 0, 0);
    DirectXUtils::TransitionBarrier(commandList, lsBlur->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 3. Combine Pass
    commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
    commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
    commandList->SetPipelineState(writeToScreen ? finalCombinePSO_.Get() : combinePSO_.Get());

    manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex =
        currentSource ? currentSource->GetSrvIndex() : 0;
    manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = lsBlur->GetSrvIndex();
    commandList->SetGraphicsRootConstantBufferView(
        (UINT)RootSlot::LightCommon, manager->bindlessCB_->GetGPUVirtualAddress() +
                                         manager->bindlessBufferOffset_ * sizeof(PostProcessManager::BindlessParams));
    manager->TryIncrementBindlessOffset();
    if (cbvAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }
    commandList->DrawInstanced(3, 1, 0, 0);
}

void LightShaftsEffect::PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
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
    lsExtractHandle_ = renderGraph->GetOrCreateTransientResource("LS_Extract", desc);
    lsBlurHandle_ = renderGraph->GetOrCreateTransientResource("LS_Blur", desc);

    const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = GetConstantBufferAddress();

    // 1. Light Shafts Extract Pass
    renderGraph->AddPass(
        "LS_Extract",
        [this, inputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsExtractHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, inputHandle, cbvAddress](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* inputTex = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(inputHandle);
            auto* lsExtract = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsExtractHandle_);
            if (!inputTex || !lsExtract) {
                return;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE lsExtractRtv = lsExtract->GetRtvHandle();
            cmdList->OMSetRenderTargets(1, &lsExtractRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(lsExtractRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(extractPSO_.Get());
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
            if (cbvAddress != 0) {
                cmdList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
            }
            cmdList->DrawInstanced(3, 1, 0, 0);
        });

    // 2. Radial Blur Pass
    renderGraph->AddPass(
        "LS_RadialBlur",
        [this](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(lsExtractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsBlurHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
        },
        [this, manager, cbvAddress](const Irufemi::RenderContext& rc) {
            auto* cmdList = rc.drawManager->GetDxCommon()->GetCommandList();
            auto* lsExtract = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsExtractHandle_);
            auto* lsBlur = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsBlurHandle_);
            if (!lsExtract || !lsBlur) {
                return;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE lsBlurRtv = lsBlur->GetRtvHandle();
            cmdList->OMSetRenderTargets(1, &lsBlurRtv, false, nullptr);
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(lsBlurRtv, clearColor, 0, nullptr);
            cmdList->SetPipelineState(radialBlurPSO_.Get());
            cmdList->SetGraphicsRootSignature(manager->rootSig_);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            manager->mappedBindless_[manager->bindlessBufferOffset_].mainTextureIndex = lsExtract->GetSrvIndex();
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = 0;
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

    // 3. Combine Pass
    renderGraph->AddPass(
        "LS_Combine",
        [this, inputHandle, outputHandle](RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) {
            builder.RequireTransientState(inputHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            builder.RequireTransientState(lsBlurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
            auto* lsBlur = rc.drawManager->GetRenderGraph()->GetTransientRenderTexture(lsBlurHandle_);
            if (!lsBlur) {
                return;
            }

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
            manager->mappedBindless_[manager->bindlessBufferOffset_].extraTextureIndex = lsBlur->GetSrvIndex();
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
