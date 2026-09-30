#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/DirectXCommon.h"

#include "Resource/Texture/TextureUtility.h"
#include <string>
#include <cassert>
#include <vector>
#include <comdef.h>
#include <iostream>
#include "Core/Utility/Log.h"
#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/StringUtility.h"
#include "Core/Utility/FileSystem.h"
#include "../../../externals/DirectXTex/d3dx12.h"
#include <algorithm>
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

#include "RHI/DirectX12/DXCommandManager.h"
#include "RHI/DirectX12/DXSwapChainManager.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/Profiler/GpuProfiler.h"

ID3D12CommandQueue* DirectXCommon::GetCommandQueue() {
    return commandManager_->GetCommandQueue();
}
ID3D12CommandAllocator* DirectXCommon::GetCommandAllocator() {
    return commandManager_->GetCommandAllocator(frameIndex_);
}
ID3D12GraphicsCommandList* DirectXCommon::GetCommandList() {
    return commandManager_->GetCommandList();
}

ID3D12Fence* DirectXCommon::GetFence() {
    return commandManager_->GetFence();
}
HANDLE DirectXCommon::GetFenceEvent() {
    return commandManager_->GetFenceEvent();
}

uint64_t& DirectXCommon::GetFenceValue() {
    return commandManager_->GetFenceValue(frameIndex_);
}
uint64_t DirectXCommon::GetFenceValue(uint32_t index) const {
    return commandManager_->GetFenceValue(index);
}
uint64_t DirectXCommon::GetGlobalFenceValue() const {
    return commandManager_->GetGlobalFenceValue();
}
uint64_t DirectXCommon::IncrementGlobalFence() {
    return commandManager_->IncrementGlobalFence();
}
uint64_t DirectXCommon::GetCurrentFrameFenceValue() const {
    return commandManager_->GetGlobalFenceValue() + 1;
}

void DirectXCommon::WaitForGPU() {
    if (commandManager_) {
        commandManager_->WaitForGPU();
    }
}

void DirectXCommon::Finalize() {

    // GPU同期 (全フレームの完了を待機)
    if (commandManager_) {
        commandManager_->Finalize();
        commandManager_.reset();
    }

    // 全てのGPU処理が完了しているので、解放待ちのリソースを直ちに破棄する
    pendingResources_.clear();

    // PSO キャッシュを解放(PSO/RSの参照を切る)
    if (psoManager_) {
        psoManager_->ClearCache();
        psoManager_.reset();
    }

    // D3D12解放順: PSO/RootSig→DSV/RTV/SRV→バッファ→コマンド系→フェンス→SwapChain→Device

    if (rootSignatureManager_) {
        rootSignatureManager_->Finalize();
        rootSignatureManager_.reset();
    }
    if (swapChainManager_) {
        swapChainManager_->Finalize();
        swapChainManager_.reset();
    }
    srvPool_.reset();

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    // リーク警告(LIVE_DEVICE等)で強制終了して詳細ログが見れなくなるのを防ぐため、Warningブレークを無効化する
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        }
    }
#endif

    device_.Reset();

    // DXGI ファクトリとデバッグコントローラの解放
    dxgiFactory_.Reset();
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    if (debugController_) {
        debugController_.Reset();
    }

    // 最後に詳細なリークレポートを出力
    Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    }
#endif

    if (hwnd_) {
        hwnd_ = nullptr;
    }
}

DirectXCommon::DirectXCommon() = default;
DirectXCommon::~DirectXCommon() = default;

void DirectXCommon::Initialize(HWND hwnd, int32_t w, int32_t h) {
    hwnd_ = hwnd;
    clientWidth_ = w;
    clientHeight_ = h;

    // 制御用クラスの生成
    fpsController_ = std::make_unique<FrameRateController>();
    shaderManager_ = std::make_unique<ShaderManager>();

    InitializeFixFPS();
    shaderManager_->Initialize();

    // シェーダーの検索パスとバイナリパスを登録
    shaderManager_->AddSearchPath(ConvertString(FileSystem::GetResourcePath("shaders") + "/"));
    shaderManager_->AddSearchPath(ConvertString(FileSystem::GetEngineRoot() + "/EngineResources/shaders/"));
#ifdef NDEBUG
    shaderManager_->SetBinaryPath(ConvertString(FileSystem::GetResourcePath("shaders/compiled") + "/"));
#else
    shaderManager_->SetBinaryPath(ConvertString(FileSystem::GetResourcePath(".cache/shaders") + "/"));
#endif

    EnableDebugLayer();
    InitializeDXGI();
    CreateDevice();
    SetInfoQueue();

    commandManager_ = std::make_unique<DXCommandManager>();
    commandManager_->Initialize(device_.Get());

    swapChainManager_ = std::make_unique<DXSwapChainManager>();
    swapChainManager_->Initialize(device_.Get(), dxgiFactory_.Get(), commandManager_->GetCommandQueue(), hwnd_,
                                  clientWidth_, clientHeight_);

    srvPool_ = std::make_unique<DescriptorPool>();
    srvPool_->Initialize(device_.Get());
    rootSignatureManager_ = std::make_unique<DXRootSignatureManager>();
    rootSignatureManager_->Initialize(device_.Get());
    CreatePSOs();

    CreateDepthSRV();

    // GpuProfilerの初期化
    gpuProfiler_ = std::make_unique<GpuProfiler>();
    gpuProfiler_->Initialize(this);
}

