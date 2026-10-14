#include "Renderer/Fluid/FluidSimulation2D.h"

#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "RHI/DirectX12/ShaderManager.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include <cassert>
#include <algorithm>

namespace {
// ユーティリティ: UAVテクスチャの作成
Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVTexture(ID3D12Device* device, uint32_t width, uint32_t height,
                                                        DXGI_FORMAT format) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr =
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        nullptr, IID_PPV_ARGS(resource.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    return resource;
}
} // namespace

FluidSimulation2D::~FluidSimulation2D() {
    if (globalParamsCB_ && mappedGlobalParams_) {
        globalParamsCB_->Unmap(0, nullptr);
        mappedGlobalParams_ = nullptr;
    }
    if (injectParamsCB_ && mappedInjectParams_) {
        injectParamsCB_->Unmap(0, nullptr);
        mappedInjectParams_ = nullptr;
    }

    // エンジンの DescriptorPool から確保した全ディスクリプタを解放
    if (dxCommon_) {
        auto* pool = dxCommon_->GetSrvPool();
        if (pool) {
            for (int i = 0; i < 2; ++i) {
                if (velocitySrvIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(velocitySrvIndices_[i]);
                }
                if (densitySrvIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(densitySrvIndices_[i]);
                }
                if (pressureSrvIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(pressureSrvIndices_[i]);
                }
                if (velocityUavIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(velocityUavIndices_[i]);
                }
                if (densityUavIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(densityUavIndices_[i]);
                }
                if (pressureUavIndices_[i] != DescriptorPool::kInvalid) {
                    pool->Free(pressureUavIndices_[i]);
                }
            }
            if (divergenceSrvIndex_ != DescriptorPool::kInvalid) {
                pool->Free(divergenceSrvIndex_);
            }
            if (divergenceUavIndex_ != DescriptorPool::kInvalid) {
                pool->Free(divergenceUavIndex_);
            }
        }
    }
}

void FluidSimulation2D::Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height) {
    dxCommon_ = dxCommon;
    width_ = width;
    height_ = height;

    CreateTextures();
    CreateDescriptors();
    CreatePipelines();
}

void FluidSimulation2D::CreateTextures() {
    auto device = dxCommon_->GetDevice();

    // 速度場: RG16F (X, Y 速度)
    velocityTex_[0] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16G16_FLOAT);
    velocityTex_[1] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16G16_FLOAT);

    // 濃度場: RGBA16F (星雲カラー・濃度)
    densityTex_[0] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16G16B16A16_FLOAT);
    densityTex_[1] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // 圧力場: R16F
    pressureTex_[0] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16_FLOAT);
    pressureTex_[1] = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16_FLOAT);

    // 発散場: R16F
    divergenceTex_ = CreateUAVTexture(device, width_, height_, DXGI_FORMAT_R16_FLOAT);

    // 定数バッファ生成
    globalParamsCB_ = dxCommon_->CreateBufferResource(sizeof(SimulationParams));
    if (globalParamsCB_) {
        globalParamsCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGlobalParams_));
    }

    injectParamsCB_ = dxCommon_->CreateBufferResource(sizeof(InjectionParamsCB));
    if (injectParamsCB_) {
        injectParamsCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInjectParams_));
    }
}

void FluidSimulation2D::CreateDescriptors() {
    auto device = dxCommon_->GetDevice();
    auto* pool = dxCommon_->GetSrvPool();
    IRUFEMI_ASSERT(pool != nullptr);

    auto CreateSRV = [&](ID3D12Resource* res, DXGI_FORMAT fmt, uint32_t& outIndex) {
        outIndex = pool->Allocate();
        IRUFEMI_ASSERT(outIndex != DescriptorPool::kInvalid);
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = fmt;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(res, &desc, pool->GetCPUHandle(outIndex));
    };

    auto CreateUAV = [&](ID3D12Resource* res, DXGI_FORMAT fmt, uint32_t& outIndex) {
        outIndex = pool->Allocate();
        IRUFEMI_ASSERT(outIndex != DescriptorPool::kInvalid);
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = fmt;
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(res, nullptr, &desc, pool->GetCPUHandle(outIndex));
    };

    // 速度場 SRV / UAV (2面)
    CreateSRV(velocityTex_[0].Get(), DXGI_FORMAT_R16G16_FLOAT, velocitySrvIndices_[0]);
    CreateSRV(velocityTex_[1].Get(), DXGI_FORMAT_R16G16_FLOAT, velocitySrvIndices_[1]);
    CreateUAV(velocityTex_[0].Get(), DXGI_FORMAT_R16G16_FLOAT, velocityUavIndices_[0]);
    CreateUAV(velocityTex_[1].Get(), DXGI_FORMAT_R16G16_FLOAT, velocityUavIndices_[1]);

    // 濃度場 SRV / UAV (2面)
    CreateSRV(densityTex_[0].Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, densitySrvIndices_[0]);
    CreateSRV(densityTex_[1].Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, densitySrvIndices_[1]);
    CreateUAV(densityTex_[0].Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, densityUavIndices_[0]);
    CreateUAV(densityTex_[1].Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, densityUavIndices_[1]);

    // 圧力場 SRV / UAV (2面)
    CreateSRV(pressureTex_[0].Get(), DXGI_FORMAT_R16_FLOAT, pressureSrvIndices_[0]);
    CreateSRV(pressureTex_[1].Get(), DXGI_FORMAT_R16_FLOAT, pressureSrvIndices_[1]);
    CreateUAV(pressureTex_[0].Get(), DXGI_FORMAT_R16_FLOAT, pressureUavIndices_[0]);
    CreateUAV(pressureTex_[1].Get(), DXGI_FORMAT_R16_FLOAT, pressureUavIndices_[1]);

    // 発散場 SRV / UAV
    CreateSRV(divergenceTex_.Get(), DXGI_FORMAT_R16_FLOAT, divergenceSrvIndex_);
    CreateUAV(divergenceTex_.Get(), DXGI_FORMAT_R16_FLOAT, divergenceUavIndex_);
}

