#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

class IrufemiEngine;
class DirectXCommon;
class RenderTexture;
class RenderGraph;
class PostProcessRunner;

namespace Irufemi {
class BloomEffect;
class LightShaftsEffect;
class SeparableBlurEffect;
class KawaseBlurEffect;
class CombinedPostEffect;
} // namespace Irufemi

/**
 * @enum PostProcessMode
 * @brief ポストプロセスエフェクトの描画モード
 */
enum class PostProcessMode {
    None,
    Grayscale,
    Sepia,
    Vignette,
    Smoothing,
    GaussianFilter,
    DepthBasedOutline,
    RadialBlur,
    Dissolve,
    Noise,
    HSV,
    ToneMapping,
    Fade,
    Slide,
    Bloom,
    Glitch,
    DualKawaseBlur,
    LuminanceBasedOutline,
    Pixelation,
    Pointillism,
    Posterization,
    NightVision,
    Kaleidoscope,
    ChromaticAberration,
    DisplacementMap,
    DirectionalBlur,
    Halftone,
    DepthOfField,
    LightShafts,
    GravityDistortion,
    BlackHoleSupernova,
    Count
};

/**
 * @struct PostProcessModeInfo
 * @brief エフェクトのモードと優先順位の組
 */
struct PostProcessModeInfo {
    PostProcessMode mode;
    int priority = 0; // 大きいほど後（手前）に描画される

    bool operator<(const PostProcessModeInfo& other) const {
        return priority < other.priority;
    }
};

/**
 * @enum EffectLayer
 * @brief ポストプロセスの適用レイヤー（UIの前か後か）
 */
enum class EffectLayer {
    PreUI,  // 3Dシーンや背景にかかる（UIにはかからない）
    PostUI, // UIを含む画面全体にかかる
    All     // 全レイヤー対象（デフォルト引数・一括解除判定用）
};

/**
 * @class PostProcessManager
 * @brief ポストプロセスのスケジューリングとレンダーグラフ連携を統括するマネージャークラス
 * @details 一線級ゲームエンジンのステートレス・パイプライン設計に基づき、個別エフェクトの実装は IPostProcessEffect 群へ完全委譲
 */
class PostProcessManager {
    friend class PostProcessRunner;
    friend class Irufemi::BloomEffect;
    friend class Irufemi::LightShaftsEffect;
    friend class Irufemi::SeparableBlurEffect;
    friend class Irufemi::KawaseBlurEffect;
    friend class Irufemi::CombinedPostEffect;

public:
    PostProcessManager();
    ~PostProcessManager();

    using Mode = PostProcessMode;
    using Layer = EffectLayer;

    static constexpr int32_t kMaxKawaseIterations = 8;            // 最大ダウンサンプル回数
    static constexpr uint32_t kMaxPostProcessBufferEntries = 256; ///< 動的定数バッファの最大リング要素数

    struct PostProcessWorkspace {
        class RenderTexture* workTextures[2] = {nullptr, nullptr};
        class RenderTexture* bloomExtract = nullptr;
        class RenderTexture* bloomBlur = nullptr;
        class RenderTexture* lsExtract = nullptr;
        class RenderTexture* lsBlur = nullptr;
        class RenderTexture* kawaseTextures[kMaxKawaseIterations] = {nullptr};
    };

    using CustomEffectParams = Irufemi::CustomEffectParams;
    static constexpr uint32_t kMaxCustomEffectParams = Irufemi::kMaxCustomEffectParams;

