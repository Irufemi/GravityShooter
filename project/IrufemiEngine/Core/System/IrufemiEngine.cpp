#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/FileSystem.h"
#include "Core/System/IEngineExtension.h"
#include "Core/System/DirectoryWatcher.h"
#include "Core/Math/Math.h"
#include "Core/Math/Random/Random.h"
#include "Core/Profiler/TelemetrySender.h"
#include "Core/Profiler/GpuProfiler.h"
#include "Framework/Utility/CVar.h"

// 標準 / OS ライブラリ
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <DbgHelp.h>
#include <mmsystem.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "winmm.lib")

// プラットフォーム & RHI
#include "Platform/Input/InputManager.h"
#include "Platform/WindowsAPI/WinApp.h"
#include "RHI/DirectX12/DirectXUtils.h"

// サブシステムマネージャー
#include "Renderer/DrawManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Font/FontManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioPlayer.h"
#include "Physics/CollisionManager.h"

// レンダリングリソース & オブジェクト
#include "Renderer/Data/VertexData.h"
#include "Renderer/System/Data/RenderData.h"
#include "Renderer/System/Core/BaseResource.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Renderer/System/Core/LineResource.h"
#include "Renderer/System/Core/Object2DResource.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Renderer/Object/PrimitiveManager.h"
#include "Renderer/Object/Effect/Effect.h"
#include "Renderer/Object/Line/LineClass.h"
#include "Renderer/Object/Skybox/Skybox.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include "Renderer/Object/2D/SpriteBatch/SpriteBatch.h"
#include "Renderer/Object/2D/Text/Text.h"
#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "Renderer/Object/Batch/ModelBatch.h"
#include "Renderer/Object/Batch/PrimitiveBatch.h"

// パーティクルシステム
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/System/ParticleGPU/GPUParticleSystem.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/System/VoxelParticle/VoxelParticleSystem.h"

// フレームワーク & シーン
#include "Framework/UI/DebugUI.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneTransition.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Prefab/PrefabManager.h"
#include "Framework/Component/ComponentFactory.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"

namespace Irufemi {
extern void ReferenceEngineCVars();
}

namespace {
static float s_gpuWaitTimeMs = 0.0f;
}

IrufemiEngine::IrufemiEngine() = default;

// デストラクタ
IrufemiEngine::~IrufemiEngine() {
    Finalize();
}

