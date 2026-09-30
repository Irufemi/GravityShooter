#pragma once

#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/Type/BlendMode.h"
#include "Core/System/ILoadingScreen.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Matrix4x4.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "RHI/DirectX12/DynamicConstantBuffer.h"
#include "Renderer/Data/Material.h"
#include "Renderer/Data/TransformationMatrix.h"
#include "Core/System/ResourceHandle.h"
#include "Core/Profiler/TelemetryGatherer.h"
#include "Core/System/ThreadPool.h"
#include "Renderer/ScreenCaptureManager.h"

#include <memory>
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <functional>
#include <string>
#include <array>
#include <chrono>
#include <algorithm>

// --- 前方宣言 ---
class InputManager;
class WinApp;
enum class DisplayMode;
class DrawManager;
class DebugPrimitiveRenderer;
class DebugUI;
class IEngineExtension;
class TextureManager;
class AudioManager;
class ModelManager;
class AnimationManager;
class Log;
class SceneManager;
class SceneTransition;
class FontManager;
class VoxelParticleManager;
class GameObject;
class CameraManager;
class CollisionManager;
class GPUParticleManager;
class PrimitiveManager;
class TelemetrySender;
class PrefabManager;

/**
 * @class IrufemiEngine
 * @brief IrufemiEngine 全体を制御するメインクラス
 * @details エンジンの初期化、メインループ、終了処理を管理し、各マネージャへのアクセスを提供します。
 */
class IrufemiEngine {
public: // 内部型などは PostProcessManager.h へ移動しました。
    using PostProcessMode = ::PostProcessMode;
    using Mode = ::PostProcessMode; // 互換性のため

    using NoiseParams = PostProcessManager::NoiseParams;
    using VignetteParams = PostProcessManager::VignetteParams;
    using SmoothingParams = PostProcessManager::SmoothingParams;
    using GaussianParams = PostProcessManager::GaussianParams;
    using RadialBlurParams = PostProcessManager::RadialBlurParams;
    using OutlineParams = PostProcessManager::OutlineParams;
    using DissolveParams = PostProcessManager::DissolveParams;

public: // メンバ関数
    /**
     * @brief コンストラクタ
     */
    IrufemiEngine();

    /**
     * @brief デストラクタ
     */
    ~IrufemiEngine();

    /**
     * @brief メインループの実行
     * @details ウィンドウが閉じられるまで、Initialize から Finalize までのフローを制御します。
     */
    void Execute();

    /**
     * @brief エンジンの初期化
     * @param[in] title ウィンドウタイトル
     * @param[in] clientWidth 画面横幅 (デフォルト: 1280)
     * @param[in] clientHeight 画面縦幅 (デフォルト: 720)
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth = 1280, const int32_t& clientHeight = 720);

    /**
     * @brief エンジンの初期化（クリアカラー指定付き）
     * @param[in] title ウィンドウタイトル
     * @param[in] clientWidth 画面横幅
     * @param[in] clientHeight 画面縦幅
     * @param[in] r クリアカラー（赤）
     * @param[in] g クリアカラー（緑）
     * @param[in] b クリアカラー（青）
     * @param[in] a クリアカラー（アルファ）
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight, float r,
                    float g, float b, float a = 1.0f);

    /**
     * @brief エンジンの初期化（クリアカラー指定付き - std::array版）
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                    const std::array<float, 4>& clearColor);

    /**
     * @brief エンジンの初期化（クリアカラー指定付き - Vector4版）
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                    const Irufemi::Vector4& clearColor);

    /**
     * @brief ウィンドウリサイズ時の処理
     * @param[in] width 新しい横幅
     * @param[in] height 新しい縦幅
     */
    void OnResize(int32_t width, int32_t height);

    // --- Application からの注入用コールバック型とセッター ---
    using SceneRegistrar = std::function<void(SceneManager&)>;