void FluidSimulation2D::CreatePipelines() {
    auto device = dxCommon_->GetDevice();
    auto rootSig = dxCommon_->GetComputeRootSignature();
    IRUFEMI_ASSERT(rootSig != nullptr);

    auto shaderManager = dxCommon_->GetShaderManager();

    auto CompileAndCreatePSO = [&](const std::wstring& shaderName,
                                   Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPSO) {
        ShaderCompileOptions options{};
        options.entryPoint = L"main";
        auto blob = shaderManager ? shaderManager->GetOrCompile(shaderName, options, L"cs_6_0") : nullptr;
        if (blob) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
            psoDesc.pRootSignature = rootSig;
            psoDesc.CS = {blob->GetBufferPointer(), blob->GetBufferSize()};
            HRESULT psoHr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&outPSO));
            ASSERT_IF_FAILED(psoHr);
        }
    };

    CompileAndCreatePSO(L"FluidAdvect.CS.hlsl", psoAdvect_);
    CompileAndCreatePSO(L"FluidInject.CS.hlsl", psoInject_);
    CompileAndCreatePSO(L"FluidVorticity.CS.hlsl", psoVorticity_);
    CompileAndCreatePSO(L"FluidDivergence.CS.hlsl", psoDivergence_);
    CompileAndCreatePSO(L"FluidJacobi.CS.hlsl", psoJacobi_);
    CompileAndCreatePSO(L"FluidProject.CS.hlsl", psoProject_);
}

void FluidSimulation2D::Update(float deltaTime) {
    deltaTime_ = (deltaTime > 0.0001f) ? deltaTime : (1.0f / 60.0f);

    if (mappedGlobalParams_) {
        mappedGlobalParams_->resolution = {static_cast<float>(width_), static_cast<float>(height_)};
        mappedGlobalParams_->invResolution = {1.0f / width_, 1.0f / height_};
        mappedGlobalParams_->deltaTime = deltaTime_;
        mappedGlobalParams_->dissipationVel = dissipationVel_;
        mappedGlobalParams_->dissipationDen = dissipationDen_;
        mappedGlobalParams_->vorticityScale = vorticityScale_;
    }
}

void FluidSimulation2D::AddForce(const Irufemi::Vector2& uv, const Irufemi::Vector2& velocity, float radius) {
    InjectionRequest req{};
    req.posUV = uv;
    req.velocity = velocity;
    req.radius = radius;
    req.type = 0.0f; // Force
    injectionQueue_.push_back(req);
}

void FluidSimulation2D::AddDensity(const Irufemi::Vector2& uv, const Irufemi::Vector4& color, float radius) {
    InjectionRequest req{};
    req.posUV = uv;
    req.color = color;
    req.radius = radius;
    req.type = 1.0f; // Density
    injectionQueue_.push_back(req);
}

void FluidSimulation2D::AddGravityVortex(const Irufemi::Vector2& uv, float pullStrength, float swirlStrength,
                                         float radius) {
    InjectionRequest req{};
    req.posUV = uv;
    req.pullStrength = pullStrength;
    req.vortexStrength = swirlStrength;
    req.radius = radius;
    req.type = 2.0f; // Singularity
    injectionQueue_.push_back(req);
}