// 初期化
void IrufemiEngine::Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight) {
    // パス解決機能の初期化 (一番最初に呼ぶ)
    FileSystem::Initialize();

    // OSタイマー精度を1ミリ秒に引き上げる（フレームペーシングの安定化）
    timeBeginPeriod(1);

    /*CrashHandler*/
    SetUnhandledExceptionFilter(WinApp::ExportDump);

    // コンポーネント・ファクトリ登録
    ComponentFactory::RegisterAllCoreComponents();

    // 時間計測の開始
    startTime_ = std::chrono::steady_clock::now();
    lastFrameTime_ = startTime_;

    // WinApp をエンジン内で生成・初期化(COM 初期化もここで実施される)
    winApp_ = std::make_unique<WinApp>();
    if (!winApp_->Initialize(GetModuleHandle(nullptr), clientWidth, clientHeight, title.c_str())) {
        IRUFEMI_ASSERT(false && "WinApp::Initialize failed");
        return;
    }

    // ログを出せるようにする
    log_ = std::make_unique<Log>();
    log_->Initialize();

    // スレッドプールの初期化（ワーカースレッド数の決定）
    // -------------------------------------------------------------------------
    // PCの限界である「全論理コア数」をゲームのワーカースレッドに割り当ててしまうと、
    // 肝心のメインスレッド（ゲームループや描画命令）やOSのバックグラウンド処理（Discord等）
    // が圧迫され、結果的にフレーム落ちや配信カクつきの原因になります。
    // そのため、「全コアから2コアを引いた数（メイン/OS用）」にしつつ、
    // スレッド管理のオーバーヘッドを避けるため最大でも16スレッドまでに制限します。
    // -------------------------------------------------------------------------
    size_t hwThreads = std::thread::hardware_concurrency();
    size_t workerThreads = (hwThreads > 2) ? (hwThreads - 2) : 1; // 最低1スレッドは確保
    workerThreads = (std::min)(workerThreads, static_cast<size_t>(16));

    threadPool_ = std::make_unique<ThreadPool>(workerThreads);

    // ScreenCaptureManagerの生成 (この時点では初期化待ち)
    screenCaptureManager_ = std::make_unique<ScreenCaptureManager>();

    // 乱数エンジンのシードを設定
    Irufemi::Random::SeedEngine();

    // AudioManagerの生成と初期化(Media Foundation含む)
    audioManager_ = std::make_unique<AudioManager>();
    audioManager_->Initialize();
    audioManager_->LoadAllSoundsFromFolder("resources/audio", threadPool_.get());

    // DirectX 基盤
    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->SetLog(log_.get());
    dxCommon_->SetEngine(this);
    dxCommon_->Initialize(winApp_->GetHwnd(), winApp_->GetClientWidth(), winApp_->GetClientHeight());

    BaseResource::SetDirectXCommon(dxCommon_.get());
    BaseBatch::SetDirectXCommon(dxCommon_.get());
    Line3DBatch::SetDirectXCommon(dxCommon_.get());
    SpriteBatch::SetDirectXCommon(dxCommon_.get());

    // ScreenCaptureManagerの初期化
    if (screenCaptureManager_) {
        screenCaptureManager_->Initialize(dxCommon_.get(), threadPool_.get());
    }

    // --- Dynamic Constant Buffer の初期化 ---
    materialBufferManager_ = std::make_unique<DynamicConstantBuffer<Material>>();
    materialBufferManager_->Initialize(dxCommon_.get(),
                                       65536); // 最大6万オブジェクト

    transformBufferManager_ = std::make_unique<DynamicConstantBuffer<TransformationMatrix>>();
    transformBufferManager_->Initialize(dxCommon_.get(),
                                        65536); // 最大6万オブジェクト

    // SRV ディスクリプタプール
    {
        DescriptorPool* srvPool = dxCommon_->GetSrvPool();

        // 注入
        Texture::SetDescriptorPool(srvPool);
        BaseBatch::SetSrvAllocator(srvPool);
        SpriteBatch::SetSrvAllocator(srvPool);
        Line3DBatch::SetSrvAllocator(srvPool);
    }

    // テクスチャ管理
    textureManager_ = std::make_unique<TextureManager>();
    textureManager_->Initialize(dxCommon_.get());

#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
    textureManager_->LoadAllFromFolder("resources/");
#endif

    // フォント管理
    fontManager_ = std::make_unique<FontManager>();
    fontManager_->Initialize(this);
    Text::SetFontManager(fontManager_.get());

    // resources/fonts/ 以下のフォントをすべて自動ロード
    fontManager_->LoadAllFromFolder("resources/fonts/");

    // モデル管理
    modelManager_ = std::make_unique<ModelManager>();
    modelManager_->Initialize(dxCommon_.get(),
                              textureManager_.get()); // dxCommon を渡す

    ModelBatch::SetModelManager(modelManager_.get());

    // プリミティブ管理(シングルトン)の初期化
    primitiveManager_ = std::make_unique<PrimitiveManager>();
    PrimitiveBatch::SetPrimitiveManager(primitiveManager_.get());
    MeshDesc::SetPrimitiveManager(primitiveManager_.get());

    // 既存SRVの走査で free-list 再構築
    {
        DescriptorPool* srvPool = dxCommon_->GetSrvPool();
        ID3D12DescriptorHeap* srvHeap = srvPool->GetHeap();
        const uint32_t inc =
            dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto toIndex = [&](D3D12_GPU_DESCRIPTOR_HANDLE h) -> uint32_t {
            if (h.ptr == 0) {
                return DescriptorPool::kInvalid;
            }
            const auto heapStart = srvHeap->GetGPUDescriptorHandleForHeapStart().ptr;
            const uint64_t diff = (h.ptr - heapStart);
            return static_cast<uint32_t>(diff / inc);
        };

        std::vector<uint32_t> used;
        // 白テクスチャ
        if (auto white = textureManager_->GetWhiteTextureHandle(); white.ptr != 0) {
            if (auto idx = toIndex(white); idx != DescriptorPool::kInvalid) {
                used.push_back(idx);
            }
        }
        // 白CubeMap
        if (auto whiteCube = textureManager_->GetWhiteCubeMapHandle(); whiteCube.ptr != 0) {
            if (auto idx = toIndex(whiteCube); idx != DescriptorPool::kInvalid) {
                used.push_back(idx);
            }
        }
        // フォントアトラスはTextureManagerに統合されたため、下の GetAllAllocatedSrvHandles に含まれる
        // 深度バッファ SRV
        if (auto idx = toIndex(dxCommon_->GetDepthSRVGPUHandle()); idx != DescriptorPool::kInvalid) {
            used.push_back(idx);
        }
        // テクスチャキャッシュ
        for (auto h : textureManager_->GetAllAllocatedSrvHandles()) {
            if (auto idx = toIndex(h); idx != DescriptorPool::kInvalid) {
                used.push_back(idx);
            }
        }
        for (uint32_t i = 0; i < srvPool->BaseIndex(); ++i) {
            used.push_back(i);
        }

        std::sort(used.begin(), used.end());
        used.erase(std::unique(used.begin(), used.end()), used.end());

        srvPool->RebuildFreeListExcept(used);
    }

    // 入力
    inputManager_ = std::make_unique<InputManager>();
    inputManager_->Initialize(winApp_->GetHwnd());
    inputManager_->SetGameResolution(static_cast<float>(gameResWidth_), static_cast<float>(gameResHeight_));
    winApp_->SetInputManager(inputManager_.get());

    // UI
    ui_ = std::make_unique<DebugUI>();
    ui_->Initialize(dxCommon_->GetHwnd(), dxCommon_.get());

    for (auto& ext : extensions_) {
        ext->OnInitialize(this);
    }
    Sprite::SetDebugUI(ui_.get());
    Text::SetDebugUI(ui_.get());
    Primitive2DObject::SetDebugUI(ui_.get());

    Primitive3DObject::SetDebugUI(ui_.get());

    // コリジョン管理の初期化は後続で実行（DebugPrimitiveRenderer生成後）

    // GPUパーティクル管理
    gpuParticleManager_ = std::make_unique<GPUParticleManager>();
    gpuParticleManager_->Initialize();
    ParticleObject::SetGPUParticleManager(gpuParticleManager_.get());
    ParticleObject::SetModelManager(modelManager_.get());

    // 描画
    drawManager_ = std::make_unique<DrawManager>();
    drawManager_->Initialize(this, dxCommon_.get());

    debugPrimitiveRenderer_ = std::make_unique<DebugPrimitiveRenderer>();
    debugPrimitiveRenderer_->Initialize(dxCommon_.get(), drawManager_.get(), dxCommon_->GetSrvPool());

    // コリジョン管理 (デバッグ描画に依存するためここで初期化)
    collisionManager_ = std::make_unique<CollisionManager>();
    collisionManager_->Initialize(debugPrimitiveRenderer_.get());
    ColliderComponent::SetCollisionManager(collisionManager_.get());

    Sprite::SetDrawManager(drawManager_.get());
    Text::SetDrawManager(drawManager_.get());
    Primitive2DObject::SetDrawManager(drawManager_.get());

    BaseBatch::SetDrawManager(drawManager_.get());
    Line3DBatch::SetDrawManager(drawManager_.get());
    SpriteBatch::SetDrawManager(drawManager_.get());

    GPUParticleSystem::SetDrawManager(drawManager_.get());
    Primitive3DObject::SetDrawManager(drawManager_.get());

    GPUParticleSystem::SetEngine(this);
    Line3DBatch::SetDrawManager(drawManager_.get());

    // テクスチャ設定の注入
    ui_->SetTextureManager(textureManager_.get());
    drawManager_->SetTextureManager(textureManager_.get());
    Sprite::SetTextureManager(textureManager_.get());
    Primitive2DObject::SetTextureManager(textureManager_.get());

    BaseBatch::SetTextureManager(textureManager_.get());
    SpriteBatch::SetTextureManager(textureManager_.get());

    GPUParticleSystem::SetTextureManager(textureManager_.get());
    ParticleObject::SetTextureManager(textureManager_.get());
    Primitive3DObject::SetTextureManager(textureManager_.get());

    animationManager_ = std::make_unique<AnimationManager>();
    animationManager_->Initialize(dxCommon_.get());

    BaseModel::SetIrufemiEngine(this);
    winApp_->SetInputManager(inputManager_.get());

    Skybox::SetEngine(this);
    GPUParticleSystem::SetDXCommon(dxCommon_.get());
    VoxelParticleSystem::SetEngine(this);

    voxelParticleManager_ = std::make_unique<VoxelParticleManager>();
    voxelParticleManager_->Initialize(this);

    Primitive2DObject::SetEngine(this);
    Line3DBatch::SetEngine(this);
    Primitive3DObject::SetEngine(this);

    // --- 全画面用 RenderTexture の初期化 ---
    mainRenderTexture_ = std::make_unique<RenderTexture>();
    mainRenderTexture_->Initialize(dxCommon_.get(), GetGameResolutionWidth(), GetGameResolutionHeight(),
                                   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                   {clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]});

    effectMaskTexture_ = std::make_unique<RenderTexture>();
    effectMaskTexture_->Initialize(dxCommon_.get(), GetGameResolutionWidth(), GetGameResolutionHeight(),
                                   DXGI_FORMAT_R8G8B8A8_UNORM, // マスク用はSRGB不要
                                   {0.0f, 0.0f, 0.0f, 0.0f});  // 黒（マスクなし）でクリア

    normalTexture_ = std::make_unique<RenderTexture>();
    normalTexture_->Initialize(dxCommon_.get(), GetGameResolutionWidth(), GetGameResolutionHeight(),
                               DXGI_FORMAT_R16G16B16A16_FLOAT, // 法線用
                               {0.0f, 0.0f, 1.0f, 1.0f});      // 初期値 (0,0,1)

    materialTexture_ = std::make_unique<RenderTexture>();
    materialTexture_->Initialize(dxCommon_.get(), GetGameResolutionWidth(), GetGameResolutionHeight(),
                                 DXGI_FORMAT_R8G8B8A8_UNORM, // マテリアル用 (RGB: 色, A: Roughness or Metallic)
                                 {0.0f, 0.0f, 0.0f, 0.0f});  // 初期値

    velocityTexture_ = std::make_unique<RenderTexture>();
    velocityTexture_->Initialize(dxCommon_.get(), GetGameResolutionWidth(), GetGameResolutionHeight(),
                                 DXGI_FORMAT_R16G16_FLOAT,  // 速度ベクトル用 (X, Y)
                                 {0.0f, 0.0f, 0.0f, 0.0f}); // 初期値

    // --- PostProcessManager の初期化 ---
    postProcessManager_ = std::make_unique<PostProcessManager>();
    postProcessManager_->Initialize(this, dxCommon_.get(), DXGI_FORMAT_R8G8B8A8_UNORM);

    // ノイズテクスチャのロードとハンドル設定
    noise0Handle_ = textureManager_->LoadTexture("resources/noise0.png");
    noise1Handle_ = textureManager_->LoadTexture("resources/noise1.png");
    postProcessManager_->SetDissolveNoiseIndex(0, textureManager_->GetTextureObject(noise0Handle_)->GetSrvIndex());
    postProcessManager_->SetDissolveNoiseIndex(1, textureManager_->GetTextureObject(noise1Handle_)->GetSrvIndex());

    // --- 深度バッファの SRV 作成とマネージャーへの設定 ---
    depthSrvIndex_ = dxCommon_->GetSrvPool()->Allocate();
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU = dxCommon_->GetSrvPool()->GetGPUHandle(depthSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;
    dxCommon_->GetDevice()->CreateShaderResourceView(dxCommon_->GetDepthStencilResource(), &depthSrvDesc,
                                                     dxCommon_->GetSrvPool()->GetCPUHandle(depthSrvIndex_));

    postProcessManager_->SetDepthSrvIndex(depthSrvIndex_);

    if (normalTexture_) {
        postProcessManager_->SetNormalSrvIndex(normalTexture_->GetSrvIndex());
    }
    if (materialTexture_) {
        postProcessManager_->SetMaterialSrvIndex(materialTexture_->GetSrvIndex());
    }
    if (velocityTexture_) {
        postProcessManager_->SetVelocitySrvIndex(velocityTexture_->GetSrvIndex());
    }
    // --- SceneTransition の初期化 ---
    sceneTransition_ = std::make_unique<SceneTransition>();
    sceneTransition_->Initialize(postProcessManager_.get());

    // WinAppに自身(Engine)のポインタを設定
    winApp_->SetEngine(this);

    // PSO（パイプラインステート）の事前コンパイルを実行し、実行中のヒッチ（カクつき）を防止（ThreadPoolによる並列コンパイル）
    if (GetPSOManager()) {
        GetPSOManager()->PreWarmCommonPSOs(threadPool_.get());
    }

    // 初回描画時の遅延ハードウェアコンパイル(JIT)を防止するためのダミー実行
    if (dxCommon_) {
        dxCommon_->PreWarmJITCompile();
    }

    // CameraManager設定
    cameraManager_ = std::make_unique<CameraManager>();
    Sprite::SetCameraManager(cameraManager_.get());
    SpriteBatch::SetCameraManager(cameraManager_.get());
    Text::SetCameraManager(cameraManager_.get());

    // SceneManager 構築(エンジンは所有のみ)
    sceneManager_ = std::make_unique<SceneManager>(this);

#if defined(_DEBUG) || defined(EditorMode)
    // シェーダーのホットリロード監視（別スレッドで動作）
    auto reloadCallback = [this]() { shouldReloadShaders_ = true; };
    shaderWatchers_.push_back(
        std::make_unique<DirectoryWatcher>(FileSystem::GetResourcePath("shaders"), reloadCallback));
    shaderWatchers_.push_back(
        std::make_unique<DirectoryWatcher>(FileSystem::GetEngineRoot() + "/EngineResources/shaders", reloadCallback));
#endif

    // --- CVar (Console Variables) のロードとコールバック設定 ---
    Irufemi::ReferenceEngineCVars();
    Irufemi::CVarSystem::Load("resources/settings.json"); // プロジェクトのプリセット設定
    Irufemi::CVarSystem::Load("resources/settings_local.json"); // ユーザーの変更済みローカル設定（上書き）

    Irufemi::CVarSystem::SetOnChangeCallback("r.DisplayMode", [this]() {
        int mode = Irufemi::CVarSystem::GetInt("r.DisplayMode");
        this->SetDisplayMode(static_cast<DisplayMode>(mode));
    });

    Irufemi::CVarSystem::SetOnChangeCallback("r.VSync", [this]() {
        bool vsync = Irufemi::CVarSystem::GetBool("r.VSync");
        this->SetVSync(vsync);
    });

    // キオスクモード（Windowsキー無効化）のCVarバインドと初期反映
    Irufemi::CVarSystem::SetOnChangeCallback("r.LockWindowsKey", [this]() {
        bool lock = Irufemi::CVarSystem::GetBool("r.LockWindowsKey");
        if (winApp_) {
            winApp_->SetWindowsKeyLock(lock);
        }
    });
    if (!Irufemi::CVarSystem::GetCVar("r.LockWindowsKey")) {
        Irufemi::CVarSystem::RegisterBool("r.LockWindowsKey", true,
                                          "Lock Windows key in kiosk mode (1: lock, 0: unlock)");
    }
    if (winApp_) {
        winApp_->SetWindowsKeyLock(Irufemi::CVarSystem::GetBool("r.LockWindowsKey"));
    }
    // -------------------------------------------------------------
    telemetrySender_ = std::make_unique<TelemetrySender>();
    telemetrySender_->Initialize();

    // TelemetryGatherer の初期化 (ここでプロファイル項目をバインド)
    telemetryGatherer_ = std::make_unique<TelemetryGatherer>();
    telemetryGatherer_->Initialize(this);

    // PrefabManager の初期化と SceneSerializer へのバインド
    prefabManager_ = std::make_unique<PrefabManager>();
    SceneSerializer::SetPrefabManager(prefabManager_.get());
}