    using NoiseParams = Irufemi::NoiseParams;
    using VignetteParams = Irufemi::VignetteParams;
    using SmoothingParams = Irufemi::SmoothingParams;
    using GaussianParams = Irufemi::GaussianParams;
    using RadialBlurParams = Irufemi::RadialBlurParams;
    using OutlineParams = Irufemi::OutlineParams;
    using LuminanceOutlineParams = Irufemi::LuminanceOutlineParams;
    using PixelationParams = Irufemi::PixelationParams;
    using PointillismParams = Irufemi::PointillismParams;
    using DissolveParams = Irufemi::DissolveParams;
    using HSVParams = Irufemi::HSVParams;
    using ToneMappingParams = Irufemi::ToneMappingParams;
    using FadeParams = Irufemi::FadeParams;
    using SlideParams = Irufemi::SlideParams;
    using BloomParams = Irufemi::BloomParams;
    using GlitchParams = Irufemi::GlitchParams;
    using DualKawaseBlurParams = Irufemi::DualKawaseBlurParams;
    using PosterizationParams = Irufemi::PosterizationParams;
    using NightVisionParams = Irufemi::NightVisionParams;
    using KaleidoscopeParams = Irufemi::KaleidoscopeParams;
    using ChromaticAberrationParams = Irufemi::ChromaticAberrationParams;
    using DisplacementMapParams = Irufemi::DisplacementMapParams;
    using DirectionalBlurParams = Irufemi::DirectionalBlurParams;
    using HalftoneParams = Irufemi::HalftoneParams;
    using DepthOfFieldParams = Irufemi::DepthOfFieldParams;
    using LightShaftsParams = Irufemi::LightShaftsParams;
    using GravityDistortionParams = Irufemi::GravityDistortionParams;
    using BlackHoleSupernovaParams = Irufemi::BlackHoleSupernovaParams;
    using CombinedParams = Irufemi::CombinedParams;
    using BindlessParams = Irufemi::BindlessParams;

    /**
     * @brief ポストプロセスの初期化
     * @param engine エンジンポインタ
     * @param dxCommon DirectX基盤クラス
     * @param rtvFormat 最終的な出力先のRTVフォーマット
     */
    void Initialize(IrufemiEngine* engine, DirectXCommon* dxCommon, DXGI_FORMAT rtvFormat);

    /**
     * @brief 型安全なエフェクト取得インターフェース（エフェクトレジストリ）
     */
    template <typename T>
    T* GetEffect() const;

    /**
     * @brief フレーム描画開始処理（リングバッファオフセットのリセットおよびカスタムパラメータの同期）
     */
    void BeginFrame();

    /**
     * @brief ポストプロセスの状態をリセットする
     * @param clearPostUI PostUIレイヤーもクリアするかどうか
     */
    void Reset(bool clearPostUI = true);

    /**
     * @brief 時間経過に伴うアニメーション・演出の更新
     */
    void Update(float totalTime);

    /**
     * @brief カスタムエフェクトパラメータリストのクリア
     */
    void ClearCustomEffectParams() {}

    /**
     * @brief エンジンへの参照を取得
     */
    IrufemiEngine* GetEngine() const {
        return engine_;
    }

    /**
     * @brief DirectXCommonへの参照を取得
     */
    DirectXCommon* GetDxCommon() const {
        return dxCommon_;
    }

    /**
     * @brief RTVフォーマットを取得
     */
    DXGI_FORMAT GetRtvFormat() const {
        return rtvFormat_;
    }

    /**
     * @brief 有効なポストプロセスモードの一覧を取得する
     * @param layer 取得対象のレイヤー
     * @return モードのリスト
     */
    std::vector<Mode> GetActiveModes(Layer layer = Layer::PreUI) const;

    /**
     * @brief 指定したポストプロセスモードが有効かどうかを判定する
     * @param mode 判定するモード
     * @param layer 判定対象のレイヤー
     * @return 有効な場合は true、無効な場合は false
     */
    bool HasActiveMode(Mode mode, Layer layer = Layer::All) const;

    /**
     * @brief ポストプロセスモードを追加する
     * @param mode 追加するモード
     * @param layer 追加先のレイヤー
     * @param priority 描画優先度（大きいほど手前に描画される）
     */
    void AddActiveMode(Mode mode, Layer layer = Layer::PreUI, int priority = 0);

