#pragma once
#include <d3d12.h>

/**
 * @enum RootSlot
 * @brief ルートパラメータのスロットインデックスを定義します。
 * @details HLSL側での register(bX) や register(tX) とは独立した、
 *          ID3D12GraphicsCommandList::SetGraphicsRoot... で使用するインデックスです。
 *          現在は既存の構成を維持するため、マジックナンバーをそのままEnum化しています。
 */
enum class RootSlot : UINT {
    Material = 0,    ///< マテリアル (register b0) - PS
    Transform = 1,   ///< 座標変換行列 (register b0) - VS
    BindlessSRV = 2, ///< フル Bindless用 SRVテーブル (register t0, space1-6) - ALL
    LightCommon = 3, ///< ライト共通データ (register b1) - VS/PS
    // NOTE: Instancing (t0, space0) および Special (b6) は既存シェーダー互換性のためバインド枠を維持
    Instancing = 4,          ///< インスタンシング用 SRV (register t0, space0) - VS
    Camera = 5,              ///< カメラ/PerFrameData (register b2) - VS/PS
    Special = 6,             ///< 特殊用 (GSなど) (register b6) - ALL
    LineInstancing = 7,      ///< ライン用インスタンシング (register t1, space0) - VS
    Lights = 8,              ///< レガシーライトSRVテーブル (register t2, t3, t4) - PS
    ShadowMap = 9,           ///< レガシーシャドウマップ (register t5) - PS
    DepthMap = 10,           ///< レガシーメイン深度マップ (register t6) - PS
    CustomEffectParams = 11, ///< 個別エフェクト用パラメータ配列 (register b3) - PS
};

/**
 * @enum ComputeRootSlot
 * @brief コンピュートシェーダー用ルートパラメータのスロットインデックスを定義します。
 * @details DXRootSignatureManager::CreateRootSignatures() の computeRootParameters に完全対応します。
 */
enum class ComputeRootSlot : UINT {
    SRV_t0 = 0,        ///< register(t0) - SRV DescriptorTable (Num: 1)
    SRV_t1 = 1,        ///< register(t1) - SRV DescriptorTable (Num: 1)
    SRV_t2 = 2,        ///< register(t2) - SRV DescriptorTable (Num: 1)
    UAV_u0 = 3,        ///< register(u0) - UAV DescriptorTable (Num: 1)
    CBV_b0 = 4,        ///< register(b0) - Root CBV
    CBV_b1 = 5,        ///< register(b1) - Root CBV
    UAV_u1 = 6,        ///< register(u1) - UAV DescriptorTable (Num: 1)
    UAV_u2 = 7,        ///< register(u2) - UAV DescriptorTable (Num: 1)
    UAV_u3 = 8,        ///< register(u3) - UAV DescriptorTable (Num: 1)
    Constants_b2 = 9,  ///< register(b2) - 32-bit Constants (Num: 2)
    SRV_t3 = 10,       ///< register(t3) - Root SRV (Buffer)
};