// クリアカラーをfloat配列で持つ初期化
void IrufemiEngine::Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                               float r, float g, float b, float a) {
    clearColor_ = {r, g, b, a};
    // 既存の Initialize を呼ぶ(互換性維持)
    Initialize(title, clientWidth, clientHeight);
}

// クリアカラーをstd::arrayで持つ初期化
void IrufemiEngine::Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                               const std::array<float, 4>& clearColor) {
    clearColor_ = clearColor;
    // 既存の Initialize を呼ぶ(互換性維持)
    Initialize(title, clientWidth, clientHeight);
}

// Irufemi::Vector4 版 Initialize
void IrufemiEngine::Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                               const Irufemi::Vector4& clearColor) {
    clearColor_ = {clearColor.x, clearColor.y, clearColor.z, clearColor.w};
    Initialize(title, clientWidth, clientHeight);
}

void IrufemiEngine::Finalize() {
    if (isFinalized_) {
        return;
    }

    // アプリケーション終了時、シーン破棄前にGPU処理の完了を待つ
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }

    // 0. シーンと画面遷移・ローディング（これらがリソースの shared_ptr を保持しているため最優先）
    if (sceneManager_) {
        sceneManager_.reset();
    }

    // シーンシリアライザが保持している静的キャッシュ(プレハブのGameObjectインスタンス)を破棄する
    // これを行わないと、メインスレッド終了後の静的変数破棄の順序(ComponentPoolより後)によって
    // コンポーネントプールの不正アクセス(デストラクタ呼び出し)が発生しクラッシュする
    SceneSerializer::ClearCache();

    if (loadingScreen_) {
        loadingScreen_->Finalize();

        loadingScreen_.reset();
    }
    if (sceneTransition_) {
        sceneTransition_.reset();
    }
    if (cameraManager_) {
        cameraManager_.reset();
    }

    // 1. エディタとUI (描画マネージャ等に依存)
    if (screenCaptureManager_) {
        screenCaptureManager_->Finalize();
        screenCaptureManager_.reset();
    }
    if (ui_) {
        ui_->Shutdown();
        ui_.reset();
    }
    for (auto& ext : extensions_) {
        ext->OnFinalize();
    }
    extensions_.clear();

    // 2. 描画・ポストプロセス系 (DirectX基盤に依存)
    if (debugPrimitiveRenderer_) {
        debugPrimitiveRenderer_.reset();
    }

    if (drawManager_) {
        drawManager_->Finalize();
        drawManager_.reset();
    }
    if (postProcessManager_) {
        postProcessManager_.reset();
    }
    if (mainRenderTexture_) {
        mainRenderTexture_.reset();
    }
    if (effectMaskTexture_) {
        effectMaskTexture_.reset();
    }
    if (normalTexture_) {
        normalTexture_.reset();
    }
    if (materialTexture_) {
        materialTexture_.reset();
    }
    if (velocityTexture_) {
        velocityTexture_.reset();
    }

    // 3. アニメーション・モデル・テクスチャ (リソースの実体を保持)
    if (animationManager_) {
        animationManager_.reset();
    }
    if (modelManager_) {
        modelManager_.reset();
    }
    if (noise0Handle_.IsValid()) {
        textureManager_->ReleaseTexture(noise0Handle_);
    }
    if (noise1Handle_.IsValid()) {
        textureManager_->ReleaseTexture(noise1Handle_);
    }
    if (textureManager_) {
        textureManager_.reset();
    }
    if (fontManager_) {
        fontManager_->Finalize();
        fontManager_.reset();
    }

    // 4. 定数バッファマネージャー (DirectX基盤のリソースを直接保持するため先に破棄)
    if (materialBufferManager_) {
        materialBufferManager_.reset();
    }
    if (transformBufferManager_) {
        transformBufferManager_.reset();
    }

    // --- 静的ポインタのクリア（デストラクタでの不正アクセス防止） ---
    BaseResource::SetDirectXCommon(nullptr);
    BaseBatch::SetDirectXCommon(nullptr);
    Line3DBatch::SetDirectXCommon(nullptr);

    Texture::SetDescriptorPool(nullptr);
    Texture::SetDirectXCommon(nullptr);
    Texture::SetWhiteTextureResource(nullptr);
    GpuMesh::dxCommon_ = nullptr;

    BaseBatch::SetSrvAllocator(nullptr);
    SpriteBatch::SetSrvAllocator(nullptr);
    Line3DBatch::SetSrvAllocator(nullptr);

    // DebugUI, DrawManager, TextureManager 等のクラスへの静的セットもクリア
    Sprite::SetDebugUI(nullptr);
    Primitive2DObject::SetDebugUI(nullptr);
    Primitive3DObject::SetDebugUI(nullptr);

    Sprite::SetDrawManager(nullptr);
    Primitive2DObject::SetDrawManager(nullptr);
    BaseBatch::SetDrawManager(nullptr);
    SpriteBatch::SetDrawManager(nullptr);

    GPUParticleSystem::SetDrawManager(nullptr);
    Primitive3DObject::SetDrawManager(nullptr);
    Line3DBatch::SetDrawManager(nullptr);

    Sprite::SetTextureManager(nullptr);
    Primitive2DObject::SetTextureManager(nullptr);
    BaseBatch::SetTextureManager(nullptr);
    SpriteBatch::SetTextureManager(nullptr);

    GPUParticleSystem::SetTextureManager(nullptr);
    ParticleObject::SetTextureManager(nullptr);
    Primitive3DObject::SetTextureManager(nullptr);

    PrimitiveBatch::SetPrimitiveManager(nullptr);
    MeshDesc::SetPrimitiveManager(nullptr);

    Sprite::SetCameraManager(nullptr);
    SpriteBatch::SetCameraManager(nullptr);
    ModelBatch::SetModelManager(nullptr);

    GPUParticleSystem::SetEngine(nullptr);
    BaseModel::SetIrufemiEngine(nullptr);
    Skybox::SetEngine(nullptr);
    GPUParticleSystem::SetDXCommon(dxCommon_.get());
    VoxelParticleSystem::SetEngine(nullptr);
    if (gpuParticleManager_) {
        gpuParticleManager_->Finalize();
        gpuParticleManager_.reset();
    }

    if (collisionManager_) {
        collisionManager_.reset();
    }

    if (voxelParticleManager_) {
        voxelParticleManager_.reset();
    }
    Primitive2DObject::SetEngine(nullptr);
    Line3DBatch::SetEngine(nullptr);
    Primitive3DObject::SetEngine(nullptr);
    Effect::SetEngine(nullptr);

    // プリミティブ管理解放
    primitiveManager_.reset();

    // 6. 基盤システム (サウンド・入力)
    if (audioManager_) {
        audioManager_->Finalize();
        audioManager_.reset();
    }
    if (inputManager_) {
        inputManager_.reset();
    }

    // 5.5 Dynamic Constant Buffers
    // (DirectX基盤に依存するため、dxCommon_より先に破棄)
    if (materialBufferManager_) {
        materialBufferManager_.reset();
    }
    if (transformBufferManager_) {
        transformBufferManager_.reset();
    }

    // 6. DirectX基盤
    if (dxCommon_) {
        if (dxCommon_->GetSrvPool() && depthSrvIndex_ != 0xFFFFFFFF) {
            dxCommon_->GetSrvPool()->FreeAfterFence(depthSrvIndex_, dxCommon_->GetCurrentFrameFenceValue());
            depthSrvIndex_ = 0xFFFFFFFF;
        }
        dxCommon_->Finalize();
        dxCommon_.reset();
    }

    // 1. エディタとUI (描画マネージャ等に依存)
    if (screenCaptureManager_) {
        screenCaptureManager_->Finalize();
        screenCaptureManager_.reset();
    }
    if (ui_) {
        ui_->Shutdown();
        ui_.reset();
    }

    // 7. OS・ウィンドウ
    if (winApp_) {
        winApp_.reset();
    }

    Irufemi::CVarSystem::Save("resources/settings_local.json");

    SceneSerializer::SetPrefabManager(nullptr);
    if (prefabManager_) {
        prefabManager_->ClearCache();
        prefabManager_.reset();
    }

    if (telemetrySender_) {
        telemetrySender_->Finalize();
    }

    // OSタイマー精度の引き上げを解除
    timeEndPeriod(1);

    isFinalized_ = true;
}

