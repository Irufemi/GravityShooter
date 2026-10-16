#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

class DirectXCommon;

/**
 * @class TransientResourceManager
 * @brief エイリアシング（メモリ再利用）を利用した一時的リソースアロケータ
 * @details 巨大なヒープを1つ確保し、そこに重ならないようテクスチャを配置（PlacedResource）することでVRAMを節約します。
 */
class TransientResourceManager {
public:
    TransientResourceManager() = default;
    ~TransientResourceManager() = default;

    /**
     * @brief 初期化（指定サイズのヒープを確保）
     * @param dxCommon DirectX基盤
     * @param heapSizeInBytes ヒープサイズ（デフォルトは64MB）
     */
    void Initialize(DirectXCommon* dxCommon, uint64_t heapSizeInBytes = 1024 * 1024 * 64);

    /**
     * @brief 解放
     */
    void Finalize();

    /**
     * @brief キャッシュされているリソースを全て破棄する（リサイズ時などに呼ぶ）
     */
    void ClearCache();

    /**
     * @brief フレームの先頭で呼び出し、確保済みリソースを使用可能な状態にリセットする
     */
    void ResetForFrame();

    /**
     * @brief ヒープ上の指定オフセットにリソースを作成・配置する
     * @param desc リソース記述子
     * @param offset ヒープ上のオフセット
     * @param initialState 新規作成時の初期ステート
     * @param clearValue クリア値（任意）
     * @param[out] outCurrentState キャッシュ再利用または新規作成時の実際のリソースステートを受け取るポインタ（任意）
     * @return 成功時はリソースのポインタ、失敗時はnullptr
     */
    ID3D12Resource* AcquirePlacedResource(const D3D12_RESOURCE_DESC& desc, uint64_t offset,
                                          D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clearValue,
                                          D3D12_RESOURCE_STATES* outCurrentState = nullptr);

    /**
     * @brief リソースの最新ステートをキャッシュ情報に反映する
     * @param resource 対象リソース
     * @param state 更新後のステート
     */
    void UpdateResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

    /**
     * @brief HeapSize を取得する。
     * @return 取得された HeapSize
     */
    uint64_t GetHeapSize() const {
        return heapSize_;
    }

#ifdef USE_IMGUI
    /**
     * @brief デバッグUIの描画（エイリアシングのメモリ配置を可視化）
     */
    void DebugUI();
#endif

private:
    Microsoft::WRL::ComPtr<ID3D12Heap> heap_;
    DirectXCommon* dxCommon_ = nullptr;
    uint64_t heapSize_ = 0;

    // フレーム内で作成した PlacedResource の保持（キャッシュとして再利用）
    struct CachedResource {
        D3D12_RESOURCE_DESC desc;
        uint64_t offset;
        D3D12_CLEAR_VALUE clearValue;
        bool hasClearValue;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        bool inUse;
        uint64_t allocationSize;
        D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    };
    std::vector<CachedResource> resourcePool_;
};