void FluidSimulation2D::Reset() {
    injectionQueue_.clear();
    needsReset_ = true;
}

D3D12_GPU_DESCRIPTOR_HANDLE FluidSimulation2D::GetDensitySRV() const {
    if (dxCommon_) {
        if (auto* pool = dxCommon_->GetSrvPool()) {
            if (densitySrvIndices_[densityPingPong_] != DescriptorPool::kInvalid) {
                return pool->GetGPUHandle(densitySrvIndices_[densityPingPong_]);
            }
        }
    }
    return {0};
}

D3D12_GPU_DESCRIPTOR_HANDLE FluidSimulation2D::GetVelocitySRV() const {
    if (dxCommon_) {
        if (auto* pool = dxCommon_->GetSrvPool()) {
            if (velocitySrvIndices_[velocityPingPong_] != DescriptorPool::kInvalid) {
                return pool->GetGPUHandle(velocitySrvIndices_[velocityPingPong_]);
            }
        }
    }
    return {0};
}

void FluidSimulation2D::DispatchCompute() {
    auto commandList = dxCommon_->GetCommandList();
    if (!commandList || !psoAdvect_) {
        return;
    }

    auto* pool = dxCommon_->GetSrvPool();
    if (!pool) {
        return;
    }

    uint32_t dispatchX = (width_ + 15) / 16;
    uint32_t dispatchY = (height_ + 15) / 16;

    // エンジン統一の ComputeRootSignature を設定 (SetDescriptorHeapsの切り替えは一切不要)
    commandList->SetComputeRootSignature(dxCommon_->GetComputeRootSignature());

    // ゼロクリア（Reset）リクエスト処理: GPU上で両面（Ping-Pong）の速度・濃度を完全にゼロ化
    if (needsReset_) {
        needsReset_ = false;
        injectionQueue_.clear();
        if (psoInject_ && mappedInjectParams_) {
            commandList->SetPipelineState(psoInject_.Get());
            mappedInjectParams_->injectType = 3.0f; // Clear mode
            commandList->SetComputeRootConstantBufferView((UINT)ComputeRootSlot::CBV_b1,
                                                          injectParamsCB_->GetGPUVirtualAddress());

            for (int i = 0; i < 2; ++i) {
                commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                           pool->GetGPUHandle(velocitySrvIndices_[i]));
                commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t1,
                                                           pool->GetGPUHandle(densitySrvIndices_[i]));
                commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                           pool->GetGPUHandle(velocityUavIndices_[i]));
                commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u1,
                                                           pool->GetGPUHandle(densityUavIndices_[i]));
                commandList->Dispatch(dispatchX, dispatchY, 1);
                DirectXUtils::UAVBarrier(commandList, velocityTex_[i].Get());
                DirectXUtils::UAVBarrier(commandList, densityTex_[i].Get());
            }
        }
        return; // リセットフレームは移流・解法処理をスキップして即座に静寂状態を確立
    }

    commandList->SetComputeRootConstantBufferView((UINT)ComputeRootSlot::CBV_b0,
                                                  globalParamsCB_->GetGPUVirtualAddress());

    // ------------------------------------------------------------------------
    // 1. 半ラグランジュ移流 (Advection)
    // ------------------------------------------------------------------------
    commandList->SetPipelineState(psoAdvect_.Get());
    uint32_t readVel = velocityPingPong_;
    uint32_t writeVel = 1 - velocityPingPong_;
    uint32_t readDen = densityPingPong_;
    uint32_t writeDen = 1 - densityPingPong_;

    commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                               pool->GetGPUHandle(velocitySrvIndices_[readVel]));
    commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t1,
                                               pool->GetGPUHandle(densitySrvIndices_[readDen]));
    commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                               pool->GetGPUHandle(velocityUavIndices_[writeVel]));
    commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u1,
                                               pool->GetGPUHandle(densityUavIndices_[writeDen]));
    commandList->Dispatch(dispatchX, dispatchY, 1);

    velocityPingPong_ = writeVel;
    densityPingPong_ = writeDen;

    DirectXUtils::UAVBarrier(commandList, velocityTex_[velocityPingPong_].Get());
    DirectXUtils::UAVBarrier(commandList, densityTex_[densityPingPong_].Get());

    // ------------------------------------------------------------------------
    // 2. 外力・染料の注入 (Injection Queue)
    // ------------------------------------------------------------------------
    if (psoInject_ && !injectionQueue_.empty()) {
        commandList->SetPipelineState(psoInject_.Get());

        for (const auto& req : injectionQueue_) {
            if (mappedInjectParams_) {
                mappedInjectParams_->injectPosUV = req.posUV;
                mappedInjectParams_->injectVelocity = req.velocity;
                mappedInjectParams_->injectColor = req.color;
                mappedInjectParams_->injectRadius = req.radius;
                mappedInjectParams_->vortexStrength = req.vortexStrength;
                mappedInjectParams_->pullStrength = req.pullStrength;
                mappedInjectParams_->injectType = req.type;
            }
            commandList->SetComputeRootConstantBufferView((UINT)ComputeRootSlot::CBV_b1,
                                                          injectParamsCB_->GetGPUVirtualAddress());

            readVel = velocityPingPong_;
            writeVel = 1 - velocityPingPong_;
            readDen = densityPingPong_;
            writeDen = 1 - densityPingPong_;

            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                       pool->GetGPUHandle(velocitySrvIndices_[readVel]));
            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t1,
                                                       pool->GetGPUHandle(densitySrvIndices_[readDen]));
            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                       pool->GetGPUHandle(velocityUavIndices_[writeVel]));
            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u1,
                                                       pool->GetGPUHandle(densityUavIndices_[writeDen]));
            commandList->Dispatch(dispatchX, dispatchY, 1);

            velocityPingPong_ = writeVel;
            densityPingPong_ = writeDen;

            DirectXUtils::UAVBarrier(commandList, velocityTex_[velocityPingPong_].Get());
            DirectXUtils::UAVBarrier(commandList, densityTex_[densityPingPong_].Get());
        }
        injectionQueue_.clear();
    }

    // ------------------------------------------------------------------------
    // 3. 渦度閉じ込め (Vorticity Confinement)
    // ------------------------------------------------------------------------
    if (psoVorticity_) {
        commandList->SetPipelineState(psoVorticity_.Get());
        readVel = velocityPingPong_;
        writeVel = 1 - velocityPingPong_;

        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                   pool->GetGPUHandle(velocitySrvIndices_[readVel]));
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                   pool->GetGPUHandle(velocityUavIndices_[writeVel]));
        commandList->Dispatch(dispatchX, dispatchY, 1);

        velocityPingPong_ = writeVel;
        DirectXUtils::UAVBarrier(commandList, velocityTex_[velocityPingPong_].Get());
    }

    // ------------------------------------------------------------------------
    // 4. 発散計算 (Divergence: ∇・u)
    // ------------------------------------------------------------------------
    if (psoDivergence_) {
        commandList->SetPipelineState(psoDivergence_.Get());
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                   pool->GetGPUHandle(velocitySrvIndices_[velocityPingPong_]));
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                   pool->GetGPUHandle(divergenceUavIndex_));
        commandList->Dispatch(dispatchX, dispatchY, 1);

        DirectXUtils::UAVBarrier(commandList, divergenceTex_.Get());
    }

    // ------------------------------------------------------------------------
    // 5. 圧力ポアソン反復 (Jacobi: ∇²p = ∇・u)
    // ------------------------------------------------------------------------
    if (psoJacobi_) {
        commandList->SetPipelineState(psoJacobi_.Get());
        // gInDivergence は反復中不変 (register t1)
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t1,
                                                   pool->GetGPUHandle(divergenceSrvIndex_));

        for (uint32_t iter = 0; iter < jacobiIterations_; ++iter) {
            uint32_t readPress = pressurePingPong_;
            uint32_t writePress = 1 - pressurePingPong_;

            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                       pool->GetGPUHandle(pressureSrvIndices_[readPress]));
            commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                       pool->GetGPUHandle(pressureUavIndices_[writePress]));
            commandList->Dispatch(dispatchX, dispatchY, 1);

            pressurePingPong_ = writePress;
            DirectXUtils::UAVBarrier(commandList, pressureTex_[pressurePingPong_].Get());
        }
    }

    // ------------------------------------------------------------------------
    // 6. 圧力勾配減算・非圧縮化投影 (Projection)
    // ------------------------------------------------------------------------
    if (psoProject_) {
        commandList->SetPipelineState(psoProject_.Get());
        readVel = velocityPingPong_;
        writeVel = 1 - velocityPingPong_;

        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t0,
                                                   pool->GetGPUHandle(velocitySrvIndices_[readVel]));
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::SRV_t1,
                                                   pool->GetGPUHandle(pressureSrvIndices_[pressurePingPong_]));
        commandList->SetComputeRootDescriptorTable((UINT)ComputeRootSlot::UAV_u0,
                                                   pool->GetGPUHandle(velocityUavIndices_[writeVel]));
        commandList->Dispatch(dispatchX, dispatchY, 1);

        velocityPingPong_ = writeVel;
        DirectXUtils::UAVBarrier(commandList, velocityTex_[velocityPingPong_].Get());
    }
}