void IrufemiEngine::Execute() {

    // ローディング画面は Application から SetLoadingScreen 経由で注入されるため、ここでの構築は不要です

    // Application からの登録を反映
    if (sceneRegistrar_) {
        sceneRegistrar_(*sceneManager_);
    }

    // 初期シーンが指定されていれば遷移
    if (!initialSceneName_.empty()) {
        sceneManager_->ChangeTo(initialSceneName_);
    }

    while (winApp_->ProcessMessages()) {
        // 画面切り替えのクールダウン更新
        if (displayToggleCooldown_ > 0.0f) {
            displayToggleCooldown_ -= deltaTime_;
            if (displayToggleCooldown_ < 0.0f) {
                displayToggleCooldown_ = 0.0f;
            }
        }

        // 全画面/ウィンドウモードの切り替え（描画・ImGui開始前の最前線で実行）
        if (inputManager_ && displayToggleCooldown_ <= 0.0f) {
            bool shouldToggle = inputManager_->IsActionTriggered("ToggleFullscreen");
            // アクション未バインド環境用のフェイルセーフ
            if (!shouldToggle) {
                shouldToggle = inputManager_->IsKeyPressed(VK_F11) ||
                               (inputManager_->IsKeyDown(VK_MENU) && inputManager_->IsKeyPressed(VK_RETURN));
            }
            if (shouldToggle) {
                ToggleDisplayMode();
                displayToggleCooldown_ = 0.3f; // 0.3秒のクールダウン（OS DWMとの過渡的競合を防止）
            }
        }

        // 緊急脱出フェイルセーフ: ESCキーの2秒長押しで即時クリーンシャットダウン
        if (inputManager_) {
            if (inputManager_->IsKeyDown(VK_ESCAPE)) {
                emergencyExitHoldTimer_ += deltaTime_;
                if (emergencyExitHoldTimer_ >= 2.0f) {
                    PostQuitMessage(0);
                }
            } else {
                emergencyExitHoldTimer_ = 0.0f;
            }
        }

        // 保留中のウィンドウリサイズがあれば、ImGuiや描画開始前の安全なフレーム境界で適用 (Deferred Resize)
        ApplyPendingResize();

        // フレーム開始時の時間更新
        StartFrame();

        // ImGui_
        ui_->FrameStart();

        for (auto& ext : extensions_) {
            ext->OnDrawUI();
        }

// ImGui
#ifdef USE_IMGUI
#ifndef EditorMode
        // Standalone時のみ表示する統合デバッグUI（EditorMode時はEngineSettingsPanelがあるため二重表示しない）
        // F1 キーで統合デバッガーの表示/非表示をトグル
        static bool showEngineDebugger = true;
        if (inputManager_ && inputManager_->IsKeyPressed(VK_F1)) {
            showEngineDebugger = !showEngineDebugger;
        }

        if (showEngineDebugger && ui_->BeginEngineDebugWindow(&showEngineDebugger)) {
            // 共通デバッグタブ（SceneSelector, PostProcess, Scene, Display, ScreenCapture, GPUParticle 等）
            ui_->DrawCommonEngineTabs(this);

            ui_->EndEngineDebugWindow();
        }
#endif // !EditorMode

        // シーン固有の独立ウィンドウ描画（TL1Scene や テスト窓など）
        if (auto* scene = sceneManager_->GetCurrentScene()) {
            scene->DrawStandaloneDebugWindows();
        }
#endif // USE_IMGUI

        // 更新
        audioManager_->Update();
        postProcessManager_->ClearCustomEffectParams();
        sceneManager_->Update();
        // ローディング画面のアニメーション進行（Update相当）は描画時にまとめて行います
        totalTime_ += deltaTime_;
        sceneTransition_->Update(deltaTime_);
        postProcessManager_->Update(gameTime_);

        // 4) ParticleのUpdate (GPU)
        if (gpuParticleManager_) {
            gpuParticleManager_->Update();
        }

        for (auto& ext : extensions_) {
            ext->OnUpdate(deltaTime_);
        }

        if (voxelParticleManager_) {
            // ポーズ中は VoxelParticle の更新をスキップする
            bool isPaused = (sceneManager_ && sceneManager_->GetCurrent() == "Pause");
            if (!isPaused) {
                voxelParticleManager_->Update(deltaTime_);
            }
        }

        // インプットを更新
        inputManager_->Update();

        // フレーム途中処理
        ProcessFrame();

        // 描画
        sceneManager_->Draw();

        if (voxelParticleManager_) {
            voxelParticleManager_->Draw();
        }

        // ParticleGPU
        if (gpuParticleManager_) {
            gpuParticleManager_->Draw();
        }

        for (auto& ext : extensions_) {
            ext->OnDraw();
        }

        // ここで溜まった描画パケットを一斉に処理する
        drawManager_->ExecuteRenderQueues(this);

        // 終了処理
        EndFrame();
    }
}

