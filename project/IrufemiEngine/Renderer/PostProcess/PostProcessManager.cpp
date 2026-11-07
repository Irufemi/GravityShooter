#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/PostProcessHandle.h"
#include "Renderer/PostProcess/PostProcessRunner.h"
#include "Renderer/PostProcess/Effects/KawaseBlurEffect.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/PostProcess/Effects/LightShaftsEffect.h"
#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/ErrorUtility.h"
#include <algorithm>
#include <cmath>
#include <d3d12.h>
#include <dxcapi.h>
#include <iostream>
#include <string>
#include <wrl/client.h>

PostProcessManager::PostProcessManager() : runner_(std::make_unique<PostProcessRunner>()) {}
PostProcessManager::~PostProcessManager() = default;

void PostProcessManager::Initialize(IrufemiEngine* engine, DirectXCommon* dxCommon, DXGI_FORMAT rtvFormat) {
    engine_ = engine;
    dxCommon_ = dxCommon;
    device_ = dxCommon->GetDevice();
    rootSig_ = dxCommon->GetRootSignature();
    rtvFormat_ = rtvFormat;

    CreateConstantBuffers();
    CreatePSOs();
    ResetCustomEffectSlots();
}

void PostProcessManager::Reset(Layer layer) {
    if (layer == Layer::All) {
        ClearActiveModes(Layer::PreUI);
        ClearActiveModes(Layer::PostUI);
    } else {
        ClearActiveModes(layer);
    }
    CommitPendingModes();

    if (runner_) {
        runner_->ResetAllEffects(layer);
    }

    if (layer == Layer::All || layer == Layer::PreUI) {
        gravityDistortionActive_ = false;
        gravityDistortionTimer_ = 0.0f;
    }
}

void PostProcessManager::Update(float totalTime) {
    CommitPendingModes();

    auto* combined = GetEffect<Irufemi::CombinedPostEffect>();
    if (combined) {
        if (HasActiveMode(Mode::Noise)) {
            combined->GetNoiseParams().time = totalTime;
        }
        if (HasActiveMode(Mode::NightVision)) {
            combined->GetNightVisionParams().time = totalTime;
        }
        if (HasActiveMode(Mode::Glitch)) {
            combined->GetGlitchParams().time = totalTime;
        }
        auto& dm = combined->GetDisplacementMapParams();
        dm.time = totalTime * dm.timeScale;
    }

    if (gravityDistortionActive_) {
        float dt = engine_ ? engine_->GetGameDeltaTime() : 0.016f;
        gravityDistortionTimer_ += dt;
        float progress =
            gravityDistortionTimer_ / (gravityDistortionDuration_ > 0.0001f ? gravityDistortionDuration_ : 0.5f);
        if (progress >= 1.0f) {
            gravityDistortionActive_ = false;
            if (combined) {
                combined->GetGravityDistortionParams().intensity = 0.0f;
            }
            RemoveActiveMode(Mode::GravityDistortion);
        } else {
            float fade = 1.0f - progress;
            float fadeCurve = std::sin(fade * 1.5707963f);
            if (combined) {
                auto& p = combined->GetGravityDistortionParams();
                p.radius = progress * gravityDistortionMaxRadius_;
                p.intensity = gravityDistortionBaseIntensity_ * fadeCurve;
                p.time = totalTime;
            }
        }
    }
}

void PostProcessManager::BeginFrame() {
    bindlessBufferOffset_ = 0;
    combinedBufferOffset_ = 0;

    std::lock_guard<std::mutex> lock(customParamsMutex_);
    if (mappedCustomEffectParams_) {
        std::memcpy(mappedCustomEffectParams_, persistentCustomParams_.data(),
                    sizeof(CustomEffectParams) * kMaxCustomEffectParams);
    }
}

std::vector<PostProcessManager::Mode> PostProcessManager::GetActiveModes(Layer layer) const {
    std::lock_guard<std::mutex> lock(modesMutex_);
    return (layer == Layer::PreUI) ? activePreUI_ : activePostUI_;
}

bool PostProcessManager::HasActiveMode(Mode mode, Layer layer) const {
    std::lock_guard<std::mutex> lock(modesMutex_);
    auto checkFunc = [mode](const std::vector<PostProcessModeInfo>& list) {
        return std::any_of(list.begin(), list.end(),
                           [mode](const PostProcessModeInfo& info) { return info.mode == mode; });
    };

    if (layer == Layer::PreUI) {
        return checkFunc(pendingPreUI_);
    }
    if (layer == Layer::PostUI) {
        return checkFunc(pendingPostUI_);
    }
    return checkFunc(pendingPreUI_) || checkFunc(pendingPostUI_);
}

