#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/PostProcess/PostProcessRunner.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/ErrorUtility.h"
#include <algorithm>
#include <cassert>
#include <d3d12.h>
#include <dxcapi.h>
#include <iostream>
#include <string>
#include <wrl/client.h>

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

void PostProcessManager::ResetAllParams(bool clearPostUI) {
    noiseParams_ = NoiseParams();
    vignetteParams_ = VignetteParams();
    smoothingParams_ = SmoothingParams();
    gaussianParams_ = GaussianParams();
    if (clearPostUI) {
        radialBlurParams_ = RadialBlurParams();
        dissolveParams_ = DissolveParams();
        fadeParams_ = FadeParams();
        slideParams_ = SlideParams();
    }
    outlineParams_ = OutlineParams();
    hsvParams_ = HSVParams();
    toneMappingParams_ = ToneMappingParams();
    bloomParams_ = BloomParams();
    glitchParams_ = GlitchParams();
    dualKawaseParams_ = DualKawaseBlurParams();
    luminanceOutlineParams_ = LuminanceOutlineParams();
    pixelationParams_ = PixelationParams();
    pointillismParams_ = PointillismParams();
    posterizationParams_ = PosterizationParams();
    nightVisionParams_ = NightVisionParams();
    kaleidoscopeParams_ = KaleidoscopeParams();
    chromaticAberrationParams_ = ChromaticAberrationParams();
    displacementMapParams_ = DisplacementMapParams();
    directionalBlurParams_ = DirectionalBlurParams();
    halftoneParams_ = HalftoneParams();
    dofParams_ = DepthOfFieldParams();
}