void DirectXCommon::CreateDepthSRV() {
    if (depthSRVIndex_ == DescriptorPool::kInvalid) {
        depthSRVIndex_ = srvPool_->Allocate();
    }
    srvPool_->CreateSRVForTexture2D(depthSRVIndex_, swapChainManager_->GetDepthStencilResource(),
                                    DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
}

void DirectXCommon::EnableDebugLayer() {
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController_.GetAddressOf())))) {
        // デバッグレイヤーを有効化する
        debugController_->EnableDebugLayer();
        // さらにGPU側でもチェックを行うようにする
        // (Bindless環境でDescriptorTable全体を検査してしまい誤検知クラッシュするため無効化)
        // debugController_->SetEnableGPUBasedValidation(TRUE);
    }
#endif
}

void DirectXCommon::InitializeDXGI() {
    // DXGIFactoryの生成
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
}

void DirectXCommon::CreateDevice() {
    /// 使用するアダプタ(GPU)を決定する
    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    for (UINT i = 0;
         dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                  IID_PPV_ARGS(useAdapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND;
         i++) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        HRESULT hr = useAdapter->GetDesc3(&adapterDesc);
        ASSERT_IF_FAILED(hr);
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // std::format
            // が環境によって不安定な可能性があるため、wstringstream等で代用するか、ワイド文字版が正しく動作することを確認
            std::wstring adapterName = adapterDesc.Description;
            Log::OutPutLog(log_->GetLogStream(), "use Adapter:" + ConvertString(adapterName) + "\n");
            break;
        }
        useAdapter = nullptr;
    }
    IRUFEMI_ASSERT(useAdapter != nullptr);

    /// D3D12Deviceの生成
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
    const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        HRESULT hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(device_.GetAddressOf()));
        if (SUCCEEDED(hr)) {
            Log::OutPutLog(log_->GetLogStream(), std::string("FeatureLevel : ") + featureLevelStrings[i] + "\n");
            break;
        }
    }
    IRUFEMI_ASSERT(device_ != nullptr);
    Log::OutPutLog(log_->GetLogStream(), "Complete create D3D12Device!!!\n");

    if (useAdapter) {
        useAdapter.Reset();
    }
}

void DirectXCommon::SetInfoQueue() {
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        D3D12_MESSAGE_ID denyIds[] = {D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
                                      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                                      D3D12_MESSAGE_ID_CREATEPIPELINESTATE_CACHEDBLOBDRIVERVERSIONMISMATCH,
                                      D3D12_MESSAGE_ID_CREATEPIPELINESTATE_CACHEDBLOBDESCMISMATCH};
        D3D12_MESSAGE_SEVERITY severties[] = {D3D12_MESSAGE_SEVERITY_INFO};
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severties);
        filter.DenyList.pSeverityList = severties;
        infoQueue->PushStorageFilter(&filter);
        infoQueue.Reset();
    }
#endif
}