void PostProcessManager::AddActiveMode(Mode mode, Layer layer, int priority) {
    std::lock_guard<std::mutex> lock(modesMutex_);
    auto& list = (layer == Layer::PreUI) ? pendingPreUI_ : pendingPostUI_;

    for (const auto& info : list) {
        if (info.mode == mode) {
            return;
        }
    }

    list.push_back({mode, priority});
    std::stable_sort(list.begin(), list.end());
}

void PostProcessManager::RemoveActiveMode(Mode mode, Layer layer) {
    std::lock_guard<std::mutex> lock(modesMutex_);
    auto removeFunc = [mode](std::vector<PostProcessModeInfo>& list) {
        auto it = std::remove_if(list.begin(), list.end(),
                                 [mode](const PostProcessModeInfo& info) { return info.mode == mode; });
        list.erase(it, list.end());
    };

    if (layer == Layer::PreUI || layer == Layer::All) {
        removeFunc(pendingPreUI_);
    }
    if (layer == Layer::PostUI || layer == Layer::All) {
        removeFunc(pendingPostUI_);
    }
}

void PostProcessManager::ClearActiveModes(Layer layer) {
    std::lock_guard<std::mutex> lock(modesMutex_);
    if (layer == Layer::PreUI || layer == Layer::All) {
        pendingPreUI_.clear();
    }
    if (layer == Layer::PostUI || layer == Layer::All) {
        pendingPostUI_.clear();
    }
}

Irufemi::ScopedEffectHandle PostProcessManager::ActivateScoped(Mode mode, Layer layer, int priority) {
    Layer actualLayer = (layer == Layer::All) ? GetDefaultLayer(mode) : layer;
    AddActiveMode(mode, actualLayer, priority);
    return Irufemi::ScopedEffectHandle(this, mode, actualLayer);
}

void PostProcessManager::SetActiveModes(const std::vector<Mode>& modes, Layer layer) {
    std::lock_guard<std::mutex> lock(modesMutex_);
    auto& list = (layer == Layer::PreUI) ? pendingPreUI_ : pendingPostUI_;
    list.clear();
    for (size_t i = 0; i < modes.size(); ++i) {
        list.push_back({modes[i], static_cast<int>(i)});
    }
    std::stable_sort(list.begin(), list.end());
}

void PostProcessManager::CommitPendingModes() {
    std::lock_guard<std::mutex> lock(modesMutex_);
    activePreUI_.clear();
    for (const auto& info : pendingPreUI_) {
        activePreUI_.push_back(info.mode);
    }
    activePostUI_.clear();
    for (const auto& info : pendingPostUI_) {
        activePostUI_.push_back(info.mode);
    }
}

uint32_t PostProcessManager::AllocateCustomEffectSlot(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    auto it = objectToSlotMap_.find(objectId);
    if (it != objectToSlotMap_.end()) {
        return it->second;
    }

    if (freeSlots_.empty()) {
        return 0; // スロット枯渇時のフォールバック
    }

    uint32_t slot = freeSlots_.back();
    freeSlots_.pop_back();
    objectToSlotMap_[objectId] = slot;
    persistentCustomParams_[slot] = CustomEffectParams();
    return slot;
}

void PostProcessManager::FreeCustomEffectSlot(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    auto it = objectToSlotMap_.find(objectId);
    if (it != objectToSlotMap_.end()) {
        uint32_t slot = it->second;
        if (slot > 0 && slot < kMaxCustomEffectParams) {
            persistentCustomParams_[slot] = CustomEffectParams();
            freeSlots_.push_back(slot);
        }
        objectToSlotMap_.erase(it);
    }
}

void PostProcessManager::SetCustomEffectParams(uint32_t slot, const CustomEffectParams& params) {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    if (slot < kMaxCustomEffectParams) {
        persistentCustomParams_[slot] = params;
    }
}

const PostProcessManager::CustomEffectParams& PostProcessManager::GetCustomEffectParams(uint32_t slot) const {
    static const CustomEffectParams defaultParams{};
    if (slot < kMaxCustomEffectParams) {
        return persistentCustomParams_[slot];
    }
    return defaultParams;
}