// フレーム開始処理
void IrufemiEngine::StartFrame() {
    // 時間の更新
    auto now = std::chrono::steady_clock::now();
    deltaTime_ = std::chrono::duration<float>(now - lastFrameTime_).count();

    // スパイク（極端な処理落ちやロード等でのブロック）による物理演算の破綻やアニメーションのスキップを防ぐ
    const float kMaxDeltaTime = 0.1f;
    if (deltaTime_ > kMaxDeltaTime) {
        deltaTime_ = kMaxDeltaTime;
    }

    totalTime_ = std::chrono::duration<float>(now - startTime_).count();

    // ヒットストップの更新（実時間 deltaTime_ でカウントダウン）
    if (hitStopTimer_ > 0.0f) {
        hitStopTimer_ -= deltaTime_;
        if (hitStopTimer_ <= 0.0f) {
            hitStopTimer_ = 0.0f;
            timeScale_ = defaultTimeScale_;
        }
    }

    // ゲーム内時間の更新（タイムスケールを適用）
    gameDeltaTime_ = deltaTime_ * timeScale_;
    gameTime_ += gameDeltaTime_;

    lastFrameTime_ = now;

#if defined(_DEBUG) || defined(EditorMode)
    // ホットリロードの発火チェック
    if (shouldReloadShaders_.exchange(false)) {
        if (log_) {
            Log::OutPutLog(log_->GetLogStream(), "[Shader Hot Reload] Changes detected. Recompiling shaders...\n");
        }
        if (dxCommon_ && dxCommon_->GetPSOManager() && dxCommon_->GetShaderManager()) {
            // 安全のためにGPU処理を待機（使用中のシェーダーを破棄しないようにする）
            dxCommon_->WaitForGPU();

            // キャッシュの破棄
            dxCommon_->GetPSOManager()->ClearCache();
            dxCommon_->GetShaderManager()->ClearCache();

            // 再コンパイル
            dxCommon_->RegisterAllShaders();
            dxCommon_->GetPSOManager()->PreWarmCommonPSOs(threadPool_.get());

            if (log_) {
                Log::OutPutLog(log_->GetLogStream(), "[Shader Hot Reload] Compilation finished.\n");
            }
        }
    }
#endif
}