void PostProcessManager::Update(float totalTime) {
    CommitPendingModes();

    noiseParams_.time = totalTime;
    nightVisionParams_.time = totalTime;
    if (mappedNoise_) {
        *mappedNoise_ = noiseParams_;
    }

    // 他のパラメータも同期
    if (mappedVignette_) {
        *mappedVignette_ = vignetteParams_;
    }
    if (mappedSmoothing_) {
        *mappedSmoothing_ = smoothingParams_;
    }
    if (mappedGaussian_) {
        *mappedGaussian_ = gaussianParams_;
    }
    if (mappedRadialBlur_) {
        *mappedRadialBlur_ = radialBlurParams_;
    }
    if (mappedOutline_) {
        *mappedOutline_ = outlineParams_;
    }
    if (mappedDissolve_) {
        *mappedDissolve_ = dissolveParams_;
    }
    if (mappedHsv_) {
        *mappedHsv_ = hsvParams_;
    }
    if (mappedToneMapping_) {
        *mappedToneMapping_ = toneMappingParams_;
    }
    if (mappedFade_) {
        *mappedFade_ = fadeParams_;
    }
    if (mappedSlide_) {
        *mappedSlide_ = slideParams_;
    }
    if (mappedBloom_) {
        *mappedBloom_ = bloomParams_;
    }
    if (mappedLightShafts_) {
        *mappedLightShafts_ = lightShaftsParams_;
    }

    glitchParams_.time = totalTime;
    if (mappedGlitch_) {
        *mappedGlitch_ = glitchParams_;
    }

    if (mappedDualKawase_) {
        *mappedDualKawase_ = dualKawaseParams_;
    }

    // 統合パラメータの同期
    combinedParams_.vignetteColor = vignetteParams_.color;
    combinedParams_.vignetteRadius = vignetteParams_.radius;
    combinedParams_.vignetteSoftness = vignetteParams_.softness;
    combinedParams_.noiseIntensity = noiseParams_.intensity;
    combinedParams_.noiseTime = noiseParams_.time;
    combinedParams_.dissolveEdgeColor = dissolveParams_.edgeColor;
    combinedParams_.dissolveBackgroundColor = dissolveParams_.backgroundColor;
    combinedParams_.dissolveThreshold = dissolveParams_.threshold;
    combinedParams_.dissolveEdgeRange = dissolveParams_.edgeRange;
    combinedParams_.hsvHue = hsvParams_.hue;
    combinedParams_.hsvSaturation = hsvParams_.saturation;
    combinedParams_.hsvValue = hsvParams_.value;
    combinedParams_.toneMappingExposure = toneMappingParams_.exposure;
    combinedParams_.fadeColor = fadeParams_.color;
    combinedParams_.fadeIntensity = fadeParams_.intensity;
    combinedParams_.slideColor = slideParams_.color;
    combinedParams_.slideThreshold = slideParams_.threshold;
    combinedParams_.projectionInverse = outlineParams_.projectionInverse;
    combinedParams_.outlineIntensity = outlineParams_.intensity;
    combinedParams_.maskOutlineMaxRadius = outlineParams_.maskMaxRadius;
    combinedParams_.radialBlurCenter = radialBlurParams_.center;
    combinedParams_.radialBlurWidth = radialBlurParams_.blurWidth;
    combinedParams_.radialBlurSamples = radialBlurParams_.numSamples;
    combinedParams_.glitchIntensity = glitchParams_.intensity;
    combinedParams_.glitchTime = glitchParams_.time;
    combinedParams_.glitchEdgeMaskStrength = glitchParams_.edgeMaskStrength;
    combinedParams_.glitchProbability = glitchParams_.probability;
    combinedParams_.glitchBlockSizeX = glitchParams_.blockSizeX;
    combinedParams_.glitchBlockSizeY = glitchParams_.blockSizeY;
    combinedParams_.glitchOffsetBase = glitchParams_.offsetBase;
    combinedParams_.glitchOffsetMax = glitchParams_.offsetMax;
    combinedParams_.glitchRgbShiftBase = glitchParams_.rgbShiftBase;
    combinedParams_.glitchRgbShiftMax = glitchParams_.rgbShiftMax;
    combinedParams_.glitchScanlineFreq = glitchParams_.scanlineFreq;
    combinedParams_.glitchScanlineIntensity = glitchParams_.scanlineIntensity;
    combinedParams_.glitchColor = glitchParams_.color;

    combinedParams_.luminanceOutlineThreshold = luminanceOutlineParams_.threshold;
    combinedParams_.luminanceOutlineColor = luminanceOutlineParams_.outlineColor;

    combinedParams_.pixelationSize = pixelationParams_.pixelSize;

    combinedParams_.pointillismStrokeSize = pointillismParams_.strokeSize;
    combinedParams_.pointillismColorSteps = pointillismParams_.colorSteps;

    combinedParams_.posterizationSteps = posterizationParams_.colorSteps;

    combinedParams_.nightVisionIntensity = nightVisionParams_.intensity;
    combinedParams_.nightVisionTime = nightVisionParams_.time;

    combinedParams_.kaleidoscopeSegments = kaleidoscopeParams_.segments;

    combinedParams_.chromaticAberrationIntensity = chromaticAberrationParams_.intensity;

    displacementMapParams_.time = totalTime * displacementMapParams_.timeScale;
    combinedParams_.displacementMapIntensity = displacementMapParams_.intensity;
    combinedParams_.displacementMapTime = displacementMapParams_.time;
    combinedParams_.displacementMapTimeScale = displacementMapParams_.timeScale;

    combinedParams_.directionalBlurDirection = directionalBlurParams_.direction;
    combinedParams_.directionalBlurStrength = directionalBlurParams_.strength;
    combinedParams_.directionalBlurSamples = directionalBlurParams_.samples;

    combinedParams_.halftoneScale = halftoneParams_.scale;
    combinedParams_.halftoneAngle = halftoneParams_.angle;
    combinedParams_.halftoneBlend = halftoneParams_.blend;

    combinedParams_.dofFocusDistance = dofParams_.focusDistance;
    combinedParams_.dofFocusRange = dofParams_.focusRange;
    combinedParams_.dofBlurSize = dofParams_.blurSize;
    combinedParams_.dofSamples = dofParams_.samples;
}