    /**
     * @brief シーン登録用コールバックの設定
     * @param[in] registrar シーン登録を行う関数
     */
    void SetSceneRegistrar(SceneRegistrar registrar) {
        sceneRegistrar_ = std::move(registrar);
    }

    /**
     * @brief 起動時に読み込むシーン名の設定
     * @param[in] name シーン名
     */
    void SetInitialSceneName(std::string name) {
        initialSceneName_ = std::move(name);
    }

    /**
     * @brief エンジンの拡張機能（エディタなど）を追加する
     */
    void AddExtension(std::shared_ptr<IEngineExtension> extension) {
        if (extension) {
            extensions_.push_back(std::move(extension));
        }
    }

    /**
     * @brief ローディング画面の描画インターフェースを設定する
     * @param loadingScreen アプリケーション側で実装したローディング画面
     */
    void SetLoadingScreen(std::shared_ptr<ILoadingScreen> loadingScreen) {
        loadingScreen_ = std::move(loadingScreen);
    }

    /**
     * @brief ディスプレイモード（ウィンドウ/仮想フルスクリーン）を変更する
     */
    void SetDisplayMode(DisplayMode mode);

    /**
     * @brief ディスプレイモード（ウィンドウ/仮想フルスクリーン）を切り替える
     */
    void ToggleDisplayMode();

    /**
     * @brief VSync（垂直同期）の有効/無効を設定する
     */
    void SetVSync(bool enable);

private: // メンバ関数(内部処理)
    /**
     * @brief 終了処理
     */
    void Finalize();

    /**
     * @brief フレーム開始処理
     */
    void StartFrame();

    /**
     * @brief フレーム更新処理
     */
    void ProcessFrame();

