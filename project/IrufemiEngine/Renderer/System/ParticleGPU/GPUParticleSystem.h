#pragma once
#include "Renderer/System/Core/IRenderable.h"

#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Type/PerFrame.h"
#include "Core/Type/PerView.h"
#include "Core/Type/PrimitiveType.h"
#include "Core/Type/BlendMode.h"
#include "Renderer/Pipeline/PSOManager.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Renderer/Compute/IComputeTask.h"
#include <random>
#include "RHI/DirectX12/ConstantBuffer.h"
#include "Core/System/ResourceHandle.h"

// 前方宣言
class DrawManager;
class TextureManager;
class Camera;
class IrufemiEngine;
class Line3DBatch;

#include "RHI/DirectX12/DirectXCommon.h"

/**
 * @struct ParticleCS
 * @brief コンピュートシェーダ（CS）側で扱う粒子データ構造体
 */
struct ParticleCS {
    Irufemi::Vector3 translate; ///< 位置
    uint32_t type;              ///< 0: 親, 1: Trail, 2: Death
    Irufemi::Vector3 scale;     ///< スケール
    float lifeTime;             ///< 寿命（秒）
    Irufemi::Vector3 velocity;  ///< 速度
    float currentTime;          ///< 現在の経過時間
    Irufemi::Vector4 color;     ///< 色

    // 拡張パラメータ
    Irufemi::Vector3 rotation;    ///< 回転角
    float trailTimer;             ///< Trail放出タイマー
    Irufemi::Vector3 rotateSpeed; ///< 回転速度
    uint32_t emitterIndex;
    Irufemi::Vector3 startScale; ///< 開始スケール
    uint32_t billboardMode;
    Irufemi::Vector3 endScale; ///< 終了スケール
    uint32_t atlasSize;
    Irufemi::Vector4 startColor; ///< 開始色
    Irufemi::Vector4 endColor;   ///< 終了色
    Irufemi::Vector4 midColor;   ///< 中間色
    Irufemi::Vector3 midScale;   ///< 中間スケール
    float midPoint;              ///< 中間タイミング (0.0~1.0)
};

#include "Renderer/Data/Material.h"

/**
 * @struct GPUParticleEmitter
 * @brief パーティクル放出器の設定（統合版）
 * @details HLSL側の GPUParticleEmitter と構造を一致させています。
 */
struct GPUParticleEmitter {
    // float4 x 1
    uint32_t type = 0; ///< 0: Irufemi::Sphere, 1: Beam, 2: Ring, 3: Irufemi::Cylinder, 4: Box, 5: Hemisphere
    float translateX = 0, translateY = 0, translateZ = 0; ///< 放出中心位置

    // float4 x 2
    float emissionRate = 0.0f;    ///< 1秒あたりの連続放出数
    float emissionResidue = 0.0f; ///< 端数繰り越し用
    float padFreqTime = 0.0f;     ///< 未使用/アライメント用
    int32_t emit = 0;             ///< 1: 放出許可, 0: 停止

    // float4 x 3
    float radius = 1.0f;                                  ///< Irufemi::Sphere/Ring/Cylinder用: 半径
    float directionX = 0, directionY = 0, directionZ = 1; ///< Beam用: 方向

    // float4 x 4
    float spread = 0.1f;   ///< Beam用: 広がり
    float velocity = 1.0f; ///< Beam用: 速度
    float minLife = 1.0f;  ///< 最小寿命
    float maxLife = 1.0f;  ///< 最大寿命

    // float4 x 5
    float startScaleMinX = 1, startScaleMinY = 1, startScaleMinZ = 1; ///< 開始スケール最小
    float pad0 = 0;
    // float4 x 6
    float startScaleMaxX = 1, startScaleMaxY = 1, startScaleMaxZ = 1; ///< 開始スケール最大
    float pad1 = 0;
    // float4 x 7
    float endScaleMinX = 1, endScaleMinY = 1, endScaleMinZ = 1; ///< 終了スケール最小
    float pad2 = 0;
    // float4 x 8
    float endScaleMaxX = 1, endScaleMaxY = 1, endScaleMaxZ = 1; ///< 終了スケール最大
    float pad3 = 0;

