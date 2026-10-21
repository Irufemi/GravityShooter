/**
 * @file FluidCommon.hlsli
 * @brief 2D GPGPU 流体シミュレーション（Navier-Stokes）共通定数バッファおよびサンプラー定義
 */

#ifndef FLUID_COMMON_HLSLI
#define FLUID_COMMON_HLSLI

// 静的サンプラー (DXRootSignatureManager で共通ComputeRootSignatureに設定された s0)
SamplerState gSamplerLinear : register(s0);

// 共通シミュレーション定数バッファ (register b0 -> ComputeRootSlot::CBV_b0)
cbuffer FluidGlobalParams : register(b0) {
    float2 gResolution;       //!< グリッド解像度 (例: 256, 256)
    float2 gInvResolution;    //!< 1.0 / 解像度
    float  gDeltaTime;        //!< フレーム経過時間 (秒)
    float  gDissipationVel;   //!< 速度減衰率 (0.990 - 0.999)
    float  gDissipationDen;   //!< 濃度減衰率 (0.980 - 0.995)
    float  gVorticityScale;   //!< 渦度閉じ込め強度 (0.0 - 4.0)
    float2 gPad0;             //!< 16バイトアライメントパディング
};

// 外力・染料注入パラメータ (register b1 -> ComputeRootSlot::CBV_b1)
cbuffer InjectionParams : register(b1) {
    float2 gInjectPosUV;      //!< 注入中心UV (0.0 - 1.0)
    float2 gInjectVelocity;   //!< 注入速度ベクトル (UV/sec)
    float4 gInjectColor;      //!< 注入カラー・濃度 (RGBA)
    float  gInjectRadius;     //!< 注入影響半径 (UV空間)
    float  gVortexStrength;   //!< 重力特異点（回転渦）強度
    float  gPullStrength;     //!< 重力特異点（吸引力）強度
    float  gInjectType;       //!< 0: 通常力, 1: 染料, 2: 重力特異点
};

#include "Noise.hlsli"

// ============================================================================
// プロシージャル流体制御ライブラリ (Procedural Fluid Motion Library)
// ============================================================================

/**
 * @brief スカラーポテンシャル場 ψ(p, t) を計算する
 */
float EvaluateFluidPotential(float2 p, float time)
{
    float2 p1 = p * 1.1f + float2(time * 0.15f, sin(time * 0.25f) * 0.35f);
    float2 p2 = p * 2.2f - float2(cos(time * 0.20f) * 0.30f, time * 0.12f);
    return snoise(p1) * 0.65f + snoise(p2) * 0.35f;
}

/**
 * @brief 非発散カール流体ベクトル場 V = (dpsi/dy, -dpsi/dx) を計算する
 * @details 数学的に Divergence = 0（質量保存・非圧縮流体）を満たすため、
 *          不自然な消失や圧縮を起こさず、本物の流体渦・うねりを生成します。
 */
float2 EvaluateFluidCurl(float2 p, float time)
{
    const float eps = 0.015f;
    float psi_y1 = EvaluateFluidPotential(p + float2(0.0f, eps), time);
    float psi_y0 = EvaluateFluidPotential(p - float2(0.0f, eps), time);
    float psi_x1 = EvaluateFluidPotential(p + float2(eps, 0.0f), time);
    float psi_x0 = EvaluateFluidPotential(p - float2(eps, 0.0f), time);

    float dpsi_dy = (psi_y1 - psi_y0) / (2.0f * eps);
    float dpsi_dx = (psi_x1 - psi_x0) / (2.0f * eps);

    return float2(dpsi_dy, -dpsi_dx);
}

/**
 * @brief カール流体場による有機的な二段階流体移流（Advection）座標を計算する
 * @param p アスペクト比補正後のUV座標
 * @param time アニメーション時間
 * @param driftVelocity 全体の緩やかな漂流速度（左右・上下の基底流動）
 * @param curlStrength カール渦による揺らめき・うねりの強度
 * @return 流体移流によって有機的に揺らめく空間座標
 */
float2 AdvectFluidCoords(float2 p, float time, float2 driftVelocity, float curlStrength = 0.35f)
{
    // 1. 基底の緩やかな漂流（一直線ではなく、緩やかなサイン波のうねりを持たせる）
    float2 driftedP = p + driftVelocity * time + float2(sin(time * 0.4f) * 0.08f, cos(time * 0.3f) * 0.06f);

    // 2. カールノイズ流体場による一次移流（1st Advection Step）
    float2 v1 = EvaluateFluidCurl(driftedP * 0.85f, time * 0.5f);
    float2 advectedP1 = driftedP + v1 * curlStrength;

    // 3. 多段移流（2nd Advection Step: 流体が折り重なるS字の波打ち）
    float2 v2 = EvaluateFluidCurl(advectedP1 * 1.4f + float2(0.5f, 0.5f), time * 0.7f);
    return driftedP + (v1 * 0.6f + v2 * 0.4f) * curlStrength;
}

#endif // FLUID_COMMON_HLSLI
