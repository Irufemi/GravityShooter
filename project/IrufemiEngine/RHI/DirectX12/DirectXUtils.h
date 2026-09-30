#pragma once
#include <d3d12.h>
#include <array>
#include <initializer_list>
#include <vector>

namespace DirectXUtils {

/**
 * @brief Transition（状態遷移）バリアを構築し、指定したコマンドリストに積むヘルパー関数
 * @param cmdList コマンドリスト
 * @param resource 対象のリソース
 * @param stateBefore 遷移前の状態
 * @param stateAfter 遷移後の状態
 * @param subresource サブリソース指定（デフォルトは全サブリソース）
 */
inline void TransitionBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource,
                              D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter,
                              UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
    if (!resource || !cmdList || stateBefore == stateAfter) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    cmdList->ResourceBarrier(1, &barrier);
}

inline void UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource = nullptr) {
    if (!cmdList) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;
    cmdList->ResourceBarrier(1, &barrier);
}

/**
 * @brief 複数リソースの UAV バリアを一括で構築し、コマンドリストに積むヘルパー関数
 * @param cmdList コマンドリスト
 * @param resources 対象リソースのリスト (例: { resA, resB, resC })
 */
inline void UAVBarriers(ID3D12GraphicsCommandList* cmdList, std::initializer_list<ID3D12Resource*> resources) {
    if (!cmdList || resources.size() == 0) {
        return;
    }

    constexpr size_t kStackBarrierCapacity = 16;
    if (resources.size() <= kStackBarrierCapacity) {
        std::array<D3D12_RESOURCE_BARRIER, kStackBarrierCapacity> barriers;
        UINT count = 0;
        for (auto* res : resources) {
            if (!res) {
                continue;
            }
            D3D12_RESOURCE_BARRIER& b = barriers[count++];
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.UAV.pResource = res;
        }
        if (count > 0) {
            cmdList->ResourceBarrier(count, barriers.data());
        }
    } else {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(resources.size());
        for (auto* res : resources) {
            if (!res) {
                continue;
            }
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.UAV.pResource = res;
            barriers.push_back(b);
        }
        if (!barriers.empty()) {
            cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }
    }
}

} // namespace DirectXUtils