    /**
     * @brief フレーム終了処理
     */
    void EndFrame();

public: // スクリーンショットAPI
    /** @name スクリーンショット機能 */
    ///@{
    bool SaveScreenShot(const std::wstring& filePath);
    /**
     * @brief UIを含めた現在の画面のスクリーンショットを保存する。
     * @param[in] filePath 保存先のファイルパス
     * @return 保存に成功した場合はtrue
     */
    bool SaveScreenShotWithUI(const std::wstring& filePath);
    /**
     * @brief メタデータを含めたスクリーンショットを保存する。
     * @param[in] filePath 保存先のファイルパス
     * @return 保存に成功した場合はtrue
     */
    bool SaveScreenShotWithMetadata(const std::wstring& filePath);
    /**
     * @brief アルファチャンネル（透過情報）を含めたスクリーンショットを保存する。
     * @param[in] filePath 保存先のファイルパス
     * @return 保存に成功した場合はtrue
     */
    bool SaveScreenShotWithAlpha(const std::wstring& filePath);
    /**
     * @brief 深度バッファの内容をスクリーンショットとして保存する。
     * @param[in] filePath 保存先のファイルパス
     * @return 保存に成功した場合はtrue
     */
    bool SaveScreenShotDepth(const std::wstring& filePath);
    ///@}

public: // ゲッター
    /** @name グラフィックス関連の取得 */
    ///@{
    ID3D12GraphicsCommandList* GetCommandList() {
        return dxCommon_->GetCommandList();
    }
    /**
     * @brief Device を取得する。
     * @return 取得された Device
     */
    ID3D12Device* GetDevice() {
        return dxCommon_->GetDevice();
    }
    /**
     * @brief Hwnd を取得する。
     * @return 取得された Hwnd
     */
    HWND GetHwnd() {
        return dxCommon_->GetHwnd();
    }
    /**
     * @brief SwapChainDesc を取得する。
     * @return 取得された SwapChainDesc
     */
    DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() {
        return dxCommon_->GetSwapChainDesc();
    }
    /**
     * @brief RtvDesc を取得する。
     * @return 取得された RtvDesc
     */
    D3D12_RENDER_TARGET_VIEW_DESC& GetRtvDesc() {
        return dxCommon_->GetRtvDesc();
    }
    /**
     * @brief SrvDescriptorHeap を取得する。
     * @return 取得された SrvDescriptorHeap
     */
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() {
        return dxCommon_->GetSrvDescriptorHeap();
    }
    /**
     * @brief CommandQueue を取得する。
     * @return 取得された CommandQueue
     */
    ID3D12CommandQueue* GetCommandQueue() {
        return dxCommon_->GetCommandQueue();
    }
    /**
     * @brief SwapChain を取得する。
     * @return 取得された SwapChain
     */
    IDXGISwapChain4* GetSwapChain() {
        return dxCommon_->GetSwapChain();
    }
    /**
     * @brief Fence を取得する。
     * @return 取得された Fence
     */
    ID3D12Fence* GetFence() {
        return dxCommon_->GetFence();
    }
    /**
     * @brief FenceEvent を取得する。
     * @return 取得された FenceEvent
     */
    HANDLE GetFenceEvent() {
        return dxCommon_->GetFenceEvent();
    }
    /**
     * @brief CommandAllocator を取得する。
     * @return 取得された CommandAllocator
     */
    ID3D12CommandAllocator* GetCommandAllocator() {
        return dxCommon_->GetCommandAllocator();
    }
    /**
     * @brief RootSignature を取得する。
     * @return 取得された RootSignature
     */
    ID3D12RootSignature* GetRootSignature() {
        return dxCommon_->GetRootSignature();
    }
    /**
     * @brief DsvDescriptorHeap を取得する。
     * @return 取得された DsvDescriptorHeap
     */
    ID3D12DescriptorHeap* GetDsvDescriptorHeap() {
        return dxCommon_->GetDsvDescriptorHeap();
    }
    /**
     * @brief SwapChainResources を取得する。
     * @return 取得された SwapChainResources
     */
    ID3D12Resource* GetSwapChainResources(UINT index) {
        return dxCommon_->GetSwapChainResources(index);
    }
    /**
     * @brief RtvHandles を取得する。
     * @return 取得された RtvHandles
     */
    D3D12_CPU_DESCRIPTOR_HANDLE& GetRtvHandles(UINT index) {
        return dxCommon_->GetRtvHandles(index);
    }
    /**
     * @brief FenceValue を取得する。
     * @return 取得された FenceValue
     */
    uint64_t& GetFenceValue() {
        return dxCommon_->GetFenceValue();
    }
    /**
     * @brief MainRenderTexture を取得する。
     * @return 取得された MainRenderTexture
     */
    RenderTexture* GetMainRenderTexture() const {
        return mainRenderTexture_.get();
    }
    /**
     * @brief EffectMaskTexture を取得する。
     * @return 取得された EffectMaskTexture
     */
    RenderTexture* GetEffectMaskTexture() const {
        return effectMaskTexture_.get();
    }
    /**
     * @brief NormalTexture を取得する。
     * @return 取得された NormalTexture
     */
    RenderTexture* GetNormalTexture() const {
        return normalTexture_.get();
    }
    /**
     * @brief MaterialTexture を取得する。
     * @return 取得された MaterialTexture
     */
    RenderTexture* GetMaterialTexture() const {
        return materialTexture_.get();
    }
    /**
     * @brief VelocityTexture を取得する。
     * @return 取得された VelocityTexture
     */
    RenderTexture* GetVelocityTexture() const {
        return velocityTexture_.get();
    }
    ///@}

