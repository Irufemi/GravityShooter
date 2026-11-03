#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <d3d12.h>
#include <wrl/client.h>
#ifndef __IInspectable_INTERFACE_DEFINED__
// ComPtr の一部のテンプレート展開で IInspectable が必要になる場合がある
typedef struct IInspectable IInspectable;
#endif
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <mutex>

/**
 * @enum PostProcessMode
 * @brief ポストプロセスの各モードを定義する列挙型
 */
enum class PostProcessMode {
    None,                  ///< 何も適用しない
    Grayscale,             ///< グレースケール
    Sepia,                 ///< セピア調
    Vignette,              ///< ビネット（画面端を暗くする）
    Smoothing,             ///< 平滑化（ぼかし）
    GaussianFilter,        ///< ガウスぼかし
    DepthBasedOutline,     ///< 深度バッファを使用したアウトライン抽出
    RadialBlur,            ///< 放射状ぼかし
    Dissolve,              ///< ディゾルブ（ノイズテクスチャによる消失演出）
    Noise,                 ///< ランダムノイズ粒子
    HSV,                   ///< HSV色空間による色調整
    ToneMapping,           ///< トーンマッピング（ACES）
    Fade,                  ///< フェード（指定色への塗りつぶし）
    Slide,                 ///< スライド（ワイプ演出）
    Bloom,                 ///< ブルーム（高輝度抽出による発光）
    Glitch,                ///< グリッチ（ノイズや色収差による映像の乱れ）
    DualKawaseBlur,        ///< カワセブラー（軽量で広範囲なぼかし）
    LuminanceBasedOutline, ///< 輝度ベースのアウトライン抽出（2Dトゥーン調）
    Pixelation,            ///< ピクセレーション（ドット絵化・モザイク）
    Pointillism,           ///< 点描画・印象派風フィルタ
    Posterization,         ///< ポスタリゼーション（トゥーン調階調化）
    NightVision,           ///< 暗視ゴーグル風エフェクト
    Kaleidoscope,          ///< 万華鏡・複眼エフェクト
    ChromaticAberration,   ///< 色収差
    DisplacementMap,       ///< 画面の歪み・陽炎
    DirectionalBlur,       ///< 方向ブラー
    Halftone,              ///< ハーフトーン（網点・コミック調）
    DepthOfField,          ///< 被写界深度（DoF）
    LightShafts,           ///< ゴッドレイ（光の筋）
    GravityDistortion,     ///< 重力波・空間歪曲（法線波紋＋色収差）
    BlackHoleSupernova, ///< 自機重力特異点崩壊＆超新星爆発（事象の地平面＋GRAVITY DAZE色収差）
};

class DirectXCommon;
class IrufemiEngine;
class PostProcessRunner;

namespace Irufemi {
class BloomEffect;
class LightShaftsEffect;
class SeparableBlurEffect;
class KawaseBlurEffect;
class CombinedPostEffect;
} // namespace Irufemi

/**
 * @struct PostProcessModeInfo
 * @brief エフェクトとその優先度を保持する内部用構造体
 */
struct PostProcessModeInfo {
    PostProcessMode mode;
    int priority;

    bool operator<(const PostProcessModeInfo& other) const {
        return priority < other.priority;
    }
};

/**
 * @class PostProcessManager
 * @brief ポストプロセス（画面全体にかけるエフェクト）を管理するクラス。
 *
 * マルチパスレンダリングに対応しており、複数のエフェクトをスタックに追加して重ね掛けできます。
 *
 * @par 推奨される適用順序:
 * プロの現場でも、以下のような順序で適用することで意図した映像表現になります。
 * 1. 色調補正系 (ToneMapping, Grayscale, Sepia, HSV 等)
 * 2. 空間・ぼかし系 (Smoothing, GaussianFilter, RadialBlur 等)
 * 3. 画面演出系 (Vignette, Noise, Glitch, Dissolve 等)
 * 4. 画面遷移系 (Fade, Slide)
 *
 * @par シーンでの使用例:
 * @code
 * // 1. マネージャーの取得
 * auto* pp = engine->GetPostProcessManager();
 *
 * // 2. エフェクトのリセットと追加
 * pp->ClearActiveModes();
 * pp->AddActiveMode(PostProcessMode::DepthBasedOutline);
 * pp->AddActiveMode(PostProcessMode::Noise);
 *
 * // 3. 各エフェクトのパラメータ調整
 * pp->GetNoiseParams().intensity = 0.2f;
 * @endcode
 */
