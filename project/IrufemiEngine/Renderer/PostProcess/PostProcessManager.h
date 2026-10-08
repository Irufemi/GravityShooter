#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "RHI/DirectX12/RenderTexture.h"
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
};

class DirectXCommon;
class IrufemiEngine;
class PostProcessRunner;

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

public:
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

    /**
     * @struct CustomEffectParams
     * @brief 個別オブジェクトに適用する詳細エフェクト用パラメータ（汎用構造体）
     */
    struct CustomEffectParams {
        Irufemi::Vector4 color1 = {1.0f, 1.0f, 1.0f, 1.0f}; // Edge Color, Slide Color, etc.
        Irufemi::Vector4 color2 = {0.0f, 0.0f, 0.0f, 1.0f}; // Background Color
        float param1 = 0.0f;                                // Threshold, Intensity, etc.
        float param2 = 0.0f;                                // Edge Range, Radius, etc.
        float param3 = 0.0f;                                // Noise Type, Softness, etc.
        float param4 = 0.0f;                                // Time, Angle, etc.

        bool operator==(const CustomEffectParams& other) const {
            return color1 == other.color1 && color2 == other.color2 && param1 == other.param1 &&
                   param2 == other.param2 && param3 == other.param3 && param4 == other.param4;
        }
        bool operator!=(const CustomEffectParams& other) const {
            return !(*this == other);
        }
    };

    // 定数バッファの最大サイズ（256個まで）
    static constexpr uint32_t kMaxCustomEffectParams = 256;

    /**
     * @struct NoiseParams
     * @brief ノイズエフェクト用パラメータ
     */
    struct NoiseParams {
        float intensity = 0.5f; ///< ノイズの強度 (0.0 ~ 1.0)
        float time = 0.0f;      ///< 時間経過（内部で更新される）
    };

    /**
     * @struct VignetteParams
     * @brief ビネットエフェクト用パラメータ
     */
    struct VignetteParams {
        Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< ビネットの色 (RGB)
        float radius = 0.8f;                               ///< 減衰の開始半径 (0.0~1.5)
        float softness = 0.5f;                             ///< 減衰の柔らかさ (0.0~1.0)
        float pad[2];                                      // 16バイトアライメント用パディング
    };

    /**
     * @struct SmoothingParams
     * @brief 平滑化エフェクト用パラメータ
     */
    struct SmoothingParams {
        Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        int32_t kernelSize = 3;                    ///< カーネルサイズ (奇数推奨)
        float pad;
    };

    /**
     * @struct GaussianParams
     * @brief ガウスぼかし用パラメータ
     */
    struct GaussianParams {
        Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        float sigma = 2.0f;                        ///< 標準偏差（ぼけ具合）
        int32_t kernelSize = 3;                    ///< カーネルサイズ (奇数推奨)
    };

    /**
     * @struct RadialBlurParams
     * @brief 放射状ぼかし用パラメータ
     */
    struct RadialBlurParams {
        Irufemi::Vector2 center = {0.5f, 0.5f}; ///< ぼかしの中心点 (UV空間 0.0 ~ 1.0)
        float blurWidth = 0.01f;                ///< ぼかしの幅
        int32_t numSamples = 10;                ///< サンプル数
    };

    /**
     * @struct OutlineParams
     * @brief アウトラインエフェクト用パラメータ
     */
    struct OutlineParams {
        float intensity = 6.0f;    ///< アウトラインの強度
        int32_t maskMaxRadius = 5; ///< マスクベースアウトラインの最大探索半径
        float pad[2];
        Irufemi::Matrix4x4 projectionInverse; ///< 逆投影行列 (自動でセットされる)
    };

    /**
     * @struct LuminanceOutlineParams
     * @brief 輝度ベースのアウトラインエフェクト用パラメータ
     */
    struct LuminanceOutlineParams {
        float threshold = 0.5f;                                   ///< 輪郭抽出のしきい値
        float pad[3];                                             // 16バイトアライメント
        Irufemi::Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 1.0f}; ///< アウトラインの色
    };

    /**
     * @struct PixelationParams
     * @brief ピクセレーション（モザイク）エフェクト用パラメータ
     */
    struct PixelationParams {
        float pixelSize = 4.0f; ///< 1ドットを構成するピクセル数（解像度ダウン係数）
        float pad[3];
    };

    /**
     * @struct PointillismParams
     * @brief 点描画エフェクト用パラメータ
     */
    struct PointillismParams {
        float strokeSize = 10.0f; ///< 筆の荒さ
        float colorSteps = 8.0f;  ///< 色の階調数
        float pad[2];
    };

    /**
     * @struct DissolveParams
     * @brief ディゾルブエフェクト用パラメータ
     */
    struct DissolveParams {
        Irufemi::Vector4 edgeColor = {1.0f, 1.0f, 1.0f, 1.0f};       ///< 境界線の色 (無彩色化)
        Irufemi::Vector4 backgroundColor = {0.0f, 0.0f, 0.0f, 1.0f}; ///< 背景色 (追加)
        float threshold = 0.0f;                                      ///< 消失しきい値 (0.0 ~ 1.0)
        float edgeRange = 0.03f;                                     ///< 境界線の幅
        int32_t noiseType = 0; ///< 使用するノイズテクスチャのインデックス (0 or 1)
    };

    /**
     * @struct HSVParams
     * @brief HSVエフェクト用パラメータ
     */
    struct HSVParams {
        float hue = 0.0f;        ///< 色相オフセット (-1.0 ~ 1.0)
        float saturation = 0.0f; ///< 彩度オフセット (-1.0 ~ 1.0)
        float value = 0.0f;      ///< 明度オフセット (-1.0 ~ 1.0)
    };

    /**
     * @struct ToneMappingParams
     * @brief トーンマッピングエフェクト用パラメータ
     */
    struct ToneMappingParams {
        float exposure = 1.0f; ///< 露出補正 (0.0 ~ )
    };

    /**
     * @struct FadeParams
     * @brief フェードエフェクト用パラメータ
     */
    struct FadeParams {
        Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< フェード色
        float intensity = 0.0f;                            ///< 強度 (0.0 ~ 1.0)
    };

    /**
     * @struct SlideParams
     * @brief スライドエフェクト用パラメータ
     */
    struct SlideParams {
        Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f}; ///< スライドの色
        float threshold = 0.0f;                            ///< 進行度 (0.0 ~ 1.0)
    };

    /**
     * @struct BloomParams
     * @brief ブルームエフェクト用パラメータ
     */
    struct BloomParams {
        Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        float threshold = 0.8f;                    ///< 高輝度抽出のしきい値
        float sigma = 3.0f;                        ///< ぼかしの強さ
        float intensity = 1.0f;                    ///< ブルームの強度
        int32_t kernelSize = 21;                   ///< ぼかしのカーネルサイズ
    };

    /**
     * @struct GlitchParams
     * @brief グリッチエフェクト用パラメータ
     */
    struct GlitchParams {
        float intensity = 1.0f;        ///< グリッチの強さ
        float time = 0.0f;             ///< 時間経過（内部で更新される）
        float edgeMaskStrength = 0.0f; ///< 画面端にかける強さ
        float probability = 0.4f;      ///< グリッチが発生する確率 (0.0 ~ 1.0)

        float blockSizeX = 16.0f; ///< ブロックの横分割数
        float blockSizeY = 32.0f; ///< ブロックの縦分割数
        float offsetBase = 0.03f; ///< 基本の横ズレ幅
        float offsetMax = 0.15f;  ///< グリッチ時の最大横ズレ幅

        float rgbShiftBase = 0.01f;      ///< 基本のRGBズレ幅
        float rgbShiftMax = 0.02f;       ///< グリッチ時の最大RGBズレ幅
        float scanlineFreq = 800.0f;     ///< スキャンラインの周波数（細かさ）
        float scanlineIntensity = 0.05f; ///< スキャンラインの濃さ

        Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 0.0f}; ///< rgb = 色, a = ブレンド強度
    };

    /**
     * @struct DualKawaseBlurParams
     * @brief カワセブラー用パラメータ
     */
    struct DualKawaseBlurParams {
        float blurRadius = 1.0f;    ///< ぼかしのサンプリング半径オフセット
        float intensity = 1.0f;     ///< ブラーの最終的な強度
        int32_t iterationCount = 4; ///< ダウン/アップサンプルの繰り返し回数（最大8等）
        float pad;                  // 16バイトアライメント用
    };

    /**
     * @struct PosterizationParams
     * @brief ポスタリゼーション（階調化）用パラメータ
     */
    struct PosterizationParams {
        float colorSteps = 8.0f; ///< 階調の段数（少ないほどベタ塗りになる）
    };

    /**
     * @struct NightVisionParams
     * @brief 暗視ゴーグル風エフェクト用パラメータ
     */
    struct NightVisionParams {
        float intensity = 0.5f; ///< ノイズとスキャンラインの強度
        float time = 0.0f;      ///< 時間経過（内部で更新される）
        float pad[2];
    };

    /**
     * @struct KaleidoscopeParams
     * @brief 万華鏡エフェクト用パラメータ
     */
    struct KaleidoscopeParams {
        float segments = 6.0f; ///< 分割数
        float pad[3];
    };

    /**
     * @struct ChromaticAberrationParams
     * @brief 色収差エフェクト用パラメータ
     */
    struct ChromaticAberrationParams {
        float intensity = 0.05f; ///< 色ズレの幅
        float pad[3];
    };

    /**
     * @struct DisplacementMapParams
     * @brief 画面の歪み・陽炎エフェクト用パラメータ
     */
    struct DisplacementMapParams {
        float intensity = 0.05f; ///< 歪みの強さ
        float timeScale = 1.0f;  ///< うねりの速度
        float time = 0.0f;       ///< 時間
        float pad;
    };

    /**
     * @struct DirectionalBlurParams
     * @brief 方向ブラーエフェクト用パラメータ
     */
    struct DirectionalBlurParams {
        Irufemi::Vector2 direction = {1.0f, 0.0f}; ///< ブラーの方向
        float strength = 0.05f;                    ///< ブラーの強さ
        int samples = 10;                          ///< サンプル数
        // 16バイト境界は { Irufemi::Vector2(8), float(4), int(4) } = 16バイト なのでpad不要
    };

    /**
     * @struct HalftoneParams
     * @brief ハーフトーンエフェクト用パラメータ
     */
    struct HalftoneParams {
        float scale = 150.0f;    ///< ドットの細かさ
        float angle = 0.785398f; ///< ドットの回転角 (45度 = 約0.785ラジアン)
        float blend = 1.0f;      ///< 適用強度
        float pad;
    };

    /**
     * @struct DepthOfFieldParams
     * @brief 被写界深度エフェクト用パラメータ
     */
    struct DepthOfFieldParams {
        float focusDistance = 10.0f; ///< ピントが合う距離 (View Z)
        float focusRange = 5.0f;     ///< ピントが合う範囲 (前後)
        float blurSize = 10.0f;      ///< 最大ブラーサイズ (ピクセル半径)
        int samples = 16;            ///< ブラーのサンプリング数
    };

    /**
     * @struct LightShaftsParams
     * @brief ゴッドレイ（光の筋）エフェクト用パラメータ
     */
    struct LightShaftsParams {
        Irufemi::Vector2 lightScreenPos = {0.5f, 0.5f}; ///< 光源のスクリーン座標 (0.0~1.0)
        float density = 1.0f;                           ///< サンプリング密度
        float decay = 0.95f;                            ///< 減衰率
        float weight = 0.5f;                            ///< 重み
        float exposure = 1.0f;                          ///< 露出
        int32_t samples = 64;                           ///< サンプリング数
        float pad;                                      ///< パディング
    };

    /**
     * @struct CombinedParams
     * @brief 統合ポストプロセス用定数バッファ構造体
     */
    struct CombinedParams {
        int32_t effectCount;
        int32_t useMask;
        int32_t pad0[2]; // HLSLの int4[4] の開始位置（16バイト境界）に合わせるためのパディング
        int32_t effects[16];

        // Vignette
        Irufemi::Vector4 vignetteColor;
        float vignetteRadius;
        float vignetteSoftness;
        float pad1[2];

        // Noise
        float noiseIntensity;
        float noiseTime;
        float pad_noise[2]; // HLSLの float4(dissolveEdgeColor) 用に16バイト境界までパディング

        // Dissolve
        Irufemi::Vector4 dissolveEdgeColor;
        Irufemi::Vector4 dissolveBackgroundColor;
        float dissolveThreshold;
        float dissolveEdgeRange;

        // HSV
        float hsvHue;
        float hsvSaturation;
        float hsvValue;

        // ToneMapping
        float toneMappingExposure;
        float pad2[2]; // HLSLの float4(fadeColor) 用に16バイト境界までパディング

        // Fade
        Irufemi::Vector4 fadeColor;
        float fadeIntensity;
        float pad3[3]; // HLSLの float4(slideColor) 用に16バイト境界までパディング

        // Slide
        Irufemi::Vector4 slideColor;
        float slideThreshold;
        float pad4[3]; // HLSLの float4x4(projectionInverse) 用に16バイト境界までパディング

        // Outline
        Irufemi::Matrix4x4 projectionInverse;
        float outlineIntensity;
        int32_t maskOutlineMaxRadius;
        float pad_outline[2]; // HLSLの float2(radialBlurCenter) 用に16バイト境界までパディング

        // RadialBlur
        Irufemi::Vector2 radialBlurCenter;
        float radialBlurWidth;
        int32_t radialBlurSamples;

        // Glitch
        float glitchIntensity;
        float glitchTime;
        float glitchEdgeMaskStrength;
        float glitchProbability;

        float glitchBlockSizeX;
        float glitchBlockSizeY;
        float glitchOffsetBase;
        float glitchOffsetMax;

        float glitchRgbShiftBase;
        float glitchRgbShiftMax;
        float glitchScanlineFreq;
        float glitchScanlineIntensity;
        Irufemi::Vector4 glitchColor;

        // LuminanceBasedOutline
        Irufemi::Vector4 luminanceOutlineColor;
        float luminanceOutlineThreshold;
        float pad_lumOutline[3];

        // Pixelation
        float pixelationSize;
        float pad_pixelation[3];

        // Pointillism
        float pointillismStrokeSize;
        float pointillismColorSteps;
        float pad_pointillism[2];

        // Posterization
        float posterizationSteps;
        float pad_posterization[3];

        // NightVision
        float nightVisionIntensity;
        float nightVisionTime;
        float pad_nightVision[2];

        // Kaleidoscope
        float kaleidoscopeSegments;
        float pad_kaleidoscope[3];

        // ChromaticAberration
        float chromaticAberrationIntensity;
        float pad_chromaticAberration[3];

        // DisplacementMap
        float displacementMapIntensity;
        float displacementMapTime;
        float displacementMapTimeScale;
        float pad_displacementMap;

        // DirectionalBlur
        Irufemi::Vector2 directionalBlurDirection;
        float directionalBlurStrength;
        int directionalBlurSamples;

        // Halftone
        float halftoneScale;
        float halftoneAngle;
        float halftoneBlend;
        float pad_halftone;

        // DepthOfField
        float dofFocusDistance;
        float dofFocusRange;
        float dofBlurSize;
        int32_t dofSamples;

        // [Bindless]
        uint32_t mainTextureIndex;
        uint32_t extraTextureIndex;
        uint32_t maskTextureIndex;
        uint32_t padding_bindless;

        // 256-byte alignment padding (Current size: 624 bytes, padded to 768 bytes)
        uint32_t alignPadding[36];
    };

    struct BindlessParams {
        uint32_t mainTextureIndex;
        uint32_t extraTextureIndex;
        uint32_t maskTextureIndex;
        uint32_t normalTextureIndex;
        uint32_t materialTextureIndex;
        uint32_t velocityTextureIndex;
        uint32_t padding[58]; // 256バイトアライメント (64 * 4 = 256)
    };