    /** @name マネージャ類の取得 */
    ///@{
    DirectXCommon* GetDirectXCommon() {
        return this->dxCommon_.get();
    }
    /**
     * @brief InputManager を取得する。
     * @return 取得された InputManager
     */
    InputManager* GetInputManager() {
        return this->inputManager_.get();
    }
    /**
     * @brief DrawManager を取得する。
     * @return 取得された DrawManager
     */
    DrawManager* GetDrawManager() {
        return this->drawManager_.get();
    }
    /**
     * @brief DebugPrimitiveRenderer を取得する。
     * @return 取得された DebugPrimitiveRenderer
     */
    DebugPrimitiveRenderer* GetDebugPrimitiveRenderer() const {
        return this->debugPrimitiveRenderer_.get();
    }
    /**
     * @brief DebugUI を取得する。
     * @return 取得された DebugUI
     */
    DebugUI* GetDebugUI() {
        return this->ui_.get();
    }
    /**
     * @brief AudioManager を取得する。
     * @return 取得された AudioManager
     */
    AudioManager* GetAudioManager() {
        return this->audioManager_.get();
    }
    /**
     * @brief FontManager を取得する。
     * @return 取得された FontManager
     */
    FontManager* GetFontManager() {
        return this->fontManager_.get();
    }
    /**
     * @brief TextureManager を取得する。
     * @return 取得された TextureManager
     */
    TextureManager* GetTextureManager() {
        return this->textureManager_.get();
    }
    /**
     * @brief ObjModelManager を取得する。
     * @return 取得された ObjModelManager
     */
    ModelManager* GetObjModelManager() {
        return modelManager_.get();
    }
    /**
     * @brief AnimationManager を取得する。
     * @return 取得された AnimationManager
     */
    AnimationManager* GetAnimationManager() {
        return animationManager_.get();
    }
    /**
     * @brief CameraManager を取得する。
     * @return 取得された CameraManager
     */
    CameraManager* GetCameraManager() const {
        return cameraManager_.get();
    }
    /**
     * @brief CollisionManager を取得する。
     * @return 取得された CollisionManager
     */
    CollisionManager* GetCollisionManager() const {
        return collisionManager_.get();
    }
    /**
     * @brief VoxelParticleManager を取得する。
     * @return 取得された VoxelParticleManager
     */
    VoxelParticleManager* GetVoxelParticleManager() const {
        return voxelParticleManager_.get();
    }
    /**
     * @brief TelemetrySender を取得する。
     * @return 取得された TelemetrySender
     */
    TelemetrySender* GetTelemetrySender() const {
        return telemetrySender_.get();
    }
    /**
     * @brief PrefabManager を取得する。
     * @return 取得された PrefabManager
     */
    PrefabManager* GetPrefabManager() const {
        return prefabManager_.get();
    }
    /**
     * @brief ThreadPool を取得する。
     * @return 取得された ThreadPool
     */
    ThreadPool* GetThreadPool() const {
        return threadPool_.get();
    }
    /**
     * @brief GPUParticleManager を取得する。
     * @return 取得された GPUParticleManager
     */
    GPUParticleManager* GetGPUParticleManager() {
        return gpuParticleManager_.get();
    }
    /**
     * @brief PrimitiveManager を取得する。
     * @return 取得された PrimitiveManager
     */
    PrimitiveManager* GetPrimitiveManager() {
        return primitiveManager_.get();
    }
    /**
     * @brief ScreenCaptureManager を取得する。
     * @return 取得された ScreenCaptureManager
     */
    ScreenCaptureManager* GetScreenCaptureManager() const {
        return screenCaptureManager_.get();
    }
    /**
     * @brief ポストプロセス管理者を取得
     * @details シーンから pp->AddActiveMode() や pp->GetNoiseParams() のように使用します。
     */
    PostProcessManager* GetPostProcessManager() {
        return postProcessManager_.get();
    }

    /** @brief 画面遷移管理者を取得 */
    SceneTransition* GetSceneTransition() {
        return sceneTransition_.get();
    }

    /** @brief WinApp を取得 */
    WinApp* GetWinApp() const {
        return winApp_.get();
    }
    ///@}

    /** @name 画面情報の取得 */
    ///@{
    int32_t& GetClientWidth() {
        return dxCommon_->GetClientWidth();
    }