void PostProcessManager::Draw(ID3D12GraphicsCommandList* commandList, RenderTexture* srcTexture,
                              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const PostProcessWorkspace& workspace, Layer layer,
                              bool isBackBufferTarget) {
    // bindlessBufferOffset_ と combinedBufferOffset_ は PreUI (最初のパス) のみリセットする
    if (layer == Layer::PreUI) {
        bindlessBufferOffset_ = 0;
        combinedBufferOffset_ = 0;

        std::lock_guard<std::mutex> lock(customParamsMutex_);
        if (mappedCustomEffectParams_) {
            std::memcpy(mappedCustomEffectParams_ + 1, persistentCustomParams_.data() + 1,
                        (kMaxCustomEffectParams - 1) * sizeof(CustomEffectParams));
        }
    }

    const std::vector<Mode>& modesToDraw = (layer == Layer::PreUI) ? activePreUI_ : activePostUI_;

    combinedParams_.useMask = (layer == Layer::PreUI) ? 1 : 0;

    PostProcessRunner runner;
    runner.Run(this, commandList, modesToDraw, srcTexture, rtvHandle, workspace, true, isBackBufferTarget);
}

void PostProcessManager::DrawSinglePass(ID3D12GraphicsCommandList* commandList, Mode mode, RenderTexture* srcTexture,
                                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalPass,
                                        ID3D12PipelineState* psoOverride) {
    uint32_t modeIdx = static_cast<uint32_t>(mode);
    if (!psoOverride && (modeIdx >= psos_.size() || !psos_[modeIdx])) {
        return;
    }

    // レンダーターゲットの設定
    commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    // 描画前にターゲットをクリアする（Shaderで discard された箇所が背景色として残るように）
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (mode == Mode::Dissolve) {
        clearColor[0] = dissolveParams_.backgroundColor.x;
        clearColor[1] = dissolveParams_.backgroundColor.y;
        clearColor[2] = dissolveParams_.backgroundColor.z;
        clearColor[3] = dissolveParams_.backgroundColor.w;
    }
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // PSOとルートシグネチャの設定
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

    // ソーステクスチャのバインド (Root2 -> t0)

    // -------------------------------------------------------------
    // Bindless パラメータの更新
    mappedBindless_[bindlessBufferOffset_].mainTextureIndex = srcTexture ? srcTexture->GetSrvIndex() : 0;

    uint32_t extraIdx = 0;
    if (mode == Mode::DepthBasedOutline) {
        extraIdx = depthSrvIndex_;
    } else if (mode == Mode::Dissolve) {
        int noiseIdx = (dissolveParams_.noiseType <= 0) ? 0 : 1;
        extraIdx = dissolveNoiseIndex_[noiseIdx];
    }
    mappedBindless_[bindlessBufferOffset_].extraTextureIndex = extraIdx;

    // マスク等のG-Bufferテクスチャのインデックスを渡す
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
    TryIncrementBindlessOffset();
    // -------------------------------------------------------------

    // 定数バッファのバインド (Root0 -> b0)
    D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0;
    switch (mode) {
    case Mode::Vignette:
        cbvAddress = vignetteCB_->GetGPUVirtualAddress();
        break;
    case Mode::Smoothing:
        cbvAddress = smoothingCB_->GetGPUVirtualAddress();
        break;
    case Mode::GaussianFilter:
        cbvAddress = gaussianCB_->GetGPUVirtualAddress();
        break;
    case Mode::RadialBlur:
        cbvAddress = radialBlurCB_->GetGPUVirtualAddress();
        break;
    case Mode::DepthBasedOutline:
        cbvAddress = outlineCB_->GetGPUVirtualAddress();
        break;
    case Mode::Dissolve:
        cbvAddress = dissolveCB_->GetGPUVirtualAddress();
        break;
    case Mode::Noise:
        cbvAddress = noiseCB_->GetGPUVirtualAddress();
        break;
    case Mode::HSV:
        cbvAddress = hsvCB_->GetGPUVirtualAddress();
        break;
    case Mode::ToneMapping:
        cbvAddress = toneMappingCB_->GetGPUVirtualAddress();
        break;
    case Mode::Fade:
        cbvAddress = fadeCB_->GetGPUVirtualAddress();
        break;
    case Mode::Slide:
        cbvAddress = slideCB_->GetGPUVirtualAddress();
        break;
    case Mode::Bloom:
        cbvAddress = bloomCB_->GetGPUVirtualAddress();
        break;
    case Mode::Glitch:
        cbvAddress = glitchCB_->GetGPUVirtualAddress();
        break;
    case Mode::DualKawaseBlur:
        cbvAddress = dualKawaseCB_->GetGPUVirtualAddress();
        break;
    default:
        break;
    }
    if (cbvAddress != 0) {
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }

    // 追加のリソース（深度、ノイズテクスチャ）のバインド (Root12 -> t1)
    if (mode == Mode::DepthBasedOutline) {
    } else if (mode == Mode::Dissolve) {
        int32_t noiseIdx = (std::max)(int32_t(0), (std::min)(int32_t(1), dissolveParams_.noiseType));
    }

    // 描画実行 (3頂点インデックスなし)
    commandList->DrawInstanced(3, 1, 0, 0);
}