void PostProcessManager::ResetCustomEffectSlots() {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    objectToSlotMap_.clear();
    freeSlots_.clear();
    for (uint32_t i = 1; i < kMaxCustomEffectParams; ++i) {
        freeSlots_.push_back(i);
    }
    persistentCustomParams_.fill(CustomEffectParams());
}

D3D12_GPU_VIRTUAL_ADDRESS PostProcessManager::GetCustomEffectParamsGpuAddress(uint32_t slot) const {
    if (!customEffectParamsCB_ || slot >= kMaxCustomEffectParams) {
        return 0;
    }
    return customEffectParamsCB_->GetGPUVirtualAddress() + (slot * sizeof(CustomEffectParams));
}

void PostProcessManager::TriggerGravityDistortion(const Irufemi::Vector2& centerUV, float intensity, float duration,
                                                  float maxRadius, float chromaticRatio, float waveWidth) {
    if (auto* combined = GetEffect<Irufemi::CombinedPostEffect>()) {
        auto& p = combined->GetGravityDistortionParams();
        p.center = centerUV;
        p.radius = 0.0f;
        p.waveWidth = waveWidth;
        p.chromaticRatio = chromaticRatio;
        p.intensity = intensity;
    }
    gravityDistortionBaseIntensity_ = intensity;
    gravityDistortionDuration_ = (duration > 0.0f) ? duration : 0.5f;
    gravityDistortionMaxRadius_ = maxRadius;
    gravityDistortionTimer_ = 0.0f;
    gravityDistortionActive_ = true;

    AddActiveMode(Mode::GravityDistortion, Layer::PreUI, 1250);
}

void PostProcessManager::CreateConstantBuffers() {
    combinedCB_ = CreateBuffer(sizeof(CombinedParams) * kMaxPostProcessBufferEntries);
    bindlessCB_ = CreateBuffer(sizeof(BindlessParams) * kMaxPostProcessBufferEntries);
    customEffectParamsCB_ = CreateBuffer(sizeof(CustomEffectParams) * kMaxCustomEffectParams);

    bindlessCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBindless_));
    combinedCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCombined_));
    customEffectParamsCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCustomEffectParams_));

    smoothingCB_ = CreateBuffer(sizeof(SmoothingParams));
    smoothingCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedSmoothing_));

    gaussianCB_ = CreateBuffer(sizeof(GaussianParams));
    gaussianCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGaussian_));
}

Microsoft::WRL::ComPtr<ID3D12Resource> PostProcessManager::CreateBuffer(size_t size) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (size + 255) & ~255;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                     nullptr, IID_PPV_ARGS(&buffer));
    return buffer;
}