// フレーム途中処理
void IrufemiEngine::ProcessFrame() {
    // 非同期スレッドで遅延されていたSRVの更新をメインスレッドで一括適用する（データ競合の防止）
    if (dxCommon_) {
        dxCommon_->FlushPendingSRVUpdates();
    }

    // ステートのリセット（前フレームの描画ステートを引き継がないようにする）
    currentBlend_ = Irufemi::BlendMode::kBlendModeNormal;
    currentDepth_ = PSOManager::DepthWrite::Enable;
    currentCull_ = PSOManager::CullMode::Back;

    // 描画処理に入る前にImGui_::Renderを積む
    ui_->QueueDrawCommands();

    // 1. バックバッファをクリア (GPU同期待ちが発生する可能性があるため、待機時間を計測)
    auto beforePreDraw = std::chrono::steady_clock::now();
    drawManager_->PreDraw(clearColor_, 1.0f, 0);
    auto afterPreDraw = std::chrono::steady_clock::now();

    s_gpuWaitTimeMs = std::chrono::duration<float>(afterPreDraw - beforePreDraw).count() * 1000.0f;

    // (Compute Shaderの一括実行は、RenderGraph内のComputePassに移行しました)

    // 2. メインの描画先を RenderTexture に切り替え、指定のクリアカラーでクリア
    // G-Buffer拡張に伴い、5つのレンダーターゲットをバインド
    std::vector<RenderTexture*> renderTargets = {mainRenderTexture_.get(), effectMaskTexture_.get(),
                                                 normalTexture_.get(), materialTexture_.get(), velocityTexture_.get()};

    std::vector<Irufemi::Vector4> clearColors = {
        Irufemi::Vector4{clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]}, // Color
        Irufemi::Vector4{0.0f, 0.0f, 0.0f, 0.0f},                                         // Mask
        Irufemi::Vector4{0.0f, 0.0f, 1.0f, 1.0f},                                         // Normal
        Irufemi::Vector4{0.0f, 0.0f, 0.0f, 0.0f},                                         // Material
        Irufemi::Vector4{0.0f, 0.0f, 0.0f, 0.0f}                                          // Velocity
    };

    drawManager_->BeginRenderTextures(renderTargets, clearColors);
}

