#pragma once

#include <vector>
#include "Core/Math/Matrix4x4.h"
#include <wrl.h>
#include <d3d12.h>
#include <span>
#include <array>
#include "Resource/Model/Data/VertexInfluence.h"
#include "Resource/Model/Data/WellForGPU.h"
#include "RHI/DirectX12/DirectXCommon.h"

namespace Irufemi {

/**
 * @struct SkinningInformation
 * @brief コンピュートシェーダーに渡すスキニング情報定数バッファ
 */
struct SkinningInformation {
    uint32_t numVertices = 0;
    uint32_t pad[3] = {};
};
static_assert(sizeof(SkinningInformation) == 16,
              "SkinningInformation size must be 16 bytes for GPU ConstantBuffer alignment.");

/**
 * @class SkinCluster
 * @brief 頂点ごとのボーンウェイトと行列を管理するスキンクラスターデータ
 * @details GPUスキニングのために必要な行列パレットやウェイト情報を保持し、頂点を適切に変形させます。
 */
struct SkinCluster {
    /** @brief 各ボーンの初期姿勢の逆行列（Inverse Bind Pose） */
    std::vector<Irufemi::Matrix4x4> inverseBindPoseMatrices;

    /** @brief 頂点ごとの影響ボーンインデックスとウェイトを保持するGPUリソース */
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;

    /** @brief 上記リソースの頂点バッファビュー */
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView;

    /** @brief GPUと同期するためのマップされた頂点影響データのスパン */
    std::span<VertexInfluence> mappedInfluence;

    /** @brief スキニング行列パレット用のバッファリソース（フレームバッファごとの配列） */
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> paletteResource;

    /** @brief マップされた行列パレットデータのスパン */
    std::array<std::span<WellForGPU>, kMaxFramesInFlight> mappedPalette;

    /** @brief 行列パレットのシェーダーリソースビュー（SRV）のCPU/GPUディスクリプタハンドル */
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight>
        paletteSrvHandle;

    Microsoft::WRL::ComPtr<ID3D12Resource> inputVertexResource;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> inputVertexSrvHandle;

    // コンピュートシェーダー用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource;
    SkinningInformation* mappedSkinningInformation = nullptr;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> skinnedVertexResource;
    std::array<D3D12_VERTEX_BUFFER_VIEW, kMaxFramesInFlight> skinnedVertexBufferView;
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight>
        skinnedVertexSrvHandle;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> influenceSrvHandle;
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight>
        skinnedVertexUavHandle;
};

} // namespace Irufemi