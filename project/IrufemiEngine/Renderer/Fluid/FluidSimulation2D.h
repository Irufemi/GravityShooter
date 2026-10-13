#pragma once

#include "Renderer/Compute/IComputeTask.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

class DirectXCommon;

/**
 * @class FluidSimulation2D
 * @brief 2D GPGPU 非圧縮性 Navier-Stokes 流体シミュレーター (Eulerian Grid)
 * @details Jos Stam の Stable Fluids アルゴリズムを DirectX 12 Compute Shader 上で完全実装。
 *          半ラグランジュ移流、外力・染料注入、渦度閉じ込め（Vorticity Confinement）、
 *          および Jacobi 反復による圧力ポアソン方程式の求解を毎フレーム実行し、
 *          高品位な流速場（Velocity Field）と濃度場（Density/Color Field）を提供します。
 */
class FluidSimulation2D : public IComputeTask {
public: // 内部定数・構造体
    /**
     * @struct InjectionRequest
     * @brief フレームごとの外力・染料注入リクエスト
     */
    struct InjectionRequest {
        Irufemi::Vector2 posUV{0.5f, 0.5f};      //!< 注入中心UV (0.0 - 1.0)
        Irufemi::Vector2 velocity{0.0f, 0.0f};    //!< 注入速度ベクトル (UV/sec)
        Irufemi::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f}; //!< 注入カラー・濃度 (RGBA)
        float radius = 0.05f;                    //!< 影響半径 (UV空間)
        float vortexStrength = 0.0f;             //!< 重力特異点（回転渦）強度
        float pullStrength = 0.0f;               //!< 重力特異点（吸引力）強度
        float type = 0.0f;                       //!< 0: 通常速度, 1: 染料, 2: 重力特異点
    };

    /**
     * @struct SimulationParams
     * @brief 定数バッファ (HLSL b0 整合用)
     */
    struct alignas(256) SimulationParams {
        Irufemi::Vector2 resolution{256.0f, 256.0f};
        Irufemi::Vector2 invResolution{1.0f / 256.0f, 1.0f / 256.0f};
        float deltaTime = 1.0f / 60.0f;
        float dissipationVel = 0.995f;
        float dissipationDen = 0.990f;
        float vorticityScale = 2.5f;
        float pad[2]{};
    };

    /**
     * @struct InjectionParamsCB
     * @brief 外力注入定数バッファ (HLSL b1 整合用)
     */
    struct alignas(256) InjectionParamsCB {
        Irufemi::Vector2 injectPosUV{0.5f, 0.5f};
        Irufemi::Vector2 injectVelocity{0.0f, 0.0f};
        Irufemi::Vector4 injectColor{1.0f, 1.0f, 1.0f, 1.0f};
        float injectRadius = 0.05f;
        float vortexStrength = 0.0f;
        float pullStrength = 0.0f;
        float injectType = 0.0f;
    };

public: // メンバ関数
    FluidSimulation2D() = default;
    ~FluidSimulation2D() override;

    /**
     * @brief 流体シミュレーターの初期化
     * @param[in] dxCommon DirectX 共通基盤へのポインタ
     * @param[in] width グリッド横解像度 (推奨: 256)
     * @param[in] height グリッド縦解像度 (推奨: 256)
     */
    void Initialize(DirectXCommon* dxCommon, uint32_t width = 256, uint32_t height = 256);

    /**
     * @brief 毎フレームの更新（パラメータ更新・外力キュー管理）
     * @param[in] deltaTime フレーム間経過時間 (秒)
     */
    void Update(float deltaTime);

    /**
     * @brief Compute Shader の一括ディスパッチ（IComputeTask 実装）
     * @details DrawManagerのExecuteComputePasses()により毎フレーム描画前に呼び出されます。
     */
    void DispatchCompute() override;

    /**
     * @brief 流体場に局所的な速度ベクトル（力）を注入する
     * @param[in] uv 注入位置 (0.0 - 1.0)
     * @param[in] velocity 速度ベクトル (UV/sec)
     * @param[in] radius 影響半径 (UV空間)
     */
    void AddForce(const Irufemi::Vector2& uv, const Irufemi::Vector2& velocity, float radius);

    /**
     * @brief 流体場に局所的な染料・星雲エネルギー（濃度）を注入する
     * @param[in] uv 注入位置 (0.0 - 1.0)
     * @param[in] color 注入するカラー・濃度 (RGBA)
     * @param[in] radius 影響半径 (UV空間)
     */
    void AddDensity(const Irufemi::Vector2& uv, const Irufemi::Vector4& color, float radius);

    /**
     * @brief 重力特異点（引力＋回転スワール）を局所注入する
     * @param[in] uv 特異点中心 (0.0 - 1.0)
     * @param[in] pullStrength 吸引力強度
     * @param[in] swirlStrength 渦巻き回転強度
     * @param[in] radius 影響半径
     */
    void AddGravityVortex(const Irufemi::Vector2& uv, float pullStrength, float swirlStrength, float radius);

    /**
     * @brief 描画シェーダー用 濃度テクスチャ SRV を取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetDensitySRV() const;

    /**
     * @brief 描画シェーダー用 速度テクスチャ SRV を取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetVelocitySRV() const;

    /**
     * @brief 速度減衰率を設定する [0.90 - 1.00]
     */
    void SetVelocityDissipation(float factor) {
        dissipationVel_ = factor;
    }

    /**
     * @brief 濃度減衰率を設定する [0.90 - 1.00]
     */
    void SetDensityDissipation(float factor) {
        dissipationDen_ = factor;
    }

    /**
     * @brief 渦度閉じ込め強度を設定する [0.0 - 5.0]
     */
    void SetVorticityScale(float scale) {
        vorticityScale_ = scale;
    }

    /**
     * @brief Jacobi 圧力反復回数を設定する (推奨: 15 - 25)
     */
    void SetJacobiIterations(uint32_t iterations) {
        jacobiIterations_ = iterations;
    }

private: // 内部ヘルパー
    void CreateTextures();
    void CreateDescriptors();
    void CreatePipelines();

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    uint32_t width_ = 256;
    uint32_t height_ = 256;
    float deltaTime_ = 1.0f / 60.0f;
    float dissipationVel_ = 0.995f;
    float dissipationDen_ = 0.990f;
    float vorticityScale_ = 2.5f;
    uint32_t jacobiIterations_ = 20;

    // ピンポンインデックス
    uint32_t velocityPingPong_ = 0;
    uint32_t densityPingPong_ = 0;
    uint32_t pressurePingPong_ = 0;

    // テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> velocityTex_[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> densityTex_[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> pressureTex_[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> divergenceTex_;

    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> globalParamsCB_;
    SimulationParams* mappedGlobalParams_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> injectParamsCB_;
    InjectionParamsCB* mappedInjectParams_ = nullptr;

    // エンジンの統一 DescriptorPool (SRV / UAV) インデックス
    uint32_t velocitySrvIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t densitySrvIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t pressureSrvIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t divergenceSrvIndex_ = 0xFFFFFFFFu;

    uint32_t velocityUavIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t densityUavIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t pressureUavIndices_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    uint32_t divergenceUavIndex_ = 0xFFFFFFFFu;

    // パイプライン (エンジンの dxCommon_->GetComputeRootSignature() を共有)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAdvect_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoInject_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoVorticity_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoDivergence_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoJacobi_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoProject_;

    // フレームごとの外力注入キュー
    std::vector<InjectionRequest> injectionQueue_;
};