    /**
     * @brief 指定したポストプロセスモードを解除する
     * @param mode 解除するモード
     * @param layer 解除対象のレイヤー（Layer::All で全レイヤーから完全解除）
     */
    void RemoveActiveMode(Mode mode, Layer layer = Layer::All);

    /**
     * @brief すべてのポストプロセスモードを解除する
     * @param layer 解除対象のレイヤー（Layer::All で全レイヤーをクリア）
     */
    void ClearActiveModes(Layer layer = Layer::All);

    /**
     * @brief ポストプロセスモードの一覧を一括設定する
     * @param modes 設定するモードのリスト
     * @param layer 設定先のレイヤー
     */
    void SetActiveModes(const std::vector<Mode>& modes, Layer layer = Layer::PreUI);

    /**
     * @brief ペンディング状態のモード変更を描画用リストに反映する
     */
    void CommitPendingModes();

    /**
     * @brief そのエフェクトが深度バッファを必要とするかどうか
     */
    static bool UsesDepthBuffer(Mode mode) {
        return mode == Mode::DepthBasedOutline || mode == Mode::DepthOfField || mode == Mode::LightShafts;
    }

    /**
     * @brief 描画実行（マルチパスおよび単一パス対応）
     */
    void Draw(ID3D12GraphicsCommandList* commandList, class RenderTexture* srcTexture,
              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, Layer layer = Layer::PreUI, bool isBackBufferTarget = false,
              const D3D12_VIEWPORT* targetViewport = nullptr, const D3D12_RECT* targetScissor = nullptr);

    D3D12_GPU_VIRTUAL_ADDRESS AllocateBindlessParams(uint32_t mainTextureIndex, uint32_t extraTextureIndex = 0,
                                                   uint32_t maskTextureIndex = 0);

    uint32_t RegisterCustomEffectParams(uint64_t objectId, const CustomEffectParams& params);
    uint32_t RegisterCustomEffectParams(const CustomEffectParams& params) {
        return RegisterCustomEffectParams(0, params);
    }
    void UnregisterCustomEffectParams(uint64_t objectId);

    void TryIncrementBindlessOffset() {
        if (bindlessBufferOffset_ + 1 < kMaxPostProcessBufferEntries) {
            bindlessBufferOffset_++;
        }
    }
    void TryIncrementCombinedOffset() {
        if (combinedBufferOffset_ + 1 < kMaxPostProcessBufferEntries) {
            combinedBufferOffset_++;
        }
    }

    /**
     * @brief エフェクトの実行ランナーを取得する
     */
    PostProcessRunner* GetRunner() const {
        return runner_.get();
    }

    /**
     * @brief オブジェクト固有のカスタムパラメータ用スロットを確保する
     * @param objectId オブジェクト固有の識別子
     * @return 割り当てられたスロット番号
     */
    uint32_t AllocateCustomEffectSlot(uint64_t objectId);

    /**
     * @brief オブジェクト固有のカスタムパラメータ用スロットを解放する
     * @param objectId オブジェクト固有の識別子
     */
    void FreeCustomEffectSlot(uint64_t objectId);

    /**
     * @brief カスタムパラメータの値を設定する
     * @param slot スロット番号
     * @param params 設定するパラメータ
     */
    void SetCustomEffectParams(uint32_t slot, const CustomEffectParams& params);

    /**
     * @brief カスタムパラメータの値を取得する
     * @param slot スロット番号
     * @return 設定されているパラメータ
     */
    const CustomEffectParams& GetCustomEffectParams(uint32_t slot) const;

    /**
     * @brief すべてのカスタムエフェクトスロットを初期化する
     */
    void ResetCustomEffectSlots();