    // float4 x 9
    float startColorMinR = 1, startColorMinG = 1, startColorMinB = 1, startColorMinA = 1; ///< 開始色最小
    // float4 x 10
    float startColorMaxR = 1, startColorMaxG = 1, startColorMaxB = 1, startColorMaxA = 1; ///< 開始色最大
    // float4 x 11
    float endColorMinR = 1, endColorMinG = 1, endColorMinB = 1, endColorMinA = 1; ///< 終了色最小
    // float4 x 12
    float endColorMaxR = 1, endColorMaxG = 1, endColorMaxB = 1, endColorMaxA = 1; ///< 終了色最大

    // float4 x 13
    uint32_t colorMode = 0; ///< カラーモード
    float gravity = 0.0f;   ///< 重力
    float damping = 0.0f;   ///< 空気抵抗
    uint32_t billboardMode = 1;

    // float4 x 14
    uint32_t burstCount = 0;
    float jitter = 0.0f;
    uint32_t atlasRows = 1;
    uint32_t atlasCols = 1;

    // float4 x 15
    float groundHeight = -100.0f;
    float bounce = 0.5f;
    float attractorStrength = 0.0f;
    uint32_t randomSeed = 0;

    // float4 x 16
    float attractorPosX = 0, attractorPosY = 0, attractorPosZ = 0;
    uint32_t enableRandomRotation = 0;

    // float4 x 17
    float areaSizeX = 10, areaSizeY = 10, areaSizeZ = 10;
    uint32_t enableTrail = 0;

    // float4 x 18
    uint32_t enableDeathEmit = 0;
    float trailFrequency = 0.05f;
    uint32_t showDebugArea = 1;
    uint32_t enableLighting = 0;

    // float4 x 19
    float midColorMinR = 1, midColorMinG = 1, midColorMinB = 1, midColorMinA = 1;
    // float4 x 20
    float midColorMaxR = 1, midColorMaxG = 1, midColorMaxB = 1, midColorMaxA = 1;
    // float4 x 21
    float midScaleMinX = 1, midScaleMinY = 1, midScaleMinZ = 1;
    float pad9 = 0;
    // float4 x 22
    float midScaleMaxX = 1, midScaleMaxY = 1, midScaleMaxZ = 1;
    float midPoint = 0.0f;

    // float4 x 23
    /// @brief 前フレームのエミッター位置 (X, Y, Z)
    float prevTranslateX = 0, prevTranslateY = 0, prevTranslateZ = 0;
    /// @brief メモリ整列用パディング
    float pad10 = 0;
};

/**
 * @struct ParticleField
 * @brief 空間に配置される力場 (重力、引力、竜巻など) の設定
 */
struct ParticleField {
    uint32_t type = 0; // 0: Directional (Gravity/Wind), 1: Point Attractor, 2: Vortex
    float strength = 0.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;

    Irufemi::Vector3 position = {0.0f, 0.0f, 0.0f};
    float range = 0.0f; // 0 means infinite

    Irufemi::Vector3 direction = {0.0f, -1.0f, 0.0f};
    float falloff = 1.0f; // 減衰率

    Irufemi::Vector3 axis = {0.0f, 1.0f, 0.0f}; // Vortex用の回転軸
    float pad2 = 0.0f;
};

/**
 * @class GPUParticleSystem
 * @brief Compute Shader を使用した、大量の粒子を GPU 上でシミュレーション・描画するクラス
 */
class GPUParticleSystem : public IComputeTask, public IRenderable {
public:
    friend class GPUParticleManager;
    static const uint32_t kMaxEmitters = 2048;
    static const uint32_t kMaxFields = 64;

    GPUParticleSystem();
    ~GPUParticleSystem();

    /**
     * @brief グローバルフィールドデータへのポインタを設定する
     * @param[in] fields フィールドデータのvectorポインタ
     */
    void SetGlobalFields(const std::vector<ParticleField>* fields) {
        globalFields_ = fields;
    }