void PostProcessManager::CreatePSOs() {
    auto* shaderManager = dxCommon_->GetShaderManager();

    ShaderCompileOptions options;
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    options.isDebug = true;
#endif

    auto vsBlob = shaderManager->GetOrCompile(L"Fullscreen.VS.hlsl", options);

    struct ShaderPath {
        Mode mode;
        std::wstring path;
    };
    std::vector<ShaderPath> shaders = {
        {Mode::None, L"CopyImage.PS.hlsl"},
        {Mode::Grayscale, L"Grayscale.PS.hlsl"},
        {Mode::Sepia, L"Sepia.PS.hlsl"},
    };

    for (const auto& s : shaders) {
        auto psBlob = shaderManager->GetOrCompile(s.path, options);
        if (!psBlob) {
            continue;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = rootSig_;
        desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.InputLayout = {nullptr, 0};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.SampleDesc.Count = 1;

        // 中間パス用 (_UNORM)
        desc.RTVFormats[0] = rtvFormat_;
        device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&psos_[static_cast<uint32_t>(s.mode)]));

        // 最終パス用 (_SRGB)
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&finalPsos_[static_cast<uint32_t>(s.mode)]));
    }

    // --- 統合ポストプロセス (Uber-Shader) 用 PSO ---
    auto combinedPS = shaderManager->GetOrCompile(L"PostProcess.PS.hlsl", options);
    if (combinedPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC combinedDesc{};
        combinedDesc.pRootSignature = rootSig_;
        combinedDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        combinedDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        combinedDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        combinedDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        combinedDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        combinedDesc.DepthStencilState.DepthEnable = FALSE;
        combinedDesc.DepthStencilState.StencilEnable = FALSE;
        combinedDesc.InputLayout = {nullptr, 0};
        combinedDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        combinedDesc.NumRenderTargets = 1;
        combinedDesc.SampleDesc.Count = 1;
        combinedDesc.PS = {combinedPS->GetBufferPointer(), combinedPS->GetBufferSize()};

        // 中間パス用
        combinedDesc.RTVFormats[0] = rtvFormat_;
        device_->CreateGraphicsPipelineState(&combinedDesc, IID_PPV_ARGS(&combinedPSO_));
        // 最終パス用
        combinedDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&combinedDesc, IID_PPV_ARGS(&finalCombinedPSO_));
    }

    // --- 分離可能フィルタ用 PSO ---
    auto boxBlurPS = shaderManager->GetOrCompile(L"BoxBlur.PS.hlsl", options);
    auto gaussianPS = shaderManager->GetOrCompile(L"GaussianFilter.PS.hlsl", options);

    if (boxBlurPS && gaussianPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC sepDesc{};
        sepDesc.pRootSignature = rootSig_;
        sepDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        sepDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        sepDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        sepDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        sepDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        sepDesc.DepthStencilState.DepthEnable = FALSE;
        sepDesc.DepthStencilState.StencilEnable = FALSE;
        sepDesc.InputLayout = {nullptr, 0};
        sepDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        sepDesc.NumRenderTargets = 1;
        sepDesc.SampleDesc.Count = 1;

        // BoxBlur PSO
        sepDesc.RTVFormats[0] = rtvFormat_;
        sepDesc.PS = {boxBlurPS->GetBufferPointer(), boxBlurPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&smoothingBlurPSO_));
        sepDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&finalSmoothingBlurPSO_));

        // GaussianFilter PSO
        sepDesc.RTVFormats[0] = rtvFormat_;
        sepDesc.PS = {gaussianPS->GetBufferPointer(), gaussianPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&gaussianBlurPSO_));
        sepDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&finalGaussianBlurPSO_));
    }

    // 各自律エフェクトの初期化（自己完結 PSO / CBV の作成）
    if (runner_) {
        runner_->InitializeEffects(device_, dxCommon_, rootSig_, {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()},
                                   rtvFormat_);
    }
}

void PostProcessManager::Draw(ID3D12GraphicsCommandList* commandList, RenderTexture* srcTexture,
                              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, Layer layer, bool isBackBufferTarget,
                              const D3D12_VIEWPORT* targetViewport, const D3D12_RECT* targetScissor) {
    if (layer == Layer::PreUI) {
        BeginFrame();
    }

    const std::vector<Mode>& modesToDraw = (layer == Layer::PreUI) ? activePreUI_ : activePostUI_;
    combinedParams_.useMask = (layer == Layer::PreUI) ? 1 : 0;

    if (runner_) {
        runner_->Run(this, commandList, modesToDraw, srcTexture, rtvHandle, layer, true, isBackBufferTarget,
                     targetViewport, targetScissor);
    }
}

uint32_t PostProcessManager::RegisterCustomEffectParams(uint64_t objectId, const CustomEffectParams& params) {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    if (objectId != 0) {
        auto it = objectToSlotMap_.find(objectId);
        if (it != objectToSlotMap_.end()) {
            uint32_t slot = it->second;
            persistentCustomParams_[slot] = params;
            return slot;
        }
    }

    uint32_t slot = 1;
    if (!freeSlots_.empty()) {
        slot = freeSlots_.back();
        freeSlots_.pop_back();
    } else {
        slot = kMaxCustomEffectParams - 1;
    }

    if (objectId != 0) {
        objectToSlotMap_[objectId] = slot;
    }
    persistentCustomParams_[slot] = params;
    return slot;
}