    /**
     * @brief カスタムエフェクト定数バッファのGPU仮想アドレスを取得する
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetCustomEffectParamsGpuAddress(uint32_t slot) const;

    /**
     * @brief 重力波ディストーションを発火する
     * @param centerUV 歪み中心スクリーンUV (0.0〜1.0)
     * @param intensity 歪みの最大強度
     * @param duration 演出持続時間（秒）
     * @param maxRadius 最大波面半径
     * @param chromaticRatio 色収差比率
     * @param waveWidth 波の幅
     */
    void TriggerGravityDistortion(const Irufemi::Vector2& centerUV, float intensity = 0.10f, float duration = 0.5f,
                                  float maxRadius = 1.3f, float chromaticRatio = 0.035f, float waveWidth = 0.18f);

    // --- 各ポストプロセスパラメータへの安全な委譲アクセサ（Facade API） ---
    FadeParams& GetFadeParams();
    const FadeParams& GetFadeParams() const;
    RadialBlurParams& GetRadialBlurParams();
    const RadialBlurParams& GetRadialBlurParams() const;
    ChromaticAberrationParams& GetChromaticAberrationParams();
    const ChromaticAberrationParams& GetChromaticAberrationParams() const;
    DisplacementMapParams& GetDisplacementMapParams();
    const DisplacementMapParams& GetDisplacementMapParams() const;
    GravityDistortionParams& GetGravityDistortionParams();
    const GravityDistortionParams& GetGravityDistortionParams() const;
    BlackHoleSupernovaParams& GetBlackHoleSupernovaParams();
    const BlackHoleSupernovaParams& GetBlackHoleSupernovaParams() const;
    DissolveParams& GetDissolveParams();
    const DissolveParams& GetDissolveParams() const;
    SlideParams& GetSlideParams();
    const SlideParams& GetSlideParams() const;
    NoiseParams& GetNoiseParams();
    const NoiseParams& GetNoiseParams() const;
    GlitchParams& GetGlitchParams();
    const GlitchParams& GetGlitchParams() const;
    VignetteParams& GetVignetteParams();
    const VignetteParams& GetVignetteParams() const;
    OutlineParams& GetOutlineParams();
    const OutlineParams& GetOutlineParams() const;
    HSVParams& GetHSVParams();
    const HSVParams& GetHSVParams() const;
    ToneMappingParams& GetToneMappingParams();
    const ToneMappingParams& GetToneMappingParams() const;
    BloomParams& GetBloomParams();
    const BloomParams& GetBloomParams() const;
    LightShaftsParams& GetLightShaftsParams();
    const LightShaftsParams& GetLightShaftsParams() const;
    DualKawaseBlurParams& GetDualKawaseBlurParams();
    const DualKawaseBlurParams& GetDualKawaseBlurParams() const;
    DepthOfFieldParams& GetDepthOfFieldParams();
    const DepthOfFieldParams& GetDepthOfFieldParams() const;
    DirectionalBlurParams& GetDirectionalBlurParams();
    const DirectionalBlurParams& GetDirectionalBlurParams() const;
    HalftoneParams& GetHalftoneParams();
    const HalftoneParams& GetHalftoneParams() const;
    LuminanceOutlineParams& GetLuminanceOutlineParams();
    const LuminanceOutlineParams& GetLuminanceOutlineParams() const;
    PixelationParams& GetPixelationParams();
    const PixelationParams& GetPixelationParams() const;
    PointillismParams& GetPointillismParams();
    const PointillismParams& GetPointillismParams() const;
    PosterizationParams& GetPosterizationParams();
    const PosterizationParams& GetPosterizationParams() const;

    void SetDissolveNoiseIndex(int index, uint32_t srvIndex) {
        if (index >= 0 && index < 2) {
            dissolveNoiseIndex_[index] = srvIndex;
        }
    }
    uint32_t GetDissolveNoiseIndex(int index) const {
        if (index >= 0 && index < 2) {
            return dissolveNoiseIndex_[index];
        }
        return 0;
    }

    void SetDepthSrvIndex(uint32_t srvIndex) { depthSrvIndex_ = srvIndex; }
    uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