void PostProcessManager::CreatePSOs() {
    auto* shaderManager = dxCommon_->GetShaderManager();

    // --- シェーダコンパイル設定 ---
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
        {Mode::Vignette, L"Vignette.PS.hlsl"},
        {Mode::Smoothing, L"Smoothing.PS.hlsl"},
        {Mode::GaussianFilter, L"GaussianFilter.PS.hlsl"},
        {Mode::DepthBasedOutline, L"DepthBasedOutline.PS.hlsl"},
        {Mode::RadialBlur, L"RadialBlur.PS.hlsl"},
        {Mode::Dissolve, L"Dissolve.PS.hlsl"},
        {Mode::Noise, L"Noise.PS.hlsl"},
        {Mode::HSV, L"HSV.PS.hlsl"},
        {Mode::ToneMapping, L"ToneMapping.PS.hlsl"},
        {Mode::Fade, L"Fade.PS.hlsl"},
        {Mode::Slide, L"Slide.PS.hlsl"},
        {Mode::Bloom, L"CopyImage.PS.hlsl"},
        {Mode::Glitch, L"Glitch.PS.hlsl"},
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

    // --- ブルーム用個別 PSO ---
    auto extractPS = shaderManager->GetOrCompile(L"HighLuminanceExtract.PS.hlsl", options);
    auto blurPS = shaderManager->GetOrCompile(L"GaussianBlur.PS.hlsl", options);
    auto combinePS = shaderManager->GetOrCompile(L"BloomCombine.PS.hlsl", options);

    if (extractPS && blurPS && combinePS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomDesc{};
        bloomDesc.pRootSignature = rootSig_;
        bloomDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        bloomDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        bloomDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        bloomDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        bloomDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        bloomDesc.DepthStencilState.DepthEnable = FALSE;
        bloomDesc.DepthStencilState.StencilEnable = FALSE;
        bloomDesc.InputLayout = {nullptr, 0};
        bloomDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bloomDesc.NumRenderTargets = 1;
        bloomDesc.RTVFormats[0] = rtvFormat_;
        bloomDesc.SampleDesc.Count = 1;

        bloomDesc.PS = {extractPS->GetBufferPointer(), extractPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomExtractPSO_));

        bloomDesc.PS = {blurPS->GetBufferPointer(), blurPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomBlurHPSO_));
        device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomBlurVPSO_));

        bloomDesc.PS = {combinePS->GetBufferPointer(), combinePS->GetBufferSize()};
        bloomDesc.RTVFormats[0] = rtvFormat_; // 中間パス用 (_UNORM)
        device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomCombinePSO_));

        bloomDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 最終パス用 (_SRGB)
        device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&finalBloomCombinePSO_));
    }

    // --- 統合ポストプロセス用 PSO ---
    auto combinedPS = shaderManager->GetOrCompile(L"PostProcess.PS.hlsl", options);
    if (combinedPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC combinedDesc{}; // ベースを流用
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

    // --- Dual Kawase Blur 用 PSO ---
    auto kawaseDownPS = shaderManager->GetOrCompile(L"DualKawaseDownsample.PS.hlsl", options);
    auto kawaseUpPS = shaderManager->GetOrCompile(L"DualKawaseUpsample.PS.hlsl", options);
    if (kawaseDownPS && kawaseUpPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC kwDesc{};
        kwDesc.pRootSignature = rootSig_;
        kwDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        kwDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        kwDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        kwDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        kwDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        kwDesc.DepthStencilState.DepthEnable = FALSE;
        kwDesc.DepthStencilState.StencilEnable = FALSE;
        kwDesc.InputLayout = {nullptr, 0};
        kwDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        kwDesc.NumRenderTargets = 1;
        kwDesc.SampleDesc.Count = 1;

        kwDesc.RTVFormats[0] = rtvFormat_;
        kwDesc.PS = {kawaseDownPS->GetBufferPointer(), kawaseDownPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&dualKawaseDownsamplePSO_));

        kwDesc.PS = {kawaseUpPS->GetBufferPointer(), kawaseUpPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&dualKawaseUpsamplePSO_));

        kwDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&kwDesc, IID_PPV_ARGS(&finalDualKawaseUpsamplePSO_));
    }

    // --- Light Shafts 用 PSO ---
    auto lsExtractPS = shaderManager->GetOrCompile(L"LightShaftsExtract.PS.hlsl", options);
    auto lsRadialBlurPS = shaderManager->GetOrCompile(L"LightShaftsRadialBlur.PS.hlsl", options);
    auto lsCombinePS = shaderManager->GetOrCompile(L"LightShaftsCombine.PS.hlsl", options);

    if (lsExtractPS && lsRadialBlurPS && lsCombinePS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC lsDesc{};
        lsDesc.pRootSignature = rootSig_;
        lsDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
        lsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        lsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        lsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        lsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        lsDesc.DepthStencilState.DepthEnable = FALSE;
        lsDesc.DepthStencilState.StencilEnable = FALSE;
        lsDesc.InputLayout = {nullptr, 0};
        lsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        lsDesc.NumRenderTargets = 1;
        lsDesc.RTVFormats[0] = rtvFormat_;
        lsDesc.SampleDesc.Count = 1;

        lsDesc.PS = {lsExtractPS->GetBufferPointer(), lsExtractPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&lsExtractPSO_));

        lsDesc.PS = {lsRadialBlurPS->GetBufferPointer(), lsRadialBlurPS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&lsRadialBlurPSO_));

        lsDesc.PS = {lsCombinePS->GetBufferPointer(), lsCombinePS->GetBufferSize()};
        device_->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&lsCombinePSO_));

        lsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device_->CreateGraphicsPipelineState(&lsDesc, IID_PPV_ARGS(&finalLsCombinePSO_));
    }
}