void PostProcessManager::UnregisterCustomEffectParams(uint64_t objectId) {
    if (objectId == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    auto it = objectToSlotMap_.find(objectId);
    if (it != objectToSlotMap_.end()) {
        uint32_t slot = it->second;
        persistentCustomParams_[slot] = CustomEffectParams{};
        freeSlots_.push_back(slot);
        objectToSlotMap_.erase(it);
    }
}

D3D12_GPU_VIRTUAL_ADDRESS PostProcessManager::AllocateBindlessParams(uint32_t mainTextureIndex,
                                                                     uint32_t extraTextureIndex,
                                                                     uint32_t maskTextureIndex) {
    if (!mappedBindless_ || !bindlessCB_) {
        return 0;
    }
    mappedBindless_[bindlessBufferOffset_].mainTextureIndex = mainTextureIndex;
    mappedBindless_[bindlessBufferOffset_].extraTextureIndex = extraTextureIndex;
    mappedBindless_[bindlessBufferOffset_].maskTextureIndex = maskTextureIndex;
    mappedBindless_[bindlessBufferOffset_].normalTextureIndex = 0;
    mappedBindless_[bindlessBufferOffset_].materialTextureIndex = 0;
    mappedBindless_[bindlessBufferOffset_].velocityTextureIndex = 0;

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress =
        bindlessCB_->GetGPUVirtualAddress() + bindlessBufferOffset_ * sizeof(BindlessParams);
    TryIncrementBindlessOffset();
    return gpuAddress;
}

void PostProcessManager::DrawSinglePass(ID3D12GraphicsCommandList* commandList, Mode mode, RenderTexture* srcTexture,
                                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalPass,
                                        ID3D12PipelineState* psoOverride, D3D12_GPU_VIRTUAL_ADDRESS cbvOverride) {
    uint32_t modeIdx = static_cast<uint32_t>(mode);
    if (!psoOverride && (modeIdx >= psos_.size() || !psos_[modeIdx])) {
        return;
    }

    commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    ID3D12PipelineState* pso = psoOverride;
    if (!pso) {
        pso = isFinalPass ? finalPsos_[modeIdx].Get() : psos_[modeIdx].Get();
    }
    if (!pso) {
        return;
    }

    commandList->SetPipelineState(pso);
    commandList->SetGraphicsRootSignature(rootSig_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mappedBindless_[bindlessBufferOffset_].mainTextureIndex = srcTexture ? srcTexture->GetSrvIndex() : 0;
    mappedBindless_[bindlessBufferOffset_].extraTextureIndex = 0;

    auto* engine = dxCommon_->GetEngine();
    mappedBindless_[bindlessBufferOffset_].maskTextureIndex =
        engine->GetEffectMaskTexture() ? engine->GetEffectMaskTexture()->GetSrvIndex() : 0;
    mappedBindless_[bindlessBufferOffset_].normalTextureIndex =
        engine->GetNormalTexture() ? engine->GetNormalTexture()->GetSrvIndex() : 0;
    mappedBindless_[bindlessBufferOffset_].materialTextureIndex =
        engine->GetMaterialTexture() ? engine->GetMaterialTexture()->GetSrvIndex() : 0;
    mappedBindless_[bindlessBufferOffset_].velocityTextureIndex =
        engine->GetVelocityTexture() ? engine->GetVelocityTexture()->GetSrvIndex() : 0;

    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon,
                                                   bindlessCB_->GetGPUVirtualAddress() +
                                                       bindlessBufferOffset_ * sizeof(BindlessParams));

    if (cbvOverride != 0) {
        commandList->SetGraphicsRootConstantBufferView(0, cbvOverride);
    }

    commandList->DrawInstanced(3, 1, 0, 0);
    TryIncrementBindlessOffset();
}

template <typename T> T* PostProcessManager::GetEffect() const {
    return runner_ ? runner_->GetEffect<T>() : nullptr;
}

template Irufemi::BloomEffect* PostProcessManager::GetEffect<Irufemi::BloomEffect>() const;
template Irufemi::KawaseBlurEffect* PostProcessManager::GetEffect<Irufemi::KawaseBlurEffect>() const;
template Irufemi::LightShaftsEffect* PostProcessManager::GetEffect<Irufemi::LightShaftsEffect>() const;
template Irufemi::CombinedPostEffect* PostProcessManager::GetEffect<Irufemi::CombinedPostEffect>() const;

// ============================================================================
// 各ポストプロセスパラメータへの安全な委譲アクセサ（Facade API）
// ============================================================================

#define IRUFEMI_PP_ACCESSOR_COMBINED(Type, Getter)                                                                     \
    PostProcessManager::Type& PostProcessManager::Get##Type() {                                                        \
        if (auto* c = GetEffect<Irufemi::CombinedPostEffect>()) {                                                      \
            return c->Getter();                                                                                        \
        }                                                                                                              \
        static Type dummy{};                                                                                           \
        return dummy;                                                                                                  \
    }                                                                                                                  \
    const PostProcessManager::Type& PostProcessManager::Get##Type() const {                                            \
        if (auto* c = GetEffect<Irufemi::CombinedPostEffect>()) {                                                      \
            return c->Getter();                                                                                        \
        }                                                                                                              \
        static Type dummy{};                                                                                           \
        return dummy;                                                                                                  \
    }