    void SetNormalSrvIndex(uint32_t srvIndex) { normalSrvIndex_ = srvIndex; }
    uint32_t GetNormalSrvIndex() const { return normalSrvIndex_; }

    void SetMaterialSrvIndex(uint32_t srvIndex) { materialSrvIndex_ = srvIndex; }
    uint32_t GetMaterialSrvIndex() const { return materialSrvIndex_; }

    void SetVelocitySrvIndex(uint32_t srvIndex) { velocitySrvIndex_ = srvIndex; }
    uint32_t GetVelocitySrvIndex() const { return velocitySrvIndex_; }

    /**
     * @brief 各ポストプロセスエフェクト用パイプラインステート（PSO）を生成する
     */
    void CreatePSOs();

    /**
     * @brief 各ポストプロセスエフェクト用定数バッファ（CBV）を生成する
     */
    void CreateConstantBuffers();

    /**
     * @brief 単一のポストプロセスマルチパス描画を実行する
     */
    void DrawSinglePass(ID3D12GraphicsCommandList* commandList, Mode mode, RenderTexture* srcTexture,
                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalPass = false,
                        ID3D12PipelineState* psoOverride = nullptr,
                        D3D12_GPU_VIRTUAL_ADDRESS cbvOverride = 0);

    /**
     * @brief アップロードヒープ上に定数バッファ用リソースを作成する
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(size_t size);

private:
    IrufemiEngine* engine_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    ID3D12Device* device_ = nullptr;
    ID3D12RootSignature* rootSig_ = nullptr;
    DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_UNKNOWN;

    mutable std::mutex modesMutex_;
    std::vector<Mode> activePreUI_;
    std::vector<PostProcessModeInfo> pendingPreUI_;

    std::vector<Mode> activePostUI_;
    std::vector<PostProcessModeInfo> pendingPostUI_;

    std::mutex customParamsMutex_;
    std::array<CustomEffectParams, kMaxCustomEffectParams> persistentCustomParams_{};
    std::unordered_map<uint64_t, uint32_t> objectToSlotMap_;
    std::vector<uint32_t> freeSlots_;
    Microsoft::WRL::ComPtr<ID3D12Resource> customEffectParamsCB_;
    CustomEffectParams* mappedCustomEffectParams_ = nullptr;

    // 単一パス用フォールバック PSO
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> psos_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> finalPsos_;

    // 統合ポストプロセス (Uber-Shader) 用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> combinedPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalCombinedPSO_;

    // 分離可能フィルタ用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> smoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalSmoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalGaussianBlurPSO_;

    Microsoft::WRL::ComPtr<ID3D12Resource> smoothingCB_;
    SmoothingParams* mappedSmoothing_ = nullptr;
    SmoothingParams smoothingParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> gaussianCB_;
    GaussianParams* mappedGaussian_ = nullptr;
    GaussianParams gaussianParams_;

    // 重力波ディストーション演出用状態
    GravityDistortionParams gravityDistortionParams_{};
    bool gravityDistortionActive_ = false;
    float gravityDistortionTimer_ = 0.0f;
    float gravityDistortionDuration_ = 0.5f;
    float gravityDistortionBaseIntensity_ = 0.08f;
    float gravityDistortionMaxRadius_ = 1.2f;

    // 統合定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> combinedCB_;
    CombinedParams* mappedCombined_ = nullptr;
    CombinedParams combinedParams_{};
    uint32_t combinedBufferOffset_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> bindlessCB_;
    BindlessParams* mappedBindless_ = nullptr;
    uint32_t bindlessBufferOffset_ = 0;

    uint32_t depthSrvIndex_ = 0;
    uint32_t normalSrvIndex_ = 0;
    uint32_t materialSrvIndex_ = 0;
    uint32_t velocitySrvIndex_ = 0;
    uint32_t dissolveNoiseIndex_[2]{0};

    std::unique_ptr<PostProcessRunner> runner_;
};