// フレーム終了処理
void IrufemiEngine::EndFrame() {
    // (RenderTexture の SRV 化やバックバッファへの転送、
    // ポストプロセス処理はすべて RenderGraph 内で行われます)

    // 描画先がバックバッファになり、ポストプロセス（暗転など）が掛かった上から
    // 影響を受けない最前面UIとしてロード画面を描画する
    if (sceneManager_ && sceneManager_->ShouldDrawLoadingScreen()) {
        if (loadingScreen_) {
            loadingScreen_->OnDrawLoadingScreen(this, deltaTime_);
        }
    }

    // 描画後処理
    ui_->QueuePostDrawCommands();

    // CPU側の純粋な処理時間（GPU同期待ちを引いた純粋なロジック・コマンド構築時間）を計測
    auto now = std::chrono::steady_clock::now();
    float totalElapsedMs = std::chrono::duration<float>(now - lastFrameTime_).count() * 1000.0f;

    // CPU処理時間 = フレーム経過時間 - GPUフェンス待ち時間 (ただしマイナスにならないようclamp)
    pureCpuTimeMs_ = (std::max)(0.0f, totalElapsedMs - s_gpuWaitTimeMs);

    drawManager_->PostDraw();

    if (screenCaptureManager_) {
        screenCaptureManager_->Update();
    }

    // 5) フレーム終端で遅延解放の回収(フェンス完了値を渡す)
    if (auto* srvPool = dxCommon_->GetSrvPool()) {
        const uint64_t completed = dxCommon_->GetFence()->GetCompletedValue();
        srvPool->GarbageCollect(completed);
    }

    // --- 中間リソースの遅延解放を実行 ---
    dxCommon_->ClearPendingResources();

    // 指数移動平均(EMA)を用いてFPSの変動を平滑化
    float currentFps = (deltaTime_ > 0.0f) ? (1.0f / deltaTime_) : 0.0f;
    emaFps_ = (emaFps_ == 0.0f || currentFps < 1.0f) ? currentFps : (emaFps_ * 0.9f + currentFps * 0.1f);

    // マニフェストに登録されたすべてのメトリクスを一括送信
    if (telemetryGatherer_) {
        telemetryGatherer_->DispatchAll();
    }
}

void IrufemiEngine::OnResize(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    // 現在のクライアント解像度と同一であれば、無駄なリサイズ要求（起動時の初期化ストール等）をスキップ
    if (dxCommon_ && dxCommon_->GetClientWidth() == width && dxCommon_->GetClientHeight() == height) {
        return;
    }
    // 即時リサイズせず、フレーム境界で安全に実行するよう保留する (Deferred Resize)
    isResizePending_ = true;
    pendingResizeWidth_ = width;
    pendingResizeHeight_ = height;
}