void PostProcessManager::CreateConstantBuffers() {
    combinedCB_ = CreateBuffer(sizeof(CombinedParams) * kMaxPostProcessBufferEntries);
    bindlessCB_ = CreateBuffer(sizeof(BindlessParams) * kMaxPostProcessBufferEntries);
    customEffectParamsCB_ = CreateBuffer(sizeof(CustomEffectParams) * kMaxCustomEffectParams);

    bindlessCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBindless_));
    combinedCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCombined_));
    customEffectParamsCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCustomEffectParams_));

    noiseCB_ = CreateBuffer(sizeof(NoiseParams));
    noiseCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedNoise_));

    vignetteCB_ = CreateBuffer(sizeof(VignetteParams));
    vignetteCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVignette_));

    smoothingCB_ = CreateBuffer(sizeof(SmoothingParams));
    smoothingCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedSmoothing_));

    gaussianCB_ = CreateBuffer(sizeof(GaussianParams));
    gaussianCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGaussian_));

    radialBlurCB_ = CreateBuffer(sizeof(RadialBlurParams));
    radialBlurCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedRadialBlur_));

    outlineCB_ = CreateBuffer(sizeof(OutlineParams));
    outlineCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedOutline_));

    dissolveCB_ = CreateBuffer(sizeof(DissolveParams));
    dissolveCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedDissolve_));

    hsvCB_ = CreateBuffer(sizeof(HSVParams));
    hsvCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedHsv_));

    toneMappingCB_ = CreateBuffer(sizeof(ToneMappingParams));
    toneMappingCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedToneMapping_));

    fadeCB_ = CreateBuffer(sizeof(FadeParams));
    fadeCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedFade_));

    slideCB_ = CreateBuffer(sizeof(SlideParams));
    slideCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedSlide_));

    bloomCB_ = CreateBuffer(sizeof(BloomParams));
    bloomCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBloom_));

    glitchCB_ = CreateBuffer(sizeof(GlitchParams));
    glitchCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGlitch_));

    dualKawaseCB_ = CreateBuffer(sizeof(DualKawaseBlurParams));
    dualKawaseCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedDualKawase_));

    lightShaftsCB_ = CreateBuffer(sizeof(LightShaftsParams));
    lightShaftsCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedLightShafts_));
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

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                     nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