enum class EffectLayer {
    PreUI, // 3Dシーンや背景にかかる（UIにはかからない）
    PostUI // UIを含む画面全体にかかる
};

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
    using GravityDistortionParams = Irufemi::GravityDistortionParams;
    using BlackHoleSupernovaParams = Irufemi::BlackHoleSupernovaParams;
    using LightShaftsParams = Irufemi::LightShaftsParams;
    using CombinedParams = Irufemi::CombinedParams;
    using BindlessParams = Irufemi::BindlessParams;


public:
    /**
     * @brief ポストプロセスの初期化
     * @param engine エンジンポインタ
     * @param dxCommon DirectX基盤クラス
     * @param rtvFormat 最終的な出力先のRTVフォーマット
     */
    void Initialize(IrufemiEngine* engine, DirectXCommon* dxCommon, DXGI_FORMAT rtvFormat);
    template <typename T>
    T* GetEffect() const;
    BloomParams& GetBloomParamsDelegated();
    const BloomParams& GetBloomParamsDelegated() const;
    LightShaftsParams& GetLightShaftsParamsDelegated();
    const LightShaftsParams& GetLightShaftsParamsDelegated() const;

    /**
     * @brief フレーム描画開始処理（リングバッファオフセットのリセットおよびカスタムパラメータの同期）
     */
    void BeginFrame();

    /**
     * @brief エンジンへの参照を取得
     */
    IrufemiEngine* GetEngine() const {
        return engine_;
    }

    /**
     * @brief 更新処理
     * @param totalTime 累計時間（ノイズ等のアニメーションに使用）
     */
    void Update(float totalTime);

    /**
     * @brief 描画実行（マルチパス対応）
     * @param commandList コマンドリスト
     * @param srcTexture 元となるレンダリングテクスチャ（メインの描画結果）
     * @param rtvHandle 最終的な出力先（バックバッファ）のRTV
     */
    void Draw(ID3D12GraphicsCommandList* commandList, class RenderTexture* srcTexture,
              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, Layer layer = Layer::PreUI, bool isBackBufferTarget = false,
              const D3D12_VIEWPORT* targetViewport = nullptr, const D3D12_RECT* targetScissor = nullptr);

    /**
     * @brief PostProcessRunner のインスタンスを取得する
     */
    PostProcessRunner* GetRunner() const {
        return runner_.get();
    }

    /**
     * @brief Bindless 定数バッファのリングオフセットを安全にインクリメントする
     * @return 上限に達しておらず安全に進められた場合は true、上限超過時は false（警告ログ出力・クランプ）
     */
    bool TryIncrementBindlessOffset();

    /**
     * @brief Combined 定数バッファのリングオフセットを安全にインクリメントする
     * @return 上限に達しておらず安全に進められた場合は true、上限超過時は false（警告ログ出力・クランプ）
     */
    bool TryIncrementCombinedOffset();

    /**
     * @brief Bindless 定数バッファを割り当ててGPUアドレスを取得する
     * @param mainTextureIndex メインテクスチャのSRVインデックス
     * @param extraTextureIndex 追加テクスチャのSRVインデックス
     * @param maskTextureIndex マスクテクスチャのSRVインデックス
     * @return 割り当てられた定数バッファのGPU仮想アドレス
     */
    D3D12_GPU_VIRTUAL_ADDRESS AllocateBindlessParams(uint32_t mainTextureIndex, uint32_t extraTextureIndex = 0,
                                                     uint32_t maskTextureIndex = 0);

    /**
     * @brief オブジェクト固有IDに紐づく永続スロット（1〜255）に個別エフェクトパラメータを登録・更新する (AAA Persistent
     * Slot Allocation)
     * @param objectId オブジェクトの固有インスタンスID (GameObject::GetInstanceID() 等)
     * @param params 個別エフェクトのパラメータ
     * @return 永続インスタンスID (1〜255)
     */
    uint32_t RegisterCustomEffectParams(uint64_t objectId, const CustomEffectParams& params);

    /**
     * @brief 互換用：単一パラメータを登録する
     */
    uint32_t RegisterCustomEffectParams(const CustomEffectParams& params) {
        return RegisterCustomEffectParams(0, params);
    }

    /**
     * @brief オブジェクト破棄時に永続スロットを解放する
     * @param objectId オブジェクトの固有インスタンスID
     */
    void UnregisterCustomEffectParams(uint64_t objectId);

    /**
     * @brief 全永続スロットをリセットする（シーン完全切り替え時用）
     */
    void ResetCustomEffectSlots();

    /**
     * @brief 個別エフェクトの詳細パラメータのリストをクリアする（互換用）
     */
    void ClearCustomEffectParams() {
        // 永続スロット方式では毎フレームの配列消去は行わず、スロットの状態を維持する
    }

    // --- Getters & Setters ---

    /** @brief 描画フェーズに備えて保留中の状態を同期し、ソートする */
    void CommitPendingModes() {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);

        // ソート実行
        std::sort(pendingPreUI_.begin(), pendingPreUI_.end());
        std::sort(pendingPostUI_.begin(), pendingPostUI_.end());

        // Mode のみを取り出して active リストへ
        activePreUI_.clear();
        for (const auto& info : pendingPreUI_) {
            activePreUI_.push_back(info.mode);
        }

        activePostUI_.clear();
        for (const auto& info : pendingPostUI_) {
            activePostUI_.push_back(info.mode);
        }
    }

    /** @brief 現在アクティブなエフェクトスタックを取得 (レイヤー指定) */
    const std::vector<Mode>& GetActiveModes(Layer layer = Layer::PreUI) const {
        return (layer == Layer::PreUI) ? activePreUI_ : activePostUI_;
    }

    /** @brief 全てのアクティブなエフェクト（PreUI + PostUI）を取得（互換性用・デバッグ用） */
    std::vector<Mode> GetAllActiveModes() const {
        std::vector<Mode> all = activePreUI_;
        all.insert(all.end(), activePostUI_.begin(), activePostUI_.end());
        return all;
    }

    /** @brief エフェクトをスタックに追加（カスタム優先度指定可能） */
    void AddActiveMode(Mode mode, int customPriority = -1) {
        AddActiveMode(mode, Layer::PreUI, customPriority);
    }

    void AddActiveMode(Mode mode, Layer layer, int customPriority = -1) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);

        // 既に同じモードが存在するかチェック（重複追加を防ぐ）
        if (layer == Layer::PreUI) {
            auto it = std::find_if(pendingPreUI_.begin(), pendingPreUI_.end(),
                                   [mode](const PostProcessModeInfo& info) { return info.mode == mode; });
            if (it != pendingPreUI_.end()) {
                return;
            }
        } else {
            auto it = std::find_if(pendingPostUI_.begin(), pendingPostUI_.end(),
                                   [mode](const PostProcessModeInfo& info) { return info.mode == mode; });
            if (it != pendingPostUI_.end()) {
                return;
            }
        }

        int priority = (customPriority == -1) ? GetDefaultPriority(mode) : customPriority;
        if (layer == Layer::PreUI) {
            pendingPreUI_.push_back({mode, priority});
        } else {
            // PostUI は全体的に優先度を高くする（既存互換のため9000番台をベースに加算）
            pendingPostUI_.push_back({mode, priority + 9000});
        }
    }

    // -------------------------------------------------------------
    // 静的ヘルパー関数
    // -------------------------------------------------------------

    /**
     * @brief モードのデフォルト実行優先度（Pipeline Order）を取得
     * 小さい数値ほど先に（下層に）描画される
     */
    static int GetDefaultPriority(Mode mode) {
        switch (mode) {
        // Priority 1000: Pre-HDR
        case Mode::DepthBasedOutline:
            return 1100;
        case Mode::RadialBlur:
            return 1200;
        case Mode::DepthOfField:
            return 1300;
        case Mode::LightShafts:
            return 1400;
        case Mode::DualKawaseBlur:
            return 1500;

        // Priority 2000: HDR Effects
        case Mode::Bloom:
            return 2000;

        // Priority 3000: ToneMapping (HDR -> LDR)
        case Mode::ToneMapping:
            return 3000;

        // Priority 4000: LDR Color Grading
        case Mode::HSV:
            return 4000;
        case Mode::Grayscale:
            return 4100;
        case Mode::Sepia:
            return 4200;
        case Mode::Posterization:
            return 4300;

        // Priority 5000: LDR Polish (画面演出・オーバーレイ系)
        case Mode::Vignette:
            return 5000;
        case Mode::Noise:
            return 5100;
        case Mode::Glitch:
            return 5200;
        case Mode::ChromaticAberration:
            return 5300;
        case Mode::Pixelation:
            return 5400;

        default:
            return 9999;
        }
    }

    /**
     * @brief そのエフェクトが深度バッファを必要とするかどうか
     */
    static bool UsesDepthBuffer(Mode mode) {
        return mode == Mode::DepthBasedOutline || mode == Mode::DepthOfField || mode == Mode::LightShafts;
    }

    // -------------------------------------------------------------
    // 初期化・更新・描画
    // -------------------------------------------------------------

    /** @brief 指定したエフェクトをスタックから削除 */
    void RemoveActiveMode(Mode mode) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.erase(std::remove_if(pendingPreUI_.begin(), pendingPreUI_.end(),
                                           [mode](const PostProcessModeInfo& info) { return info.mode == mode; }),
                            pendingPreUI_.end());
        pendingPostUI_.erase(std::remove_if(pendingPostUI_.begin(), pendingPostUI_.end(),
                                            [mode](const PostProcessModeInfo& info) { return info.mode == mode; }),
                             pendingPostUI_.end());
    }

    /** @brief 全てのエフェクトを解除（クリア） */
    void ClearActiveModes() {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.clear();
        pendingPostUI_.clear();
    }

    /**
     * @brief ポストプロセスの完全リセット（シーン遷移時用）
     *
     * 保留中および現在アクティブなエフェクトリストをすべてクリアし、
     * 全パラメータをデフォルト状態に戻します。
     * 各シーンの `Initialize()` または `Finalize()` で呼び出すことを推奨します。
     */
    void Reset(bool clearPostUI = true) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.clear();
        activePreUI_.clear();
        if (clearPostUI) {
            pendingPostUI_.clear();
            activePostUI_.clear();
            ResetCustomEffectSlots();
        }
        ResetAllParams(clearPostUI);
    }

    /** @brief エフェクトスタックを一括設定 (互換性のため PreUI に設定) */
    void SetActiveModes(const std::vector<Mode>& modes) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.clear();
        for (auto mode : modes) {
            pendingPreUI_.push_back({mode, GetDefaultPriority(mode)});
        }
        pendingPostUI_.clear(); // 一括指定時はPostUI側のエフェクトスタックもリセット
    }

    /** @brief エフェクトスタックをレイヤー別に設定 */
    void SetActiveModes(const std::vector<Mode>& preUI, const std::vector<Mode>& postUI) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.clear();
        for (auto mode : preUI) {
            pendingPreUI_.push_back({mode, GetDefaultPriority(mode)});
        }

        pendingPostUI_.clear();
        for (auto mode : postUI) {
            pendingPostUI_.push_back({mode, GetDefaultPriority(mode) + 9000});
        }
    }

    /** @brief 全てのパラメータをデフォルト状態にリセットする */
    void ResetAllParams(bool clearPostUI = true);

    /** @brief 指定したエフェクトが現在有効かチェック */
    bool HasActiveMode(Mode mode) const {
        return std::find(activePreUI_.begin(), activePreUI_.end(), mode) != activePreUI_.end() ||
               std::find(activePostUI_.begin(), activePostUI_.end(), mode) != activePostUI_.end();
    }

    /** @brief 互換性のための単一セット (既存リストをクリアして1つ追加) */
    void SetMode(Mode mode, Layer layer = Layer::PreUI) {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingPreUI_.clear();
        pendingPostUI_.clear();
        if (mode != Mode::None) {
            int priority = GetDefaultPriority(mode);
            if (layer == Layer::PreUI) {
                pendingPreUI_.push_back({mode, priority});
            } else {
                pendingPostUI_.push_back({mode, priority + 9000});
            }
        }
    }

    /** @brief 互換性のための取得 (PreUIの先頭を優先して返す) */
    Mode GetMode() const {
        if (!activePreUI_.empty()) {
            return activePreUI_.front();
        }
        if (!activePostUI_.empty()) {
            return activePostUI_.front();
        }
        return Mode::None;
    }

    // 各エフェクトのパラメータ取得 (シーンからの演出用)
    /**
     * @brief NoiseParams を設定する。
     * @param[in] params 設定する NoiseParams
     */
    void SetNoiseParams(const NoiseParams& params) {
        noiseParams_ = params;
    }
    /**
     * @brief NoiseParams を取得する。
     * @return 取得された NoiseParams
     */
    NoiseParams& GetNoiseParams() {
        return noiseParams_;
    }
    /**
     * @brief NoiseParams を取得する（読み取り専用）。
     * @return 取得された NoiseParams
     */
    const NoiseParams& GetNoiseParams() const {
        return noiseParams_;
    }
    /**
     * @brief VignetteParams を設定する。
     * @param[in] params 設定する VignetteParams
     */
    void SetVignetteParams(const VignetteParams& params) {
        vignetteParams_ = params;
    }
    /**
     * @brief VignetteParams を取得する。
     * @return 取得された VignetteParams
     */
    VignetteParams& GetVignetteParams() {
        return vignetteParams_;
    }
    /**
     * @brief VignetteParams を取得する（読み取り専用）。
     * @return 取得された VignetteParams
     */
    const VignetteParams& GetVignetteParams() const {
        return vignetteParams_;
    }
    /**
     * @brief SmoothingParams を設定する。
     * @param[in] params 設定する SmoothingParams
     */
    void SetSmoothingParams(const SmoothingParams& params) {
        smoothingParams_ = params;
    }
    /**
     * @brief SmoothingParams を取得する。
     * @return 取得された SmoothingParams
     */
    SmoothingParams& GetSmoothingParams() {
        return smoothingParams_;
    }
    /**
     * @brief SmoothingParams を取得する（読み取り専用）。
     * @return 取得された SmoothingParams
     */
    const SmoothingParams& GetSmoothingParams() const {
        return smoothingParams_;
    }
    /**
     * @brief GaussianParams を設定する。
     * @param[in] params 設定する GaussianParams
     */
    void SetGaussianParams(const GaussianParams& params) {
        gaussianParams_ = params;
    }
    /**
     * @brief GaussianParams を取得する。
     * @return 取得された GaussianParams
     */
    GaussianParams& GetGaussianParams() {
        return gaussianParams_;
    }
    /**
     * @brief GaussianParams を取得する（読み取り専用）。
     * @return 取得された GaussianParams
     */
    const GaussianParams& GetGaussianParams() const {
        return gaussianParams_;
    }
    /**
     * @brief RadialBlurParams を設定する。
     * @param[in] params 設定する RadialBlurParams
     */
    void SetRadialBlurParams(const RadialBlurParams& params) {
        radialBlurParams_ = params;
    }
    /**
     * @brief RadialBlurParams を取得する。
     * @return 取得された RadialBlurParams
     */
    RadialBlurParams& GetRadialBlurParams() {
        return radialBlurParams_;
    }
    /**
     * @brief RadialBlurParams を取得する（読み取り専用）。
     * @return 取得された RadialBlurParams
     */
    const RadialBlurParams& GetRadialBlurParams() const {
        return radialBlurParams_;
    }
    /**
     * @brief OutlineParams を設定する。
     * @param[in] params 設定する OutlineParams
     */
    void SetOutlineParams(const OutlineParams& params) {
        outlineParams_ = params;
    }
    /**
     * @brief OutlineParams を取得する。
     * @return 取得された OutlineParams
     */
    OutlineParams& GetOutlineParams() {
        return outlineParams_;
    }
    /**
     * @brief OutlineParams を取得する（読み取り専用）。
     * @return 取得された OutlineParams
     */
    const OutlineParams& GetOutlineParams() const {
        return outlineParams_;
    }
    /**
     * @brief DissolveParams を設定する。
     * @param[in] params 設定する DissolveParams
     */
    void SetDissolveParams(const DissolveParams& params) {
        dissolveParams_ = params;
    }
    /**
     * @brief DissolveParams を取得する。
     * @return 取得された DissolveParams
     */
    DissolveParams& GetDissolveParams() {
        return dissolveParams_;
    }
    /**
     * @brief DissolveParams を取得する（読み取り専用）。
     * @return 取得された DissolveParams
     */
    const DissolveParams& GetDissolveParams() const {
        return dissolveParams_;
    }
    /**
     * @brief HSVParams を設定する。
     * @param[in] params 設定する HSVParams
     */
    void SetHSVParams(const HSVParams& params) {
        hsvParams_ = params;
    }
    /**
     * @brief HSVParams を取得する。
     * @return 取得された HSVParams
     */
    HSVParams& GetHSVParams() {
        return hsvParams_;
    }
    /**
     * @brief HSVParams を取得する（読み取り専用）。
     * @return 取得された HSVParams
     */
    const HSVParams& GetHSVParams() const {
        return hsvParams_;
    }
    /**
     * @brief ToneMappingParams を設定する。
     * @param[in] params 設定する ToneMappingParams
     */
    void SetToneMappingParams(const ToneMappingParams& params) {
        toneMappingParams_ = params;
    }
    /**
     * @brief ToneMappingParams を取得する。
     * @return 取得された ToneMappingParams
     */
    ToneMappingParams& GetToneMappingParams() {
        return toneMappingParams_;
    }
    /**
     * @brief ToneMappingParams を取得する（読み取り専用）。
     * @return 取得された ToneMappingParams
     */
    const ToneMappingParams& GetToneMappingParams() const {
        return toneMappingParams_;
    }
    /**
     * @brief FadeParams を設定する。
     * @param[in] params 設定する FadeParams
     */
    void SetFadeParams(const FadeParams& params) {
        fadeParams_ = params;
    }
    /**
     * @brief FadeParams を取得する。
     * @return 取得された FadeParams
     */
    FadeParams& GetFadeParams() {
        return fadeParams_;
    }
    /**
     * @brief FadeParams を取得する（読み取り専用）。
     * @return 取得された FadeParams
     */
    const FadeParams& GetFadeParams() const {
        return fadeParams_;
    }
    /**
     * @brief SlideParams を設定する。
     * @param[in] params 設定する SlideParams
     */
    void SetSlideParams(const SlideParams& params) {
        slideParams_ = params;
    }
    /**
     * @brief SlideParams を取得する。
     * @return 取得された SlideParams
     */
    SlideParams& GetSlideParams() {
        return slideParams_;
    }
    /**
     * @brief SlideParams を取得する（読み取り専用）。
     * @return 取得された SlideParams
     */
    const SlideParams& GetSlideParams() const {
        return slideParams_;
    }
    /**
     * @brief BloomParams を設定する。
     * @param[in] params 設定する BloomParams
     */
    void SetBloomParams(const BloomParams& params) {
        bloomParams_ = params;
    }
    /**
     * @brief BloomParams を取得する。
     * @return 取得された BloomParams
     */
    BloomParams& GetBloomParams() {
        return GetBloomParamsDelegated();
    }
    /**
     * @brief BloomParams を取得する（読み取り専用）。
     * @return 取得された BloomParams
     */
    const BloomParams& GetBloomParams() const {
        return GetBloomParamsDelegated();
    }
    /**
     * @brief GlitchParams を設定する。
     * @param[in] params 設定する GlitchParams
     */
    void SetGlitchParams(const GlitchParams& params) {
        glitchParams_ = params;
    }
    /**
     * @brief GlitchParams を取得する。
     * @return 取得された GlitchParams
     */
    GlitchParams& GetGlitchParams() {
        return glitchParams_;
    }
    /**
     * @brief GlitchParams を取得する（読み取り専用）。
     * @return 取得された GlitchParams
     */
    const GlitchParams& GetGlitchParams() const {
        return glitchParams_;
    }
    /**
     * @brief DualKawaseBlurParams を取得する。
     * @return 取得された DualKawaseBlurParams
     */
    DualKawaseBlurParams& GetDualKawaseBlurParams();
    const DualKawaseBlurParams& GetDualKawaseBlurParams() const;
    /**
     * @brief LuminanceOutlineParams を取得する。
     * @return 取得された LuminanceOutlineParams
     */
    LuminanceOutlineParams& GetLuminanceOutlineParams() {
        return luminanceOutlineParams_;
    }
    /**
     * @brief PixelationParams を取得する。
     * @return 取得された PixelationParams
     */
    PixelationParams& GetPixelationParams() {
        return pixelationParams_;
    }
    /**
     * @brief PointillismParams を取得する。
     * @return 取得された PointillismParams
     */
    PointillismParams& GetPointillismParams() {
        return pointillismParams_;
    }
    /**
     * @brief PosterizationParams を取得する。
     * @return 取得された PosterizationParams
     */
    PosterizationParams& GetPosterizationParams() {
        return posterizationParams_;
    }
    /**
     * @brief NightVisionParams を取得する。
     * @return 取得された NightVisionParams
     */
    NightVisionParams& GetNightVisionParams() {
        return nightVisionParams_;
    }
    /**
     * @brief KaleidoscopeParams を取得する。
     * @return 取得された KaleidoscopeParams
     */
    KaleidoscopeParams& GetKaleidoscopeParams() {
        return kaleidoscopeParams_;
    }
    /**
     * @brief ChromaticAberrationParams を設定する。
     * @param[in] params 設定する ChromaticAberrationParams
     */
    void SetChromaticAberrationParams(const ChromaticAberrationParams& params) {
        chromaticAberrationParams_ = params;
    }
    /**
     * @brief ChromaticAberrationParams を取得する。
     * @return 取得された ChromaticAberrationParams
     */
    ChromaticAberrationParams& GetChromaticAberrationParams() {
        return chromaticAberrationParams_;
    }
    /**
     * @brief ChromaticAberrationParams を取得する（読み取り専用）。
     * @return 取得された ChromaticAberrationParams
     */
    const ChromaticAberrationParams& GetChromaticAberrationParams() const {
        return chromaticAberrationParams_;
    }
    /**
     * @brief DisplacementMapParams を取得する。
     * @return 取得された DisplacementMapParams
     */
    DisplacementMapParams& GetDisplacementMapParams() {
        return displacementMapParams_;
    }
    /**
     * @brief DirectionalBlurParams を取得する。
     * @return 取得された DirectionalBlurParams
     */
    DirectionalBlurParams& GetDirectionalBlurParams() {
        return directionalBlurParams_;
    }
    /**
     * @brief HalftoneParams を取得する。
     * @return 取得された HalftoneParams
     */
    HalftoneParams& GetHalftoneParams() {
        return halftoneParams_;
    }
    /**
     * @brief DepthOfFieldParams を取得する。
     * @return 取得された DepthOfFieldParams
     */
    DepthOfFieldParams& GetDepthOfFieldParams() {
        return dofParams_;
    }
    /**
     * @brief LightShaftsParams を取得する。
     * @return 取得された LightShaftsParams
     */
    LightShaftsParams& GetLightShaftsParams() {
        return GetLightShaftsParamsDelegated();
    }
    /**
     * @brief GravityDistortionParams を取得する。
     * @return 取得された GravityDistortionParams
     */
    GravityDistortionParams& GetGravityDistortionParams() {
        return gravityDistortionParams_;
    }

    /**
     * @brief BlackHoleSupernovaParams を取得する。
     * @return 取得された BlackHoleSupernovaParams
     */
    BlackHoleSupernovaParams& GetBlackHoleSupernovaParams() {
        return blackHoleSupernovaParams_;
    }
    const BlackHoleSupernovaParams& GetBlackHoleSupernovaParams() const {
        return blackHoleSupernovaParams_;
    }
    void SetBlackHoleSupernovaParams(const BlackHoleSupernovaParams& params) {
        blackHoleSupernovaParams_ = params;
    }

    /**
     * @brief 重力波ディストーションを発火する
     * @param centerUV 歪み中心スクリーンUV (0.0〜1.0)
     * @param intensity 歪みの最大強度
     * @param duration 演出持続時間（秒）
     * @param maxRadius 最大波面半径
     * @param chromaticRatio 色収差比率
     */
    void TriggerGravityDistortion(const Irufemi::Vector2& centerUV, float intensity = 0.10f, float duration = 0.5f,
                                  float maxRadius = 1.3f, float chromaticRatio = 0.035f, float waveWidth = 0.18f);

    /**
     * @brief DissolveNoiseIndex を設定する。
     * @param[in] index 設定する DissolveNoiseIndex の値
     * @param[in] srvIndex 設定する DissolveNoiseIndex の値
     */
    void SetDissolveNoiseIndex(int index, uint32_t srvIndex) {
        if (index >= 0 && index < 2) {
            dissolveNoiseIndex_[index] = srvIndex;
        }
    }

    /**
     * @brief DepthSrvIndex を設定する。
     * @param[in] srvIndex 設定する DepthSrvIndex の値
     */
    void SetDepthSrvIndex(uint32_t srvIndex) {
        depthSrvIndex_ = srvIndex;
    }
    /**
     * @brief NormalSrvIndex を設定する。
     * @param[in] srvIndex 設定する NormalSrvIndex の値
     */
    void SetNormalSrvIndex(uint32_t srvIndex) {
        normalSrvIndex_ = srvIndex;
    }
    /**
     * @brief MaterialSrvIndex を設定する。
     * @param[in] srvIndex 設定する MaterialSrvIndex の値
     */
    void SetMaterialSrvIndex(uint32_t srvIndex) {
        materialSrvIndex_ = srvIndex;
    }
    /**
     * @brief VelocitySrvIndex を設定する。
     * @param[in] srvIndex 設定する VelocitySrvIndex の値
     */
    void SetVelocitySrvIndex(uint32_t srvIndex) {
        velocitySrvIndex_ = srvIndex;
    }