    /** @name 初期化・更新・描画 */
    ///@{
    void DispatchCompute() override;
    void Setup(class RenderGraphBuilder& builder) override;
    /**
     * @brief Initialize を実行する。
     */
    void Initialize(const std::string& textureName = "resources/circle.png");
    /**
     * @brief Update を実行する。
     */
    void Update();
    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief Debug を実行する。
     */
    void Debug();
    ///@}

    /** @name 再生制御 */
    ///@{
    void Play(uint32_t emitterIndex = 0) {
        isPlaying_ = true;
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].emit = 1;
        }
        totalTime_ = 0.0f;
    }
    /**
     * @brief Stop を実行する。
     */
    void Stop(uint32_t emitterIndex = 0) {
        isPlaying_ = false;
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].emit = 0;
        }
    }
    /**
     * @brief Pause を実行する。
     */
    void Pause() {
        isPlaying_ = false;
    }
    /**
     * @brief Resume を実行する。
     */
    void Resume() {
        isPlaying_ = true;
    }
    /**
     * @brief Clear を実行する。
     */
    void Clear();

    /**
     * @brief Loop を設定する。
     * @param[in] loop 設定する Loop の値
     */
    void SetLoop(bool loop) {
        isLooping_ = loop;
    }
    /**
     * @brief Duration を設定する。
     * @param[in] duration 設定する Duration の値
     */
    void SetDuration(float duration) {
        duration_ = duration;
    }
    /**
     * @brief CullingEnabled を設定する。
     * @param[in] enable 設定する CullingEnabled の値
     */
    void SetCullingEnabled(bool enable) {
        isCullingEnabled_ = enable;
    }
    /**
     * @brief Emit を実行する。
     */
    void Emit(uint32_t count, uint32_t emitterIndex = 0);
    ///@}

    /** @name 汎用エミッター設定 */
    ///@{
    /** @brief 粒子の発生ON/OFF */
    void SetEmit(bool emit, uint32_t emitterIndex = 0);
    /** @brief パーティクルの基本色を設定 */
    void SetColor(const Irufemi::Vector4& color) {
        cpuMaterialData_.color = color;
    }
    /** @brief 3Dメッシュ形状を設定 */
    void SetPrimitive(Irufemi::PrimitiveType type);
    /** @brief ビルボードのON/OFF */
    void SetBillboard(bool isBillboard, uint32_t emitterIndex = 0);

    /**
     * @brief 速度方向に引き伸ばすビルボード設定
     */
    void SetVelocityAligned(bool isAligned, uint32_t emitterIndex = 0);
    /** @brief 使用するテクスチャを切り替える */
    void SetTexture(const std::string& textureFilePath);

    /** @brief UVの変換行列（スケール・スクロール用）を設定する */
    void SetUVTransform(const Irufemi::Matrix4x4& transform) {
        cpuMaterialData_.uvTransform = transform;
    }
    /** @brief 中心部の白丸対策などでSamplerをCLAMPにするか設定する */
    void SetUseClampSampler(bool useClamp) {
        cpuMaterialData_.useClampSampler = useClamp ? 1 : 0;
    }
    /** @brief アルファテストのしきい値（この値以下のアルファを持つピクセルを破棄）を設定する */
    void SetAlphaReference(float alphaRef) {
        cpuMaterialData_.alphaReference = alphaRef;
    }

    /**
     * @brief パーティクルのスケール（開始時と終了時）を動的に設定する
     * @param startMin 開始時の最小スケール
     * @param startMax 開始時の最大スケール
     * @param endMin 終了時の最小スケール
     * @param endMax 終了時の最大スケール
     */
    void SetParticleScale(const Irufemi::Vector3& startMin, const Irufemi::Vector3& startMax,
                          const Irufemi::Vector3& endMin, const Irufemi::Vector3& endMax, uint32_t emitterIndex = 0);

    /**
     * @brief パーティクルの中間スケール（3段階変化用）を設定する
     * @param midMin 中間時の最小スケール
     * @param midMax 中間時の最大スケール
     * @param midPoint 中間到達タイミング (0.0~1.0)
     */
    void SetMidScale(const Irufemi::Vector3& midMin, const Irufemi::Vector3& midMax, float midPoint,
                     uint32_t emitterIndex = 0);

    /**
     * @brief パーティクルの色（開始時と終了時、オプションで中間色）を動的に設定する
     */
    void SetParticleColor(const Irufemi::Vector4& startMin, const Irufemi::Vector4& startMax,
                          const Irufemi::Vector4& endMin, const Irufemi::Vector4& endMax, uint32_t emitterIndex = 0);

    /**
     * @brief MidColor を設定する。
     * @param[in] midMin 設定する MidColor の値
     * @param[in] midMax 設定する MidColor の値
     * @param[in] midPoint 設定する MidColor の値
     * @param[in] 0 設定する MidColor の値
     */
    void SetMidColor(const Irufemi::Vector4& midMin, const Irufemi::Vector4& midMax, float midPoint,
                     uint32_t emitterIndex = 0);

    /**
     * @brief パーティクルの寿命（最小・最大）を設定する
     * @param minLife 最小寿命（秒）
     * @param maxLife 最大寿命（秒）
     */
    void SetParticleLife(float minLife, float maxLife, uint32_t emitterIndex = 0);

    /** @brief 放出速度を設定する */
    void SetVelocity(float velocity, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].velocity = velocity;
        }
    }
    /** @brief 放出方向を設定する */
    void SetDirection(const Irufemi::Vector3& dir, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].directionX = dir.x;
            emittersData_[emitterIndex].directionY = dir.y;
            emittersData_[emitterIndex].directionZ = dir.z;
        }
    }
    /** @brief 座標のゆらぎ（Jitter）を設定する */
    void SetJitter(float jitter, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].jitter = jitter;
        }
    }
    /**
     * @brief EnableRandomRotation を設定する。
     * @param[in] enable 設定する EnableRandomRotation の値
     * @param[in] 0 設定する EnableRandomRotation の値
     */
    void SetEnableRandomRotation(bool enable, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].enableRandomRotation = enable ? 1 : 0;
        }
    }
    /** @brief ビルボードモードの設定 (0: なし, 1: 通常ビルボード, 2: 速度方向ビルボード) */
    void SetBillboardMode(uint32_t mode, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].billboardMode = mode;
        }
    }
    /** @brief 粒子の拡散力（Sphere等の放射方向の広がり/強度）を設定 */
    void SetSpread(float spread, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].spread = spread;
        }
    }

    /** @brief Trail（軌跡）機能の有効化 */
    void SetEnableTrail(bool enable, float frequency = 0.05f, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].enableTrail = enable ? 1 : 0;
            emittersData_[emitterIndex].trailFrequency = frequency;
        }
    }
    /** @brief 消滅時連鎖（Death Emit）機能の有効化 */
    void SetEnableDeathEmit(bool enable, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].enableDeathEmit = enable ? 1 : 0;
        }
    }

    /** @name 描画設定（パイプライン） */
    ///@{
    void SetBlendMode(Irufemi::BlendMode blend) {
        selectedBlend_ = blend;
    }
    /**
     * @brief UnscaledTime を設定する。
     * @param[in] isUnscaled 設定する UnscaledTime の値
     */
    void SetUnscaledTime(bool isUnscaled) {
        isUnscaledTime_ = isUnscaled;
    }
    /**
     * @brief DepthWrite を設定する。
     * @param[in] depth 設定する DepthWrite の値
     */
    void SetDepthWrite(PSOManager::DepthWrite depth) {
        selectedDepth_ = depth;
    }
    /**
     * @brief Cull を設定する。
     * @param[in] cull 設定する Cull の値
     */
    void SetCull(PSOManager::CullMode cull) {
        selectedCull_ = cull;
    }
    /**
     * @brief CustomPSO を設定する。
     * @param[in] psoName 設定する CustomPSO の値
     */
    void SetCustomPSO(const std::string& psoName) {
        customPSOName_ = psoName;
    }
    /**
     * @brief EnableLighting を設定する。
     * @param[in] val 設定する EnableLighting の値
     */
    void SetEnableLighting(bool val) {
        cpuMaterialData_.enableLighting = val ? 1 : 0;
    }

    /** @name タイプ別エミッター設定 */
    ///@{
    void SetSphereEmitter(const Irufemi::Vector3& pos, float radius, float emissionRate, uint32_t emitterIndex = 0);
    /**
     * @brief HemisphereEmitter を設定する。
     * @param[in] pos 設定する HemisphereEmitter の値
     * @param[in] radius 設定する HemisphereEmitter の値
     * @param[in] emissionRate 設定する HemisphereEmitter の値
     * @param[in] 0 設定する HemisphereEmitter の値
     */
    void SetHemisphereEmitter(const Irufemi::Vector3& pos, float radius, float emissionRate, uint32_t emitterIndex = 0);
    /**
     * @brief BeamEmitter を設定する。
     * @param[in] pos 設定する BeamEmitter の値
     * @param[in] direction 設定する BeamEmitter の値
     * @param[in] radius 設定する BeamEmitter の値
     * @param[in] velocity 設定する BeamEmitter の値
     * @param[in] spread 設定する BeamEmitter の値
     * @param[in] emissionRate 設定する BeamEmitter の値
     * @param[in] 0 設定する BeamEmitter の値
     */
    void SetBeamEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& direction, float radius, float velocity,
                        float spread, float emissionRate, uint32_t emitterIndex = 0);

    /** @name アトラス・物理挙動設定 */
    /** @brief テクスチャアトラス（Flipbook）設定 */
    void SetTextureAtlas(uint32_t rows, uint32_t cols, uint32_t emitterIndex);
    /** @brief 床衝突設定 */
    void SetGroundCollision(float height, float bounce, uint32_t emitterIndex = 0);
    /** @brief アトラクター（引力源）設定 */
    void SetAttractor(const Irufemi::Vector3& pos, float strength, uint32_t emitterIndex = 0);

    /**
     * @brief リングエミッターの設定
     * @param pos 中心位置
     * @param radius 半径
     * @param thickness 厚み
     * @param emissionRate 1秒あたりの連続放出数
     */
    void SetRingEmitter(const Irufemi::Vector3& pos, float radius, float thickness, float emissionRate,
                        uint32_t emitterIndex = 0);

    /**
     * @brief 円柱エミッターの設定
     * @param pos 中心位置
     * @param direction 円柱の方向
     * @param radius 半径
     * @param height 高さ
     * @param emissionRate 1秒あたりの連続放出数
     */
    void SetCylinderEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& direction, float radius, float height,
                            float emissionRate, uint32_t emitterIndex = 0);

    /**
     * @brief ボックス（直方体）エミッターの設定
     * @param pos 中心位置
     * @param size ボックスの各軸のサイズ（幅、高さ、奥行き）
     * @param emissionRate 1秒あたりの連続放出数
     */
    void SetBoxEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& size, float emissionRate,
                       uint32_t emitterIndex);

    /**
     * @brief メッシュエミッターの設定
     */
    void SetMeshEmitter(const Irufemi::Vector3& pos, D3D12_GPU_VIRTUAL_ADDRESS vbAddress, uint32_t vertexCount,
                        float emissionRate, uint32_t emitterIndex);
    ///@}

    /** @brief 開始色を設定する */
    void SetStartColor(const Irufemi::Vector4& minColor, const Irufemi::Vector4& maxColor, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].startColorMinR = minColor.x;
            emittersData_[emitterIndex].startColorMinG = minColor.y;
            emittersData_[emitterIndex].startColorMinB = minColor.z;
            emittersData_[emitterIndex].startColorMinA = minColor.w;
            emittersData_[emitterIndex].startColorMaxR = maxColor.x;
            emittersData_[emitterIndex].startColorMaxG = maxColor.y;
            emittersData_[emitterIndex].startColorMaxB = maxColor.z;
            emittersData_[emitterIndex].startColorMaxA = maxColor.w;
        }
    }

    /** @brief 終了色を設定する */
    void SetEndColor(const Irufemi::Vector4& minColor, const Irufemi::Vector4& maxColor, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].endColorMinR = minColor.x;
            emittersData_[emitterIndex].endColorMinG = minColor.y;
            emittersData_[emitterIndex].endColorMinB = minColor.z;
            emittersData_[emitterIndex].endColorMinA = minColor.w;
            emittersData_[emitterIndex].endColorMaxR = maxColor.x;
            emittersData_[emitterIndex].endColorMaxG = maxColor.y;
            emittersData_[emitterIndex].endColorMaxB = maxColor.z;
            emittersData_[emitterIndex].endColorMaxA = maxColor.w;
        }
    }
    /** @brief 重力を設定する */
    void SetGravity(float gravity, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].gravity = gravity;
        }
    }
    /** @brief 空気抵抗（ダンピング）を設定する */
    void SetDamping(float damping, uint32_t emitterIndex = 0) {
        if (emitterIndex < emittersData_.size()) {
            emittersData_[emitterIndex].damping = damping;
        }
    }

    /** @name 静的マネージャ設定 */
    ///@{
    static void SetDXCommon(DirectXCommon* dxCommon) {
        dxCommon_ = dxCommon;
    }
    /**
     * @brief DrawManager を設定する。
     * @param[in] drawManager 設定する DrawManager の値
     */
    static void SetDrawManager(DrawManager* drawManager) {
        drawManager_ = drawManager;
    }
    /**
     * @brief TextureManager を設定する。
     * @param[in] textureManager 設定する TextureManager の値
     */
    static void SetTextureManager(TextureManager* textureManager) {
        textureManager_ = textureManager;
    }
    /**
     * @brief Engine を設定する。
     * @param[in] engine 設定する Engine の値
     */
    static void SetEngine(IrufemiEngine* engine) {
        engine_ = engine;
    }
    /**
     * @brief Engine を取得する。
     * @return 取得された Engine
     */
    static IrufemiEngine* GetEngine() {
        return engine_;
    }

    /**
     * @brief TextureManager を取得する。
     * @return 取得された TextureManager
     */
    static TextureManager* GetTextureManager() {
        return textureManager_;
    }
    ///@}