public:
    /**
     * @brief ポストプロセスの初期化
     * @param engine エンジンポインタ
     * @param dxCommon DirectX基盤クラス
     * @param rtvFormat 最終的な出力先のRTVフォーマット
     */
    void Initialize(IrufemiEngine* engine, DirectXCommon* dxCommon, DXGI_FORMAT rtvFormat);

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
     * @param workspace Transient Resource が割り当てられた作業用領域
     */
    void Draw(ID3D12GraphicsCommandList* commandList, class RenderTexture* srcTexture,
              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const PostProcessWorkspace& workspace, Layer layer = Layer::PreUI,
              bool isBackBufferTarget = false);

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
     * @brief NoiseParams を取得する。
     * @return 取得された NoiseParams
     */
    NoiseParams& GetNoiseParams() {
        return noiseParams_;
    }
    /**
     * @brief VignetteParams を取得する。
     * @return 取得された VignetteParams
     */
    VignetteParams& GetVignetteParams() {
        return vignetteParams_;
    }
    /**
     * @brief SmoothingParams を取得する。
     * @return 取得された SmoothingParams
     */
    SmoothingParams& GetSmoothingParams() {
        return smoothingParams_;
    }
    /**
     * @brief GaussianParams を取得する。
     * @return 取得された GaussianParams
     */
    GaussianParams& GetGaussianParams() {
        return gaussianParams_;
    }
    /**
     * @brief RadialBlurParams を取得する。
     * @return 取得された RadialBlurParams
     */
    RadialBlurParams& GetRadialBlurParams() {
        return radialBlurParams_;
    }
    /**
     * @brief OutlineParams を取得する。
     * @return 取得された OutlineParams
     */
    OutlineParams& GetOutlineParams() {
        return outlineParams_;
    }
    /**
     * @brief DissolveParams を取得する。
     * @return 取得された DissolveParams
     */
    DissolveParams& GetDissolveParams() {
        return dissolveParams_;
    }
    /**
     * @brief HSVParams を取得する。
     * @return 取得された HSVParams
     */
    HSVParams& GetHSVParams() {
        return hsvParams_;
    }
    /**
     * @brief ToneMappingParams を取得する。
     * @return 取得された ToneMappingParams
     */
    ToneMappingParams& GetToneMappingParams() {
        return toneMappingParams_;
    }
    /**
     * @brief FadeParams を取得する。
     * @return 取得された FadeParams
     */
    FadeParams& GetFadeParams() {
        return fadeParams_;
    }
    /**
     * @brief SlideParams を取得する。
     * @return 取得された SlideParams
     */
    SlideParams& GetSlideParams() {
        return slideParams_;
    }
    /**
     * @brief BloomParams を取得する。
     * @return 取得された BloomParams
     */
    BloomParams& GetBloomParams() {
        return bloomParams_;
    }
    /**
     * @brief GlitchParams を取得する。
     * @return 取得された GlitchParams
     */
    GlitchParams& GetGlitchParams() {
        return glitchParams_;
    }
    /**
     * @brief DualKawaseBlurParams を取得する。
     * @return 取得された DualKawaseBlurParams
     */
    DualKawaseBlurParams& GetDualKawaseBlurParams() {
        return dualKawaseParams_;
    }
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
     * @brief ChromaticAberrationParams を取得する。
     * @return 取得された ChromaticAberrationParams
     */
    ChromaticAberrationParams& GetChromaticAberrationParams() {
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
        return lightShaftsParams_;
    }

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
                        ID3D12PipelineState* psoOverride = nullptr);

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

    Mode mode_ = Mode::None; // 互換性用（内部では不使用にする）

    std::mutex modesMutex_;
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

    // 状態追跡用は上に移動済み
};