private:
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
     * @param commandList コマンドリスト
     * @param mode ポストプロセスモード
     * @param srcTexture 入力元テクスチャ
     * @param rtvHandle 出力先レンダーターゲットハンドル
     * @param isFinalPass 最終出力パスかどうか
     * @param psoOverride オーバーライド用PSO（省略時は通常PSO）
     */
    void DrawSinglePass(ID3D12GraphicsCommandList* commandList, Mode mode, RenderTexture* srcTexture,
                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalPass = false,
                        ID3D12PipelineState* psoOverride = nullptr,
                        D3D12_GPU_VIRTUAL_ADDRESS cbvOverride = 0);

    /**
     * @brief アップロードヒープ上に定数バッファ用リソースを作成する
     * @param size バッファサイズ（バイト）
     * @return 作成された D3D12 リソース
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(size_t size);

private:
    IrufemiEngine* engine_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    ID3D12Device* device_ = nullptr;
    ID3D12RootSignature* rootSig_ = nullptr;
    DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_UNKNOWN;

    // NOTE: 描画スレッドでのレンダーグラフ実行（CommitPendingModes / GetActiveModes）と
    // ゲームスレッドからのエフェクト追加・削除の排他制御を行い、かつ GetActiveModes での
    // 参照返却パフォーマンスを担保するため、生の std::mutex を意図して維持しています。
    mutable std::mutex modesMutex_;
    std::vector<Mode> activePreUI_;
    std::vector<PostProcessModeInfo> pendingPreUI_;

    std::vector<Mode> activePostUI_;
    std::vector<PostProcessModeInfo> pendingPostUI_;

    // NOTE: GPU定数バッファのマップ領域（mappedCustomEffectParams_）への直接 memcpy と
    // スロット割り当ての同期を行う低レイヤー制御のため、生の std::mutex を意図して維持しています。
    std::mutex customParamsMutex_;
    std::array<CustomEffectParams, kMaxCustomEffectParams> persistentCustomParams_{};
    std::unordered_map<uint64_t, uint32_t> objectToSlotMap_;
    std::vector<uint32_t> freeSlots_;
    Microsoft::WRL::ComPtr<ID3D12Resource> customEffectParamsCB_;
    CustomEffectParams* mappedCustomEffectParams_ = nullptr;

    // PSOs
    struct PipelineSet {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    };
    // モードに対応するPSOを保持 (中間パス用: _UNORM)
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> psos_;
    // 最終パス用 (スワップチェーン等の _SRGB 形式用)
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> finalPsos_;

    // ブルーム専用 PSO (内部のパス用)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomExtractPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomBlurHPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomBlurVPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomCombinePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalBloomCombinePSO_;

    // LightShafts (ゴッドレイ) 専用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> lsExtractPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> lsRadialBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> lsCombinePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalLsCombinePSO_;

    // 統合ポストプロセス用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> combinedPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalCombinedPSO_;

    // 分離可能フィルタ用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> smoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalSmoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalGaussianBlurPSO_;

    // DualKawaseBlur PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dualKawaseDownsamplePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dualKawaseUpsamplePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalDualKawaseUpsamplePSO_;

    // Constant Buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> noiseCB_;
    NoiseParams* mappedNoise_ = nullptr;
    NoiseParams noiseParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vignetteCB_;
    VignetteParams* mappedVignette_ = nullptr;
    VignetteParams vignetteParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> smoothingCB_;
    SmoothingParams* mappedSmoothing_ = nullptr;
    SmoothingParams smoothingParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> gaussianCB_;
    GaussianParams* mappedGaussian_ = nullptr;
    GaussianParams gaussianParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurCB_;
    RadialBlurParams* mappedRadialBlur_ = nullptr;
    RadialBlurParams radialBlurParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> outlineCB_;
    OutlineParams* mappedOutline_ = nullptr;
    OutlineParams outlineParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveCB_;
    DissolveParams* mappedDissolve_ = nullptr;
    DissolveParams dissolveParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> hsvCB_;
    HSVParams* mappedHsv_ = nullptr;
    HSVParams hsvParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> toneMappingCB_;
    ToneMappingParams* mappedToneMapping_ = nullptr;
    ToneMappingParams toneMappingParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> dofCB_;
    DepthOfFieldParams* mappedDof_ = nullptr;
    DepthOfFieldParams dofParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightShaftsCB_;
    LightShaftsParams* mappedLightShafts_ = nullptr;
    LightShaftsParams lightShaftsParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> fadeCB_;
    FadeParams* mappedFade_ = nullptr;
    FadeParams fadeParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> slideCB_;
    SlideParams* mappedSlide_ = nullptr;
    SlideParams slideParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> bloomCB_;
    BloomParams* mappedBloom_ = nullptr;
    BloomParams bloomParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> glitchCB_;
    GlitchParams* mappedGlitch_ = nullptr;
    GlitchParams glitchParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> dualKawaseCB_;
    DualKawaseBlurParams* mappedDualKawase_ = nullptr;
    DualKawaseBlurParams dualKawaseParams_;

    LuminanceOutlineParams luminanceOutlineParams_;
    PixelationParams pixelationParams_;
    PointillismParams pointillismParams_;
    PosterizationParams posterizationParams_;
    NightVisionParams nightVisionParams_;
    KaleidoscopeParams kaleidoscopeParams_;
    ChromaticAberrationParams chromaticAberrationParams_;
    DisplacementMapParams displacementMapParams_;
    DirectionalBlurParams directionalBlurParams_;
    HalftoneParams halftoneParams_;
    GravityDistortionParams gravityDistortionParams_;
    BlackHoleSupernovaParams blackHoleSupernovaParams_;

    bool gravityDistortionActive_ = false;
    float gravityDistortionTimer_ = 0.0f;
    float gravityDistortionDuration_ = 0.5f;
    float gravityDistortionBaseIntensity_ = 0.08f;
    float gravityDistortionMaxRadius_ = 1.2f;

    Microsoft::WRL::ComPtr<ID3D12Resource> combinedCB_;
    CombinedParams* mappedCombined_ = nullptr;
    CombinedParams combinedParams_;
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

    // 状態追跡用は上に移動済み
};