void DirectXCommon::CreatePSOs() {
    // --- 入力レイアウト定義 ---
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"INDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 1, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    // --- PSOManagerの初期化 ---
    psoManager_ = std::make_unique<PSOManager>();
    psoManager_->Initialize(device_.Get(), GetRootSignature(), {inputElementDescs, _countof(inputElementDescs)},
                            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT,
                            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    // --- シェーダコンパイルと登録 ---
    RegisterAllShaders();
}

void DirectXCommon::RegisterAllShaders() {
    // --- シェーダコンパイル設定 ---
    ShaderCompileOptions options;
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    options.isDebug = true;
#endif

    // --- シェーダコンパイル ---
    auto vs3d = shaderManager_->GetOrCompile(L"Object3d.VS.hlsl", options);
    auto ps3d = shaderManager_->GetOrCompile(L"Object3d.PS.hlsl", options);
    auto vsParticle = shaderManager_->GetOrCompile(L"Particle.VS.hlsl", options);
    auto psParticle = shaderManager_->GetOrCompile(L"Particle.PS.hlsl", options);
    auto vsSprite = shaderManager_->GetOrCompile(L"Object2D.VS.hlsl", options);
    auto vsSpriteBatch = shaderManager_->GetOrCompile(L"SpriteBatch.VS.hlsl", options);
    auto psSprite = shaderManager_->GetOrCompile(L"Object2D.PS.hlsl", options);
    auto vsText = shaderManager_->GetOrCompile(L"Text.VS.hlsl", options);
    auto psText = shaderManager_->GetOrCompile(L"Text.PS.hlsl", options);
    auto vsBatch = shaderManager_->GetOrCompile(L"Batch.VS.hlsl", options);
    auto vsLine = shaderManager_->GetOrCompile(L"Line.VS.hlsl", options);
    auto psLine = shaderManager_->GetOrCompile(L"Line.PS.hlsl", options);
    auto vsLineBatch = shaderManager_->GetOrCompile(L"LineBatch.VS.hlsl", options);
    auto psLineBatch = shaderManager_->GetOrCompile(L"LineBatch.PS.hlsl", options);
    auto vsDebugPrimitive = shaderManager_->GetOrCompile(L"DebugPrimitive.VS.hlsl", options);
    auto psDebugPrimitive = shaderManager_->GetOrCompile(L"DebugPrimitive.PS.hlsl", options);
    auto vsSkin = shaderManager_->GetOrCompile(L"SkinningObject3D.VS.hlsl", options);
    auto vsSkybox = shaderManager_->GetOrCompile(L"Skybox.VS.hlsl", options);
    auto psSkybox = shaderManager_->GetOrCompile(L"Skybox.PS.hlsl", options);
    auto vsSkydome = shaderManager_->GetOrCompile(L"Skydome.VS.hlsl", options);
    auto psSkydome = shaderManager_->GetOrCompile(L"Skydome.PS.hlsl", options);
    auto vsGpuParticle = shaderManager_->GetOrCompile(L"ParticleGPU.VS.hlsl", options);
    auto psGpuParticle = shaderManager_->GetOrCompile(L"ParticleGPU.PS.hlsl", options);

    auto vsVoxel = shaderManager_->GetOrCompile(L"VoxelParticle.VS.hlsl", options);
    auto psVoxel = shaderManager_->GetOrCompile(L"VoxelParticle.PS.hlsl", options);
    auto vsShadow = shaderManager_->GetOrCompile(L"ShadowMap.VS.hlsl", options);
    auto vsShadowSkin = shaderManager_->GetOrCompile(L"ShadowMapSkinning.VS.hlsl", options);
    auto vsShadowBatch = shaderManager_->GetOrCompile(L"ShadowMapBatch.VS.hlsl", options);
    auto psAOEWarning = shaderManager_->GetOrCompile(L"AOEWarning.PS.hlsl", options);

    auto vsFullscreen = shaderManager_->GetOrCompile(L"Fullscreen.VS.hlsl", options);
    auto psFog = shaderManager_->GetOrCompile(L"Fog.PS.hlsl", options);

#ifdef EditorMode
    auto vsSelection = shaderManager_->GetOrCompile(L"SelectionMask.VS.hlsl", options);
    auto psSelection = shaderManager_->GetOrCompile(L"SelectionMask.PS.hlsl", options);
    auto psSelectionText = shaderManager_->GetOrCompile(L"SelectionMaskText.PS.hlsl", options);
    auto psOutlineComp = shaderManager_->GetOrCompile(L"OutlineComposite.PS.hlsl", options);
#endif

    auto csSkin = shaderManager_->GetOrCompile(L"Skinning.CS.hlsl", options);
    auto csGpuInit = shaderManager_->GetOrCompile(L"InitializeParticle.CS.hlsl", options);
    auto csGpuEmit = shaderManager_->GetOrCompile(L"EmitParticle.CS.hlsl", options);
    auto csGpuUpdate = shaderManager_->GetOrCompile(L"UpdateParticle.CS.hlsl", options);
    auto csGpuInitSort = shaderManager_->GetOrCompile(L"InitParticleSort.CS.hlsl", options);
    auto csGpuBitonicSort = shaderManager_->GetOrCompile(L"BitonicSort.CS.hlsl", options);
    auto csVoxelInit = shaderManager_->GetOrCompile(L"InitializeVoxel.CS.hlsl", options);
    auto csVoxelEmit = shaderManager_->GetOrCompile(L"EmitVoxel.CS.hlsl", options);
    auto csVoxelUpdate = shaderManager_->GetOrCompile(L"UpdateVoxel.CS.hlsl", options);
    auto csGpuCulling = shaderManager_->GetOrCompile(L"GPUCulling.CS.hlsl", options);

    // --- 各種シェーダの登録 ---
    // --- MRT共通設定 (フルG-Buffer対応) ---
    PSOManager::PipelineStateDesc mrtDesc{};
    mrtDesc.numRenderTargets = 5;
    mrtDesc.rtvFormat1 = DXGI_FORMAT_R8G8B8A8_UNORM;     // Mask
    mrtDesc.rtvFormat2 = DXGI_FORMAT_R16G16B16A16_FLOAT; // Normal & Depth
    mrtDesc.rtvFormat3 = DXGI_FORMAT_R8G8B8A8_UNORM;     // Material
    mrtDesc.rtvFormat4 = DXGI_FORMAT_R16G16_FLOAT;       // Velocity

    PSOManager::PipelineStateDesc obj3dDesc = mrtDesc;
    obj3dDesc.shaders = {vs3d, ps3d};
    psoManager_->RegisterShader("Object3D", obj3dDesc);

    PSOManager::PipelineStateDesc batchDesc = mrtDesc;
    batchDesc.shaders = {vsBatch, ps3d};
    psoManager_->RegisterShader("Batch", batchDesc); // ps3dを共有

    // 2D/UI系 (MRT非対応・1枚)
    psoManager_->RegisterShader("Sprite", {{vsSprite, psSprite}});
    psoManager_->RegisterShader("SpriteBatch", {{vsSpriteBatch, psSprite}});
    psoManager_->RegisterShader("Text", {{vsText, psText}});

    // 3Dエフェクト系 (MRT対応)
    PSOManager::PipelineStateDesc particleDesc = mrtDesc;
    particleDesc.shaders = {vsParticle, psParticle};
    psoManager_->RegisterShader("Particle", particleDesc);

    PSOManager::PipelineStateDesc lineDesc = mrtDesc;
    lineDesc.shaders = {vsLine, psLine};
    lineDesc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoManager_->RegisterShader("Line", lineDesc);

    PSOManager::PipelineStateDesc lineBatchDesc = mrtDesc;
    lineBatchDesc.shaders = {vsLineBatch, psLineBatch};
    lineBatchDesc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoManager_->RegisterShader("LineBatch", lineBatchDesc);

    PSOManager::PipelineStateDesc debugPrimDesc = mrtDesc;
    debugPrimDesc.shaders = {vsDebugPrimitive, psDebugPrimitive};
    debugPrimDesc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoManager_->RegisterShader("DebugPrimitive", debugPrimDesc);

    PSOManager::PipelineStateDesc skinDesc = mrtDesc;
    skinDesc.shaders = {vsSkin, ps3d};
    psoManager_->RegisterShader("Skinning", skinDesc);

    PSOManager::PipelineStateDesc skyboxDesc = mrtDesc;
    skyboxDesc.shaders = {vsSkybox, psSkybox};
    psoManager_->RegisterShader("Skybox", skyboxDesc);

    PSOManager::PipelineStateDesc skydomeDesc = mrtDesc;
    skydomeDesc.shaders = {vsSkydome, psSkydome};
    skydomeDesc.useNullInputLayout = true; // 頂点バッファなし(SV_VertexID)
    psoManager_->RegisterShader("Skydome", skydomeDesc);

    PSOManager::PipelineStateDesc gpuParticleDesc = mrtDesc;
    gpuParticleDesc.shaders = {vsGpuParticle, psGpuParticle};
    psoManager_->RegisterShader("GpuParticle", gpuParticleDesc);

    PSOManager::PipelineStateDesc voxelParticleDesc = mrtDesc;
    voxelParticleDesc.shaders = {vsVoxel, psVoxel};
    psoManager_->RegisterShader("VoxelParticle", voxelParticleDesc);
    psoManager_->RegisterShader("AOEWarning", {{vs3d, psAOEWarning}});

    // シャドウマップ(通常) - 深度のみ
    PSOManager::PipelineStateDesc shadowDesc{};
    shadowDesc.shaders = {vsShadow, nullptr};
    shadowDesc.isDepthOnly = true;
    psoManager_->RegisterShader("Shadow", shadowDesc);

    // シャドウマップ(スキニング) - 深度のみ
    PSOManager::PipelineStateDesc shadowSkinDesc{};
    shadowSkinDesc.shaders = {vsShadowSkin, nullptr};
    shadowSkinDesc.isDepthOnly = true;
    psoManager_->RegisterShader("ShadowSkinning", shadowSkinDesc);

    // シャドウマップ(バッチ) - 深度のみ
    PSOManager::PipelineStateDesc shadowBatchDesc{};
    shadowBatchDesc.shaders = {vsShadowBatch, nullptr};
    shadowBatchDesc.isDepthOnly = true;
    psoManager_->RegisterShader("ShadowBatch", shadowBatchDesc);

#ifdef EditorMode
    PSOManager::PipelineStateDesc maskDesc{};
    maskDesc.shaders = {vsSelection, psSelection};
    maskDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    maskDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
    psoManager_->RegisterShader("SelectionMask", maskDesc);

    // SelectionMaskText
    PSOManager::PipelineStateDesc maskTextDesc = maskDesc;
    maskTextDesc.shaders.vsBlob = vsText;
    maskTextDesc.shaders.psBlob = psSelectionText;
    psoManager_->RegisterShader("SelectionMaskText", maskTextDesc);

    PSOManager::PipelineStateDesc outlineCompDesc{};
    outlineCompDesc.shaders = {vsFullscreen, psOutlineComp};
    outlineCompDesc.disableDepthTest = true;
    outlineCompDesc.useNullInputLayout = true;
    outlineCompDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    outlineCompDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
    psoManager_->RegisterShader("OutlineComposite", outlineCompDesc);
#endif

    // 大気・距離フォグ (全画面ブレンド)
    PSOManager::PipelineStateDesc fogDesc{};
    fogDesc.shaders = {vsFullscreen, psFog};
    fogDesc.disableDepthTest = true;
    fogDesc.noDSV = true;
    fogDesc.useNullInputLayout = true;
    fogDesc.rtvFormat = mrtDesc.rtvFormat;
    fogDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
    psoManager_->RegisterShader("Fog", fogDesc);

    // バックバッファ書き込み用のスプライト設定
    PSOManager::PipelineStateDesc spriteBBDesc{};
    spriteBBDesc.shaders = {vsSprite, psSprite};
    spriteBBDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    spriteBBDesc.disableDepthTest = true;
    spriteBBDesc.noDSV = true;
    psoManager_->RegisterShader("SpriteForBackBuffer", spriteBBDesc);

    PSOManager::PipelineStateDesc spriteBatchBBDesc = spriteBBDesc;
    spriteBatchBBDesc.shaders = {vsSpriteBatch, psSprite};
    psoManager_->RegisterShader("SpriteBatchForBackBuffer", spriteBatchBBDesc);

    PSOManager::PipelineStateDesc textBBDesc = spriteBBDesc;
    textBBDesc.shaders = {vsText, psText};
    psoManager_->RegisterShader("TextForBackBuffer", textBBDesc);

    // --- Compute PSO生成 ---
    auto computeRootSig = GetComputeRootSignature();
    psoManager_->RegisterComputeShader("Skinning", csSkin, computeRootSig);
    psoManager_->RegisterComputeShader("GpuParticleInitialize", csGpuInit, computeRootSig);
    psoManager_->RegisterComputeShader("GpuParticleEmit", csGpuEmit, computeRootSig);
    psoManager_->RegisterComputeShader("GpuParticleUpdate", csGpuUpdate, computeRootSig);
    psoManager_->RegisterComputeShader("GpuParticleInitSort", csGpuInitSort, computeRootSig);
    psoManager_->RegisterComputeShader("GpuParticleBitonicSort", csGpuBitonicSort, computeRootSig);
    psoManager_->RegisterComputeShader("VoxelParticleInitialize", csVoxelInit, computeRootSig);
    psoManager_->RegisterComputeShader("VoxelParticleEmit", csVoxelEmit, computeRootSig);
    psoManager_->RegisterComputeShader("VoxelParticleUpdate", csVoxelUpdate, computeRootSig);
    psoManager_->RegisterComputeShader("GPUCulling", csGpuCulling, computeRootSig);

    // --- ビューポート・シザー矩形設定 ---
    viewport_.Width = static_cast<FLOAT>(clientWidth_);
    viewport_.Height = static_cast<FLOAT>(clientHeight_);
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.right = clientWidth_;
    scissorRect_.top = 0;
    scissorRect_.bottom = clientHeight_;
}

/*開発用のUIを出そう*/

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags =
        shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    return descriptorHeap;
}