    /**
     * @brief 内部レンダリング解像度の幅を取得する
     */
    uint32_t GetGameResolutionWidth() const {
        return gameResWidth_;
    }

    /**
     * @brief 内部レンダリング解像度の高さを取得する
     */
    uint32_t GetGameResolutionHeight() const {
        return gameResHeight_;
    }

    /**
     * @brief 内部レンダリング解像度を設定する
     */
    void SetGameResolution(uint32_t width, uint32_t height) {
        gameResWidth_ = width;
        gameResHeight_ = height;
    }

    /**
     * @brief ClientHeight を取得する。
     * @return 取得された ClientHeight
     */
    int32_t& GetClientHeight() {
        return dxCommon_->GetClientHeight();
    }
    /**
     * @brief Viewport を取得する。
     * @return 取得された Viewport
     */
    D3D12_VIEWPORT& GetViewport() {
        return dxCommon_->GetViewport();
    }
    /**
     * @brief ScissorRect を取得する。
     * @return 取得された ScissorRect
     */
    D3D12_RECT& GetScissorRect() {
        return dxCommon_->GetScissorRect();
    }
    /**
     * @brief PSOManager を取得する。
     * @return 取得された PSOManager
     */
    PSOManager* GetPSOManager() {
        return dxCommon_->GetPSOManager();
    }
    ///@}

    // 時間関連のゲッター
    /**
     * @brief DeltaTime を取得する。
     * @return 取得された DeltaTime
     */
    float GetDeltaTime() const {
        return gameDeltaTime_;
    } // タイムスケール適用済みの時間を返す
    /**
     * @brief RealDeltaTime を取得する。
     * @return 取得された RealDeltaTime
     */
    float GetRealDeltaTime() const {
        return deltaTime_;
    } // 実時間を返す
    /**
     * @brief TotalTime を取得する。
     * @return 取得された TotalTime
     */
    float GetTotalTime() const {
        return totalTime_;
    }

    // ポーズ対応のゲーム内時間制御
    /**
     * @brief GameTime を取得する。
     * @return 取得された GameTime
     */
    float GetGameTime() const {
        return gameTime_;
    }
    /**
     * @brief GameDeltaTime を取得する。
     * @return 取得された GameDeltaTime
     */
    float GetGameDeltaTime() const {
        return gameDeltaTime_;
    }
    /**
     * @brief TimeScale を取得する。
     * @return 取得された TimeScale
     */
    float GetTimeScale() const {
        return timeScale_;
    }
    /**
     * @brief TimeScale を設定する。
     * @param[in] scale 設定する TimeScale の値
     */
    void SetTimeScale(float scale) {
        timeScale_ = scale;
        defaultTimeScale_ = scale;
    }
    /**
     * @brief 一定時間ゲーム全体を微小停止・減速させるヒットストップを発火する（AAA基準 Game Juice）
     * @param[in] duration 停止時間（秒、実時間ベース）
     * @param[in] slowScale 停止中のタイムスケール（0.0f で完全静止、0.1f 等でスロー）
     */
    void TriggerHitStop(float duration, float slowScale = 0.0f) {
        if (duration <= 0.0f) {
            return;
        }
        hitStopTimer_ = duration;
        timeScale_ = slowScale;
    }

    float GetPureCpuTimeMs() const {
        return pureCpuTimeMs_;
    }
    float GetEmaFps() const {
        return emaFps_;
    }

    /**
     * @brief MaterialBufferManager を取得する。
     * @return 取得された MaterialBufferManager
     */
    DynamicConstantBuffer<Material>* GetMaterialBufferManager() {
        return materialBufferManager_.get();
    }
    /**
     * @brief TransformBufferManager を取得する。
     * @return 取得された TransformBufferManager
     */
    DynamicConstantBuffer<TransformationMatrix>* GetTransformBufferManager() {
        return transformBufferManager_.get();
    }