bool PostProcessManager::TryIncrementBindlessOffset() {
    if (bindlessBufferOffset_ + 1 < kMaxPostProcessBufferEntries) {
        bindlessBufferOffset_++;
        return true;
    }
    IRUFEMI_WARNING(false,
                    "PostProcess bindless constant buffer overflow! Consider increasing kMaxPostProcessBufferEntries.");
    return false;
}

bool PostProcessManager::TryIncrementCombinedOffset() {
    if (combinedBufferOffset_ + 1 < kMaxPostProcessBufferEntries) {
        combinedBufferOffset_++;
        return true;
    }
    IRUFEMI_WARNING(false,
                    "PostProcess combined constant buffer overflow! Consider increasing kMaxPostProcessBufferEntries.");
    return false;
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

uint32_t PostProcessManager::RegisterCustomEffectParams(uint64_t objectId, const CustomEffectParams& params) {
    std::lock_guard<std::mutex> lock(customParamsMutex_);

    // 既にスロットが割り当てられている場合はパラメータのみ更新 (スロット番号は完全維持)
    if (objectId != 0) {
        auto it = objectToSlotMap_.find(objectId);
        if (it != objectToSlotMap_.end()) {
            uint32_t slot = it->second;
            persistentCustomParams_[slot] = params;
            return slot;
        }
    }

    // 新規スロットの割り当て
    uint32_t slot = 1;
    if (!freeSlots_.empty()) {
        slot = freeSlots_.back();
        freeSlots_.pop_back();
    } else {
        slot = kMaxCustomEffectParams - 1; // 上限超過時は最終スロット
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
        persistentCustomParams_[slot] = CustomEffectParams{}; // ゼロクリア
        freeSlots_.push_back(slot);
        objectToSlotMap_.erase(it);
    }
}

void PostProcessManager::ResetCustomEffectSlots() {
    std::lock_guard<std::mutex> lock(customParamsMutex_);
    persistentCustomParams_.fill(CustomEffectParams{});
    objectToSlotMap_.clear();
    freeSlots_.clear();
    freeSlots_.reserve(kMaxCustomEffectParams - 1);
    for (int i = static_cast<int>(kMaxCustomEffectParams - 1); i >= 1; --i) {
        freeSlots_.push_back(static_cast<uint32_t>(i));
    }
}