IDXGISwapChain4* DirectXCommon::GetSwapChain() {
    return swapChainManager_->GetSwapChain();
}
ID3D12Resource* DirectXCommon::GetSwapChainResources(UINT index) {
    return swapChainManager_->GetSwapChainResource(index);
}
UINT DirectXCommon::GetCurrentBackBufferIndex() const {
    return swapChainManager_->GetCurrentBackBufferIndex();
}
D3D12_RENDER_TARGET_VIEW_DESC& DirectXCommon::GetRtvDesc() {
    return swapChainManager_->GetRtvDesc();
}
bool DirectXCommon::IsTearingSupported() const {
    return swapChainManager_->IsTearingSupported();
}
ID3D12DescriptorHeap* DirectXCommon::GetDsvDescriptorHeap() {
    return swapChainManager_->GetDSVDescriptorHeap();
}
D3D12_CPU_DESCRIPTOR_HANDLE& DirectXCommon::GetRtvHandles(UINT index) {
    return swapChainManager_->GetRtvHandles(index);
}
ID3D12Resource* DirectXCommon::GetDepthStencilResource() const {
    return swapChainManager_->GetDepthStencilResource();
}
DXGI_SWAP_CHAIN_DESC1& DirectXCommon::GetSwapChainDesc() {
    return swapChainManager_->GetSwapChainDesc();
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVCPUDescriptorHandle(uint32_t index) {
    return swapChainManager_->GetRTVCPUDescriptorHandle(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVGPUDescriptorHandle(uint32_t index) {
    return swapChainManager_->GetRTVGPUDescriptorHandle(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVCPUDescriptorHandle(uint32_t index) {
    return swapChainManager_->GetDSVCPUDescriptorHandle(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVGPUDescriptorHandle(uint32_t index) {
    return swapChainManager_->GetDSVGPUDescriptorHandle(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetReadOnlyDSVCPUDescriptorHandle() {
    return swapChainManager_->GetDSVCPUDescriptorHandle(1);
}

uint32_t DirectXCommon::AllocateRTVIndex() {
    return swapChainManager_->AllocateRTVIndex();
}

uint32_t DirectXCommon::AllocateDSVIndex() {
    return swapChainManager_->AllocateDSVIndex();
}

void DirectXCommon::FreeRTVIndex(uint32_t index) {
    swapChainManager_->FreeRTVIndex(index, GetCurrentFrameFenceValue());
}

void DirectXCommon::FreeDSVIndex(uint32_t index) {
    swapChainManager_->FreeDSVIndex(index, GetCurrentFrameFenceValue());
}

/*三角形の色を変えよう*/

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {

    /// BufferResourceを生成する

    // 頂点リソース用のヒープを生成
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う
    // 頂点リソースの設定
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    // バッファリソース、テクスチャの場合はまた別の設定をする
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ。今回はVector4を3頂点分
    // バッファの場合はこれらは1にする決まり
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // 実際に頂点リソースを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(bufferResource.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    return bufferResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                  D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                  IID_PPV_ARGS(bufferResource.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    return bufferResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
                                 const DirectX::ScratchImage& mipImages) {
    // ExecuteUploadCommands 内でロック処理が行われるため、ここでのロックは不要です

    /// IntermediateResource(中間リソース)
    std::vector<D3D12_SUBRESOURCE_DATA> subResources;
    DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(),
                           subResources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subResources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);

    ExecuteUploadCommands([&](ID3D12GraphicsCommandList* cmdList) {
        UpdateSubresources(cmdList, texture.Get(), intermediateResource.Get(), 0, 0, UINT(subResources.size()),
                           subResources.data());

        // Textureへの転送後は利用できるよう、ResourceStateを変更
        DirectXUtils::TransitionBarrier(cmdList, texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                        D3D12_RESOURCE_STATE_GENERIC_READ);
    });

    // 中間リソースを遅延解放に登録
    ReleaseAfterFence(intermediateResource);

    return intermediateResource;
}

/*テクスチャを貼ろう*/

/// DirectX12のTextureResourceを作る

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
    // 1. metadataを基にResourceの設定
    // 2. 利用するHeapの設定
    // 3. Resourceを生成する

    /*テクスチャを正しく配置しよう*/

    /// 正式な手順

    // before
    // 1. TextureデータそのものをCPUに読み込む
    // 2. DirectX12のTextureResourceを作る)(MainMemory)
    // 3. TextureResourceに1で読んだデータを転送する(WriteToSubResource)

    // after
    // 1.Textureデータその物をCPUで読み込む

    // 2. DirectX12TextureResourceを作る(VRAM)
    // 3. CPUに書き込む用にUploadHeapのResourceを作る(IntermediateResource)
    // 4. 3に対してCPUでデータを書き込む
    // 5. CommandListに3を2に転送するコマンドを積む
    // 6. CommandQueueを使って実行する
    // 7. 6の事項完了を待つ

    /*テクスチャを貼ろう*/

    // metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);                // Textureの幅
    resourceDesc.Height = UINT(metadata.height);              // Textureの高さ
    resourceDesc.MipLevels = UINT(metadata.mipLevels);        // mipmapの数
    resourceDesc.DepthOrArraySize = UINT(metadata.arraySize); // 奥行きor 配列Textureの配列数
    resourceDesc.Format = metadata.format;                    // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;                        // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元。

    // 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties{};
    // heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; //細かい設定を行う
    //  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
    // heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; //プロセッサの近くに配置

    /*テクスチャを正しく配置しよう*/

    // TextureResourceを作る(VRAM)

    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    /*テクスチャを貼ろう*/

    // Resourceの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,      // Heapの設定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
        &resourceDesc,        // Resourceの設定
        // D3D12_RESOURCE_STATE_GENERIC_READ, //初回のResourceState。Textureは基本読むだけ。

        /*テクスチャを正しく配置しよう*/

        D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される設定

        /*テクスチャを貼ろう*/

        nullptr,                              // Clear最適値。使わないのでnullptr
        IID_PPV_ARGS(resource.GetAddressOf()) // 作成するResourceポインタへのポインタ
    );
    ASSERT_IF_FAILED(hr);
    return resource;
}

/*テクスチャを貼ろう*/

/// Textureデータを読む

DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath) {
    using namespace DirectX;

    ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);

    // Win32 API を使用してファイルの存在確認を行う（std::filesystem の代用）
    if (GetFileAttributesW(filePathW.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring msg = L"[LoadTexture] File not found: " + filePathW;
        /**
         * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
         */
        Log::OutPutLog(std::cerr, ConvertString(msg));

        // フォールバック: 1x1のマゼンタ色テクスチャを返す
        ScratchImage fallbackImage;
        fallbackImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        uint8_t* pixels = fallbackImage.GetPixels();
        if (pixels) {
            pixels[0] = 255;
            pixels[1] = 0;
            pixels[2] = 255;
            pixels[3] = 255;
        }
        return fallbackImage;
    }

    // --- sRGB 判定ロジック ---
    bool isSRGB = true;
    std::string filePathLower = filePath;
    std::transform(filePathLower.begin(), filePathLower.end(), filePathLower.begin(), ::tolower);
    if (filePathLower.find("_n") != std::string::npos || filePathLower.find("normal") != std::string::npos ||
        filePathLower.find("_ao") != std::string::npos || filePathLower.find("_m") != std::string::npos ||
        filePathLower.find("metallic") != std::string::npos || filePathLower.find("_r") != std::string::npos ||
        filePathLower.find("roughness") != std::string::npos) {
        isSRGB = false;
    }

    HRESULT hr;
    TextureUtility::TextureFileType fileType = TextureUtility::GetTextureFileType(filePathW);

    if (fileType == TextureUtility::TextureFileType::DDS) {
        hr = LoadFromDDSFile(filePathW.c_str(), DDS_FLAGS_NONE, nullptr, image);

        // 8bit 系のフォーマットかつカラーマップ（normal等でない）であれば sRGB 形式へ変更
        if (SUCCEEDED(hr) && isSRGB) {
            const auto& metadata = image.GetMetadata();
            if (DirectX::FormatDataType(metadata.format) != DirectX::FORMAT_TYPE_FLOAT) {
                image.OverrideFormat(DirectX::MakeSRGB(metadata.format));
            }
        }
    } else if (fileType == TextureUtility::TextureFileType::TGA) {
        hr = LoadFromTGAFile(filePathW.c_str(), TGA_FLAGS_NONE, nullptr, image);

        if (SUCCEEDED(hr) && isSRGB) {
            const auto& metadata = image.GetMetadata();
            if (DirectX::FormatDataType(metadata.format) != DirectX::FORMAT_TYPE_FLOAT) {
                image.OverrideFormat(DirectX::MakeSRGB(metadata.format));
            }
        }
    } else {
        WIC_FLAGS wicFlags = isSRGB ? WIC_FLAGS_FORCE_SRGB : WIC_FLAGS_NONE;
        hr = LoadFromWICFile(filePathW.c_str(), wicFlags, nullptr, image);
    }

    if (FAILED(hr)) {
        _com_error err(hr);
        std::wstring msg = L"[LoadTexture] Load failed (" + std::to_wstring(hr) + L"): " + filePathW + L" - ";
#ifdef UNICODE
        msg += err.ErrorMessage();
#else
        msg += ConvertString(err.ErrorMessage());
#endif
        /**
         * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
         */
        Log::OutPutLog(std::cerr, ConvertString(msg));

        // フォールバック: 1x1のマゼンタ色テクスチャを返す
        ScratchImage fallbackImage;
        fallbackImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        uint8_t* pixels = fallbackImage.GetPixels();
        if (pixels) {
            pixels[0] = 255;
            pixels[1] = 0;
            pixels[2] = 255;
            pixels[3] = 255;
        }
        return fallbackImage;
    }

    ScratchImage mipImages{};
    if (IsCompressed(image.GetMetadata().format)) {
        mipImages = std::move(image);
    } else {
        TEX_FILTER_FLAGS filter = isSRGB ? TEX_FILTER_SRGB : TEX_FILTER_DEFAULT;
        hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), filter, 0, mipImages);
        if (FAILED(hr)) {
            _com_error err(hr);
            std::wstring msg =
                L"[LoadTexture] GenerateMipMaps failed (" + std::to_wstring(static_cast<unsigned long>(hr)) + L")";
            /**
             * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
             */
            Log::OutPutLog(std::cerr, ConvertString(msg));
            // ミップマップ生成に失敗した場合は、元の画像をそのまま返す
            return image;
        }
    }

    return mipImages;
}

DirectX::TexMetadata DirectXCommon::GetTextureMetadata(const std::string& filePath) {
    using namespace DirectX;
    std::wstring filePathW = ConvertString(filePath);
    TexMetadata metadata{};
    TextureUtility::TextureFileType fileType = TextureUtility::GetTextureFileType(filePathW);
    if (fileType == TextureUtility::TextureFileType::DDS) {
        GetMetadataFromDDSFile(filePathW.c_str(), DDS_FLAGS_NONE, metadata);
    } else if (fileType == TextureUtility::TextureFileType::TGA) {
        GetMetadataFromTGAFile(filePathW.c_str(), TGA_FLAGS_NONE, metadata);
    } else {
        GetMetadataFromWICFile(filePathW.c_str(), WIC_FLAGS_NONE, metadata);
    }
    return metadata;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width,
                                                 int32_t height) {

    /// Resource/Heapの設定を行う

    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;        // Textureの幅
    resourceDesc.Height = height;      // Textureの高さ
    resourceDesc.MipLevels = 1;        // mipmapの数
    resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // 深度バッファとして使いつつ、SRVで読み込むためにTYPELESSにする
    resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

    /// 深度地のクリア最適化設定

    // 深度地のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;              // 1.0f(最大値)でクリア
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // クリアには実際の深度フォーマットを指定

    /// Resourceの生成

    // Resourceの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr =
        device->CreateCommittedResource(&heapProperties,                  // Heapの設定
                                        D3D12_HEAP_FLAG_NONE,             // Heapの特殊な設定。特になし。
                                        &resourceDesc,                    // Resourceの設定
                                        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
                                        &depthClearValue,                 // Clear最適値
                                        IID_PPV_ARGS(resource.GetAddressOf()) // 作成するResourceポインタへのポインタ
        );
    ASSERT_IF_FAILED(hr);

    return resource;
}

UINT DirectXCommon::GetBackBufferIndex(const Microsoft::WRL::ComPtr<IDXGISwapChain4>& swapChain) {
    IRUFEMI_ASSERT(swapChain != nullptr);
    return swapChain->GetCurrentBackBufferIndex();
}

void DirectXCommon::PreWarmJITCompile() {
    if (!commandManager_) {
        return;
    }

    commandManager_->ExecuteUploadCommands([&](ID3D12GraphicsCommandList* uploadCommandList) {
        // --- Compute PSO ---
        uploadCommandList->SetComputeRootSignature(GetComputeRootSignature());

        // SetPipelineState とダミー Dispatch(0,0,0) を発行し、
        // NVIDIAやIntelのドライバに対し、最初のDraw()より前に強制的に
        // ハードウェア専用のISA（機械語）へJITコンパイルさせる
        const char* csNames[] = {"Skinning",
                                 "GpuParticleInitialize",
                                 "GpuParticleEmit",
                                 "GpuParticleUpdate",
                                 "GpuParticleInitSort",
                                 "GpuParticleBitonicSort",
                                 "VoxelParticleInitialize",
                                 "VoxelParticleEmit",
                                 "VoxelParticleUpdate"};
        for (const char* name : csNames) {
            if (auto pso = psoManager_->GetComputePSO(name)) {
                uploadCommandList->SetPipelineState(pso);
            }
        }

        // --- Graphics PSO (重いもの) ---
        uploadCommandList->SetGraphicsRootSignature(GetRootSignature());
        uploadCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (psoManager_) {
            // 例: 高負荷な特殊パイプライン(電撃エフェクト等)
        }
    });
}

void DirectXCommon::ExecuteUploadCommands(std::function<void(ID3D12GraphicsCommandList*)> commands) {
    if (commandManager_) {
        commandManager_->ExecuteUploadCommands(commands);
    }
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
DirectXCommon::CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
                                    D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags =
        shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t width, uint32_t height,
                                           DXGI_FORMAT format, const Irufemi::Vector4* clearColor) {
    // 1. metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;                                   // Textureの幅
    resourceDesc.Height = height;                                 // Textureの高さ
    resourceDesc.MipLevels = 1;                                   // mipmapの数
    resourceDesc.DepthOrArraySize = 1;                            // 奥行きor 配列Textureの配列数
    resourceDesc.Format = format;                                 // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // Textureの次元数。
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // RenderTargetとして利用可能にする

    // 2. 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 当然VRAM上に作る

    // 3. ClearValueの設定
    D3D12_CLEAR_VALUE clearValue{};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    if (clearColor) {
        clearValue.Format = format;
        clearValue.Color[0] = clearColor->x;
        clearValue.Color[1] = clearColor->y;
        clearValue.Color[2] = clearColor->z;
        clearValue.Color[3] = clearColor->w;
        pClearValue = &clearValue;
    }

    // 4. Resourceを生成する
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,                            // Heapの設定
        D3D12_HEAP_FLAG_NONE,                       // Heapの特殊な設定。
        &resourceDesc,                              // Resourceの設定
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 最初はSRVとして扱える状態で生成する
        pClearValue,                                // Clear最適値。指定がなければnullptr
        IID_PPV_ARGS(resource.GetAddressOf())       // 作成するResourceポインタへのポインタ
    );
    ASSERT_IF_FAILED(hr);
    return resource;
}

void DirectXCommon::ResizeSwapChain(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    // 1. GPUの完了を待つ (Flush)
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        WaitForGPU();
    }

    clientWidth_ = width;
    clientHeight_ = height;

    // DXSwapChainManager 側でバッファ再構築
    swapChainManager_->ResizeSwapChain(device_.Get(), width, height);
    CreateDepthSRV(); // ウィンドウリサイズに伴う深度SRVの再生成

    // ビューポートとシザーレクトの更新
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = width;
    scissorRect_.bottom = height;
}

void DirectXCommon::ReleaseAfterFence(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
    if (!resource) {
        return;
    }
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pendingResources_.push_back({commandManager_->GetGlobalFenceValue() + 1, std::move(resource)});
}

void DirectXCommon::ClearPendingResources() {
    uint64_t completed = commandManager_->GetFence()->GetCompletedValue();
    std::lock_guard<std::mutex> lock(pendingMutex_);

    // リソースの回収 (C++20 std::erase_if)
    std::erase_if(pendingResources_, [completed](const PendingResource& res) { return res.fenceValue <= completed; });

    // デスクリプタの回収をマネージャに委譲
    swapChainManager_->FlushPendingDescriptors(completed);
}

void DirectXCommon::EnqueueSRVUpdate(const Microsoft::WRL::ComPtr<ID3D12Resource>& textureResource,
                                     const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
                                     D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU) {
    std::lock_guard<std::mutex> lock(pendingSRVMutex_);
    pendingSRVUpdates_.push_back({textureResource, srvDesc, textureSrvHandleCPU});
}

void DirectXCommon::FlushPendingSRVUpdates() {
    std::lock_guard<std::mutex> lock(pendingSRVMutex_);
    for (const auto& update : pendingSRVUpdates_) {
        device_->CreateShaderResourceView(update.textureResource.Get(), &update.srvDesc, update.textureSrvHandleCPU);
    }
    pendingSRVUpdates_.clear();
}