    // オプション: 取得用
    /**
     * @brief SrvPool を取得する。
     * @return 取得された SrvPool
     */
    DescriptorPool* GetSrvPool() const {
        return dxCommon_->GetSrvPool();
    }

    // SceneManager参照
    /**
     * @brief SceneManager を取得する。
     * @return 取得された SceneManager
     */
    SceneManager* GetSceneManager() const {
        return sceneManager_.get();
    }

    // Sceneディレクトリ設定
    /**
     * @brief SceneDirectory を取得する。
     * @return 取得された SceneDirectory
     */
    const std::string& GetSceneDirectory() const {
        return sceneDirectory_;
    }
    /**
     * @brief SceneDirectory を設定する。
     * @param[in] dir 設定する SceneDirectory の値
     */
    void SetSceneDirectory(const std::string& dir) {
        sceneDirectory_ = dir;
    }

    /**
     * @brief IsAssetLoading かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsAssetLoading() const;

    // --- エディタ・プレイスタイル関連の状態 ---
    /**
     * @brief IsPlayMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsPlayMode() const {
        return isPlayMode_;
    }
    /**
     * @brief PlayMode を設定する。
     * @param[in] play 設定する PlayMode の値
     */
    void SetPlayMode(bool play);

    /**
     * @brief SelectedObject を取得する。
     * @return 取得された SelectedObject
     */
    std::shared_ptr<GameObject> GetSelectedObject() const {
        return selectedObject_.lock();
    }
    /**
     * @brief SelectedObject を設定する。
     * @param[in] obj 設定する SelectedObject の値
     */
    void SetSelectedObject(std::shared_ptr<GameObject> obj) {
        selectedObject_ = obj;
    }

public: // セッター
    /**
     * @brief AddFenceValue を実行する。
     */
    void AddFenceValue(uint32_t index) {
        dxCommon_->GetFenceValue() += index;
    }

    // セッター(引数なし描画のためのプリセット切替)
    /**
     * @brief Blend を設定する。
     * @param[in] m 設定する Blend の値
     */
    void SetBlend(Irufemi::BlendMode m) {
        currentBlend_ = m;
    }
    /**
     * @brief DepthWrite を設定する。
     * @param[in] w 設定する DepthWrite の値
     */
    void SetDepthWrite(PSOManager::DepthWrite w) {
        currentDepth_ = w;
    }
    /**
     * @brief Cull を設定する。
     * @param[in] c 設定する Cull の値
     */
    void SetCull(PSOManager::CullMode c) {
        currentCull_ = c;
    }

    /**
     * @brief 画面のクリアカラー（背景色）を設定する。
     * @param[in] color クリアカラー(RGBA)
     */
    void SetClearColor(float r, float g, float b, float a = 1.0f) {
        clearColor_ = {r, g, b, a};
    }
    /**
     * @brief 画面のクリアカラー（背景色）を設定する。
     * @param[in] color クリアカラー(RGBA)
     */
    void SetClearColor(const std::array<float, 4>& c) {
        clearColor_ = c;
    }
    /**
     * @brief 画面のクリアカラー（背景色）を設定する。
     * @param[in] color クリアカラー(RGBA)
     */
    void SetClearColor(const Irufemi::Vector4& c) {
        clearColor_ = {c.x, c.y, c.z, c.w};
    }