private:
    /**
     * @brief DrawAABB を実行する。
     */
    void DrawAABB(const Irufemi::Vector3& min, const Irufemi::Vector3& max, const Irufemi::Vector4& color);
    /**
     * @brief DrawCircle を実行する。
     */
    void DrawCircle(const Irufemi::Vector3& center, float radius, const Irufemi::Vector3& axis,
                    const Irufemi::Vector4& color);
    /**
     * @brief DrawSphereWireframe を実行する。
     */
    void DrawSphereWireframe(const Irufemi::Vector3& center, float radius, const Irufemi::Vector4& color);
    /**
     * @brief DrawCylinderWireframe を実行する。
     */
    void DrawCylinderWireframe(const Irufemi::Vector3& center, const Irufemi::Vector3& direction, float radius,
                               float height, const Irufemi::Vector4& color);

    /**
     * @brief UpdateDebugLines を実行する。
     */
    void UpdateDebugLines();

    /**
     * @brief DirectX12の各種リソースとディスクリプタ（SRV/UAV）の生成を行う
     */
    void CreateBuffersAndViews();

    /**
     * @brief コンピュートシェーダ（初期化、更新、放出）を実行する
     * @param commandList D3D12コマンドリスト
     */
    void DispatchComputeShaders(ID3D12GraphicsCommandList* commandList);

    /** @name ImGuiデバッグ用描画分割 */
    ///@{

    ///@}

    static DirectXCommon* dxCommon_;
    static DrawManager* drawManager_;
    static TextureManager* textureManager_;
    static IrufemiEngine* engine_;

    static const uint32_t kMaxParticles = 32768;

    /** @name 再生状態フラグ */
    bool isPlaying_ = true;
    bool isLooping_ = true;
    float duration_ = -1.0f; // -1: 無限
    float totalTime_ = 0.0f;
    float timeSinceStop_ = 0.0f;

    /** @name エミッターリソース */
    ///@{
    std::vector<GPUParticleEmitter> emittersData_;
    std::vector<D3D12_GPU_VIRTUAL_ADDRESS> meshVertexBuffers_;

    // CPU-GPU共有用バッファ（kMaxFramesInFlight個）
    Microsoft::WRL::ComPtr<ID3D12Resource> emittersResource_[3]; // 3 = kMaxFramesInFlight
    GPUParticleEmitter* emittersMappedData_[3] = {nullptr, nullptr, nullptr};
    uint32_t emittersSrvIndex_[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    D3D12_GPU_DESCRIPTOR_HANDLE emittersSrvHandleGPU_[3]{};

    /** @name Fieldリソース */
    const std::vector<ParticleField>* globalFields_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> fieldsResource_[3];
    ParticleField* fieldsMappedData_[3] = {nullptr, nullptr, nullptr};
    uint32_t fieldsSrvIndex_[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    D3D12_GPU_DESCRIPTOR_HANDLE fieldsSrvHandleGPU_[3]{};
    ///@}

    /** @name 各種リソース */
    ///@{
    Irufemi::PerFrame perFrameDataStruct_{};
    Irufemi::PerFrame* perFrameData_ = &perFrameDataStruct_; // CPU側のマスターへのポインタ
    ConstantBuffer<Irufemi::PerFrame> perFrameBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE perFrameSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE perFrameSrvHandleGPU_{};

    ParticleCS* particleData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE particleUavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE particleSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_{};

    int32_t* freeListIndex_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE freeListIndexUavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE freeListIndexSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexSrvHandleGPU_{};

    int32_t* freeList_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE freeListUavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE freeListSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE freeListSrvHandleGPU_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> sortResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE sortUavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE sortUavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE sortSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE sortSrvHandleGPU_{};
    uint32_t sortIndex_ = 0xFFFFFFFF;
    uint32_t sortSrvIndex_ = 0xFFFFFFFF;

    ConstantBuffer<Irufemi::PerView> perViewBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};                            // NEW: インデックスバッファ
    uint32_t indexCount_ = 0;                                              // NEW: インデックス数
    Irufemi::PrimitiveType primitiveType_ = Irufemi::PrimitiveType::Plane; // 現在の形状

    ConstantBuffer<Material> materialBuffer_;
    Material cpuMaterialData_{
        {1.0f, 1.0f, 1.0f, 1.0f},                         // color
        0,                                                // enableLighting
        1,                                                // hasTexture
        0,                                                // lightingMode
        0.0f,                                             // environmentCoefficient
        {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, // uvTransform
        0.0f,                                             // metallic
        0.0f,                                             // roughness
        0,                                                // useClampSampler
        0.0f                                              // alphaReference
    };

    ResourceHandle textureHandle_;
    int selectedTextureIndex_ = 0;

    uint32_t perFrameSrvIndex_ = 0xFFFFFFFF;
    uint32_t particleUavIndex_ = 0xFFFFFFFF;
    uint32_t particleSrvIndex_ = 0xFFFFFFFF;
    uint32_t freeListIndexUavIndex_ = 0xFFFFFFFF;
    uint32_t freeListUavIndex_ = 0xFFFFFFFF;
    ///@}

    bool isCullingEnabled_ = false;
    bool isCulled_ = false;
    bool isInitializedCS_ = false;
    bool isSortResourceInitialized_ = false;
    bool needsUpdateCS_ = false;
    bool isUnscaledTime_ = false;

    // --- State tracking for multiple pass rendering ---
    bool isCsDispatchedThisFrame_ = false;
    uint32_t lastUpdateFrame_ = static_cast<uint32_t>(-1);

    Irufemi::BlendMode selectedBlend_ = Irufemi::BlendMode::kBlendModeAdd;
    PSOManager::DepthWrite selectedDepth_ = PSOManager::DepthWrite::Disable;
    PSOManager::CullMode selectedCull_ = PSOManager::CullMode::None;
    std::string customPSOName_ = "";

    std::unique_ptr<Line3DBatch> debugLineRegion_;
    bool showEmitterArea_ = true;
};