IRUFEMI_PP_ACCESSOR_COMBINED(FadeParams, GetFadeParams)
IRUFEMI_PP_ACCESSOR_COMBINED(RadialBlurParams, GetRadialBlurParams)
IRUFEMI_PP_ACCESSOR_COMBINED(ChromaticAberrationParams, GetChromaticAberrationParams)
IRUFEMI_PP_ACCESSOR_COMBINED(DisplacementMapParams, GetDisplacementMapParams)
IRUFEMI_PP_ACCESSOR_COMBINED(GravityDistortionParams, GetGravityDistortionParams)
IRUFEMI_PP_ACCESSOR_COMBINED(BlackHoleSupernovaParams, GetBlackHoleSupernovaParams)
IRUFEMI_PP_ACCESSOR_COMBINED(DissolveParams, GetDissolveParams)
IRUFEMI_PP_ACCESSOR_COMBINED(SlideParams, GetSlideParams)
IRUFEMI_PP_ACCESSOR_COMBINED(NoiseParams, GetNoiseParams)
IRUFEMI_PP_ACCESSOR_COMBINED(GlitchParams, GetGlitchParams)
IRUFEMI_PP_ACCESSOR_COMBINED(VignetteParams, GetVignetteParams)
IRUFEMI_PP_ACCESSOR_COMBINED(OutlineParams, GetOutlineParams)
IRUFEMI_PP_ACCESSOR_COMBINED(HSVParams, GetHSVParams)
IRUFEMI_PP_ACCESSOR_COMBINED(ToneMappingParams, GetToneMappingParams)
IRUFEMI_PP_ACCESSOR_COMBINED(DepthOfFieldParams, GetDepthOfFieldParams)
IRUFEMI_PP_ACCESSOR_COMBINED(DirectionalBlurParams, GetDirectionalBlurParams)
IRUFEMI_PP_ACCESSOR_COMBINED(HalftoneParams, GetHalftoneParams)
IRUFEMI_PP_ACCESSOR_COMBINED(LuminanceOutlineParams, GetLuminanceOutlineParams)
IRUFEMI_PP_ACCESSOR_COMBINED(PixelationParams, GetPixelationParams)
IRUFEMI_PP_ACCESSOR_COMBINED(PointillismParams, GetPointillismParams)
IRUFEMI_PP_ACCESSOR_COMBINED(PosterizationParams, GetPosterizationParams)

#undef IRUFEMI_PP_ACCESSOR_COMBINED

PostProcessManager::BloomParams& PostProcessManager::GetBloomParams() {
    if (auto* b = GetEffect<Irufemi::BloomEffect>()) {
        return b->GetParams();
    }
    static BloomParams dummy{};
    return dummy;
}
const PostProcessManager::BloomParams& PostProcessManager::GetBloomParams() const {
    if (auto* b = GetEffect<Irufemi::BloomEffect>()) {
        return b->GetParams();
    }
    static BloomParams dummy{};
    return dummy;
}

PostProcessManager::LightShaftsParams& PostProcessManager::GetLightShaftsParams() {
    if (auto* ls = GetEffect<Irufemi::LightShaftsEffect>()) {
        return ls->GetParams();
    }
    static LightShaftsParams dummy{};
    return dummy;
}
const PostProcessManager::LightShaftsParams& PostProcessManager::GetLightShaftsParams() const {
    if (auto* ls = GetEffect<Irufemi::LightShaftsEffect>()) {
        return ls->GetParams();
    }
    static LightShaftsParams dummy{};
    return dummy;
}

PostProcessManager::DualKawaseBlurParams& PostProcessManager::GetDualKawaseBlurParams() {
    if (auto* k = GetEffect<Irufemi::KawaseBlurEffect>()) {
        return k->GetParams();
    }
    static DualKawaseBlurParams dummy{};
    return dummy;
}
const PostProcessManager::DualKawaseBlurParams& PostProcessManager::GetDualKawaseBlurParams() const {
    if (auto* k = GetEffect<Irufemi::KawaseBlurEffect>()) {
        return k->GetParams();
    }
    static DualKawaseBlurParams dummy{};
    return dummy;
}