    /**
     * @brief PostProcessMode を取得する。
     * @return 取得された PostProcessMode
     */
    PostProcessMode GetPostProcessMode() const {
        return postProcessManager_->GetMode();
    }
    /**
     * @brief PostProcessMode を設定する。
     * @param[in] mode 設定する PostProcessMode の値
     */
    void SetPostProcessMode(PostProcessMode mode) {
        postProcessManager_->SetMode(mode);
    }
    /**
     * @brief VignetteParams を取得する。
     * @return 取得された VignetteParams
     */
    VignetteParams& GetVignetteParams() {
        return postProcessManager_->GetVignetteParams();
    }
    /**
     * @brief OutlineParams を取得する。
     * @return 取得された OutlineParams
     */
    OutlineParams& GetOutlineParams() {
        return postProcessManager_->GetOutlineParams();
    }
    /**
     * @brief DissolveParams を取得する。
     * @return 取得された DissolveParams
     */
    DissolveParams& GetDissolveParams() {
        return postProcessManager_->GetDissolveParams();
    }
    /**
     * @brief SmoothingParams を取得する。
     * @return 取得された SmoothingParams
     */
    SmoothingParams& GetSmoothingParams() {
        return postProcessManager_->GetSmoothingParams();
    }
    /**
     * @brief GaussianParams を取得する。
     * @return 取得された GaussianParams
     */
    GaussianParams& GetGaussianParams() {
        return postProcessManager_->GetGaussianParams();
    }
    /**
     * @brief RadialBlurParams を取得する。
     * @return 取得された RadialBlurParams
     */
    RadialBlurParams& GetRadialBlurParams() {
        return postProcessManager_->GetRadialBlurParams();
    }
    /**
     * @brief NoiseParams を取得する。
     * @return 取得された NoiseParams
     */
    NoiseParams& GetNoiseParams() {
        return postProcessManager_->GetNoiseParams();
    }

    /**
     * @brief CursorLocked を設定する。
     * @param[in] lock 設定する CursorLocked の値
     */
    void SetCursorLocked(bool lock);
    /**
     * @brief IsCursorLocked かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCursorLocked() const;

    /**
     * @brief 登録されたシェーダー名と現在の状態からPSOを適用する
     * @param shaderName 登録済みのシェーダー名 (例: "Object3D", "Sprite", "Particle" 等)
     */
    void ApplyPSO(const std::string& shaderName);

    /**
     * @brief 電撃エフェクト用パラメータを特設スロットにバインドする
     * @param address LightningParams 構造体の GPU 仮想アドレス
     */
    void BindLightningParams(D3D12_GPU_VIRTUAL_ADDRESS address);

public:
    // 状態(現在のブレンドと深度書き込み)
    Irufemi::BlendMode currentBlend_ = Irufemi::BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite currentDepth_ = PSOManager::DepthWrite::Enable;
    PSOManager::CullMode currentCull_ = PSOManager::CullMode::Back; //!< 現在のカリングモード（デフォルトは背面）

private: // メンバ変数
    // --- Debug & Logging ---
    // ログ
    std::unique_ptr<Log> log_ = nullptr;

    // WinApp
    std::unique_ptr<WinApp> winApp_ = nullptr;

    // DirectX基盤
    std::unique_ptr<DirectXCommon> dxCommon_ = nullptr;

    // --- Manager ---

    // InputManager
    std::unique_ptr<InputManager> inputManager_ = nullptr;

    // DrawManager
    std::unique_ptr<DrawManager> drawManager_ = nullptr;

    // DebugPrimitiveRenderer
    std::unique_ptr<DebugPrimitiveRenderer> debugPrimitiveRenderer_ = nullptr;

    // DebugUI
    std::unique_ptr<DebugUI> ui_ = nullptr;

    // Extensions
    std::vector<std::shared_ptr<IEngineExtension>> extensions_;

    // TextureManager
    std::unique_ptr<TextureManager> textureManager_ = nullptr;

    // AudioManager
    std::unique_ptr<AudioManager> audioManager_ = nullptr;

    // SceneManager
    std::unique_ptr<SceneManager> sceneManager_ = nullptr;

    // LoadingScreen (Application側から注入)
    std::shared_ptr<ILoadingScreen> loadingScreen_ = nullptr;

    // FontManager
    std::unique_ptr<FontManager> fontManager_ = nullptr;

    // ModelManager
    std::unique_ptr<ModelManager> modelManager_ = nullptr;

