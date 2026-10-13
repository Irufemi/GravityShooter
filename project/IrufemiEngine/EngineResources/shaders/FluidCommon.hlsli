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

#endif // FLUID_COMMON_HLSLI