void IrufemiEngine::ApplyPendingResize() {
    if (!isResizePending_) {
        return;
    }
    isResizePending_ = false;

    int32_t width = pendingResizeWidth_;
    int32_t height = pendingResizeHeight_;
    if (width <= 0 || height <= 0) {
        return;
    }
    // 現在のサイズと同一であればスワップチェーン再生成・GPUストールをスキップ
    if (dxCommon_ && dxCommon_->GetClientWidth() == width && dxCommon_->GetClientHeight() == height) {
        return;
    }

    // 1. スワップチェーン、深度バッファのリサイズ (安全なフレーム境界で実行)
    dxCommon_->ResizeSwapChain(width, height);

    if (normalTexture_) {
        postProcessManager_->SetNormalSrvIndex(normalTexture_->GetSrvIndex());
    }
    if (materialTexture_) {
        postProcessManager_->SetMaterialSrvIndex(materialTexture_->GetSrvIndex());
    }
    if (velocityTexture_) {
        postProcessManager_->SetVelocitySrvIndex(velocityTexture_->GetSrvIndex());
    }

    // 3. 入力管理クラスへの解像度通知
    if (inputManager_) {
        inputManager_->SetGameResolution(static_cast<float>(gameResWidth_), static_cast<float>(gameResHeight_));
    }

    // 4. カメラの解像度更新 (3D空間の歪み防止)
    if (cameraManager_) {
        cameraManager_->OnResize(gameResWidth_, gameResHeight_);
    }

    // 5. 描画マネージャーへの通知 (RenderGraph等のキャッシュクリア)
    if (drawManager_) {
        drawManager_->OnResize(width, height);

        // 再構築された永続リソースの初期ステートをRenderGraphへ再登録する
        if (mainRenderTexture_ && mainRenderTexture_->GetResource()) {
            drawManager_->SetInitialResourceState(mainRenderTexture_->GetResource(),
                                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (dxCommon_ && dxCommon_->GetDepthStencilResource()) {
            drawManager_->SetInitialResourceState(dxCommon_->GetDepthStencilResource(),
                                                  D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }
}

void IrufemiEngine::SetCursorLocked(bool lock) {
    sceneRequestedCursorLock_ = lock;
    if (winApp_) {
#ifdef EditorMode
        if (!isPlayMode_) {
            winApp_->SetCursorLocked(false);
            return;
        }
#endif
        winApp_->SetCursorLocked(lock);
    }
}

void IrufemiEngine::SetPlayMode(bool play) {
    isPlayMode_ = play;
    SetCursorLocked(sceneRequestedCursorLock_);
}

bool IrufemiEngine::IsCursorLocked() const {
    if (winApp_) {
        return winApp_->IsCursorLocked();
    }
    return false;
}

void IrufemiEngine::ApplyPSO(const std::string& shaderName) {
    // Shadowパスの場合は自動的にシャドウ用シェーダに切り替える(元のコードの仕様維持)
    if (drawManager_->IsShadowPass()) {
        if (shaderName == "Object3D") {
            auto* pso = GetPSOManager()->GetPSO("Shadow", Irufemi::BlendMode::kBlendModeNone,
                                                PSOManager::DepthWrite::Enable, currentCull_);
            if (pso) {
                drawManager_->BindPSO(pso);
            }
            return;
        } else if (shaderName == "Skinning") {
            auto* pso = GetPSOManager()->GetPSO("ShadowSkinning", Irufemi::BlendMode::kBlendModeNone,
                                                PSOManager::DepthWrite::Enable, currentCull_);
            if (pso) {
                drawManager_->BindPSO(pso);
            }
            return;
        } else if (shaderName == "Batch") {
            auto* pso = GetPSOManager()->GetPSO("ShadowBatch", Irufemi::BlendMode::kBlendModeNone,
                                                PSOManager::DepthWrite::Enable, currentCull_);
            if (pso) {
                drawManager_->BindPSO(pso);
            }
            return;
        }
        // それ以外はシャドウパスでは描画しない(無視)
        return;
    }

    // Skybox用の特殊対応 (元のコードでは CullMode::Front 決め打ちでブレンドと深度は不要だった)
    if (shaderName == "Skybox") {
        auto* pso = GetPSOManager()->GetPSO("Skybox", Irufemi::BlendMode::kBlendModeNone,
                                            PSOManager::DepthWrite::Disable, PSOManager::CullMode::Front);
        if (pso) {
            drawManager_->BindPSO(pso);
        }
        return;
    }

    auto* pso = GetPSOManager()->GetPSO(shaderName, currentBlend_, currentDepth_, currentCull_);
    IRUFEMI_ASSERT(pso && ("PSO is null for " + shaderName).c_str());
    if (pso) {
        drawManager_->BindPSO(pso);
    }
}

void IrufemiEngine::BindLightningParams(D3D12_GPU_VIRTUAL_ADDRESS address) {
    if (address == 0) {
        return;
    }
    GetCommandList()->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special, address);
}

bool IrufemiEngine::IsAssetLoading() const {
    bool modelsLoaded = !modelManager_ || modelManager_->IsAllLoaded();
    bool texturesLoaded = !textureManager_ || textureManager_->IsAllLoaded();
    bool fontsLoaded = !fontManager_ || fontManager_->IsAllLoaded();
    return !modelsLoaded || !texturesLoaded || !fontsLoaded;
}

bool IrufemiEngine::SaveScreenShot(const std::wstring& filePath) {
    if (screenCaptureManager_) {
        return screenCaptureManager_->RequestCapture(filePath, ScreenCaptureType::SceneOnly);
    }
    return false;
}

bool IrufemiEngine::SaveScreenShotWithUI(const std::wstring& filePath) {
    if (screenCaptureManager_) {
        return screenCaptureManager_->RequestCapture(filePath, ScreenCaptureType::WithUI);
    }
    return false;
}

bool IrufemiEngine::SaveScreenShotWithMetadata(const std::wstring& filePath) {
    if (screenCaptureManager_) {
        return screenCaptureManager_->RequestCaptureWithMetadata(filePath, ScreenCaptureType::SceneOnly);
    }
    return false;
}

bool IrufemiEngine::SaveScreenShotWithAlpha(const std::wstring& filePath) {
    if (screenCaptureManager_) {
        return screenCaptureManager_->RequestCaptureWithAlpha(filePath);
    }
    return false;
}

bool IrufemiEngine::SaveScreenShotDepth(const std::wstring& filePath) {
    if (screenCaptureManager_) {
        return screenCaptureManager_->RequestCaptureDepth(filePath);
    }
    return false;
}

void IrufemiEngine::SetDisplayMode(DisplayMode mode) {
    if (winApp_) {
        winApp_->SetDisplayMode(mode);
    }
}

void IrufemiEngine::ToggleDisplayMode() {
    DisplayMode currentMode = (winApp_ ? winApp_->GetDisplayMode() : DisplayMode::Windowed);
    DisplayMode nextMode = (currentMode == DisplayMode::Windowed) ? DisplayMode::Borderless : DisplayMode::Windowed;
    Irufemi::CVarSystem::SetInt("r.DisplayMode", static_cast<int>(nextMode));
    if (winApp_ && winApp_->GetDisplayMode() != nextMode) {
        winApp_->SetDisplayMode(nextMode);
    }
}

void IrufemiEngine::SetVSync(bool enable) {
    if (drawManager_) {
        drawManager_->SetVSync(enable);
    }
}