    // AnimationManager
    std::unique_ptr<AnimationManager> animationManager_ = nullptr;

    // CameraManager
    std::unique_ptr<CameraManager> cameraManager_ = nullptr;

    // VoxelParticleManager
    std::unique_ptr<VoxelParticleManager> voxelParticleManager_ = nullptr;

    // CollisionManager
    std::unique_ptr<CollisionManager> collisionManager_ = nullptr;

    // GPUParticleManager
    std::unique_ptr<GPUParticleManager> gpuParticleManager_ = nullptr;

    // PrimitiveManager
    std::unique_ptr<PrimitiveManager> primitiveManager_ = nullptr;

    // ScreenCaptureManager
    std::unique_ptr<ScreenCaptureManager> screenCaptureManager_ = nullptr;

    // ThreadPool
    std::unique_ptr<ThreadPool> threadPool_ = nullptr;

    // 画面の色
    std::array<float, 4> clearColor_{0.1f, 0.25f, 0.5f, 1.0f};
    std::string sceneDirectory_ = "resources/scenes/";
    UINT backBufferIndex_{};

    // Application から注入
    SceneRegistrar sceneRegistrar_{};
    std::string initialSceneName_{};

    // --- 時間管理 ---
    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point lastFrameTime_{}; // 時間系変数
    float totalTime_ = 0.0f;
    float gameTime_ = 0.0f;
    float deltaTime_ = 0.0f;
    float gameDeltaTime_ = 0.0f;
    float timeScale_ = 1.0f;
    float defaultTimeScale_ = 1.0f;
    float hitStopTimer_ = 0.0f;

    // プロファイラ用
    float pureCpuTimeMs_ = 0.0f;
    float emaFps_ = 60.0f;

    // --- Dynamic Constant Buffer ---
    std::unique_ptr<DynamicConstantBuffer<Material>> materialBufferManager_ = nullptr;
    std::unique_ptr<DynamicConstantBuffer<TransformationMatrix>> transformBufferManager_ = nullptr;

    // --- 全画面用 RenderTexture ---
    std::unique_ptr<RenderTexture> mainRenderTexture_ = nullptr;
    std::unique_ptr<RenderTexture> effectMaskTexture_ = nullptr; // ★ MRT対応: マスク用
    std::unique_ptr<RenderTexture> normalTexture_ = nullptr;     // MRT対応: 法線/深度用
    std::unique_ptr<RenderTexture> materialTexture_ = nullptr;   // MRT対応: マテリアル用
    std::unique_ptr<RenderTexture> velocityTexture_ = nullptr;   // MRT対応: モーションベクトル用
    std::unique_ptr<PostProcessManager> postProcessManager_ = nullptr;
    std::unique_ptr<SceneTransition> sceneTransition_ = nullptr;

    // Telemetry
    std::unique_ptr<TelemetrySender> telemetrySender_ = nullptr;
    std::unique_ptr<TelemetryGatherer> telemetryGatherer_ = nullptr;

    // Prefab Manager
    std::unique_ptr<PrefabManager> prefabManager_ = nullptr;

    uint32_t depthSrvIndex_ = 0xFFFFFFFF; // 深度SRVのインデックスを保持
    bool isFinalized_ = false;            // 終了処理済みフラグ

    // --- ゲーム内部レンダリング解像度 ---
    uint32_t gameResWidth_ = 1280;
    uint32_t gameResHeight_ = 720;

    ResourceHandle noise0Handle_;
    ResourceHandle noise1Handle_;

    bool sceneRequestedCursorLock_ = false;
    bool isPlayMode_ = true;
    std::weak_ptr<GameObject> selectedObject_;
#if defined(_DEBUG) || defined(EditorMode)
    std::vector<std::unique_ptr<class DirectoryWatcher>> shaderWatchers_;
    std::atomic<bool> shouldReloadShaders_{false};
#endif
};
