#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <vector>
#include <mutex>

/**
 * @class DXSwapChainManager
 * @brief スワップチェーンおよび付随するレンダーターゲット・深度バッファを管理するクラス
 */
class DXSwapChainManager {
public:
    DXSwapChainManager() = default;
    ~DXSwapChainManager() {
        Finalize();
    }

    /**
     * @brief 初期化処理。スワップチェーン、RTV/DSVデスクリプタヒープ、レンダーターゲットを生成
     * @param device D3D12デバイス
     * @param dxgiFactory DXGIファクトリー
     * @param commandQueue コマンドキュー (スワップチェーン生成に必要)
     * @param hwnd ウィンドウハンドル
     * @param width ウィンドウ幅
     * @param height ウィンドウ高さ
     */
    void Initialize(ID3D12Device* device, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, HWND hwnd,
                    int32_t width, int32_t height);

    /**
     * @brief 解放処理。スワップチェーンおよびRTV/DSVリソースを破棄
     */
    void Finalize();

    /**
     * @brief スワップチェーンのリサイズ処理
     * @param device D3D12デバイス
     * @param width 新しいウィンドウ幅
     * @param height 新しいウィンドウ高さ
     */
    void ResizeSwapChain(ID3D12Device* device, int32_t width, int32_t height);

    /**
     * @brief 表示するバックバッファのインデックスを取得
     */
    UINT GetCurrentBackBufferIndex() const {
        return swapChain_->GetCurrentBackBufferIndex();
    }

    /** @name デスクリプタヒープの割り当て・解放 */
    ///@{
    /** @brief 新しいRTVデスクリプタインデックスを割り当てる */
    uint32_t AllocateRTVIndex();
    /**
     * @brief 指定フレームのGPU完了後にRTVデスクリプタを解放・返却する
     * @param index 解放するRTVインデックス
     * @param currentFenceValue 現在のフレームフェンス値
     */
    void FreeRTVIndex(uint32_t index, uint64_t currentFenceValue);

    /** @brief 新しいDSVデスクリプタインデックスを割り当てる */
    uint32_t AllocateDSVIndex();
    /**
     * @brief 指定フレームのGPU完了後にDSVデスクリプタを解放・返却する
     * @param index 解放するDSVインデックス
     * @param currentFenceValue 現在のフレームフェンス値
     */
    void FreeDSVIndex(uint32_t index, uint64_t currentFenceValue);

    /**
     * @brief GPU処理完了に合わせた保留中のデスクリプタ解放
     * @param completedFenceValue 完了したフェンス値
     */
    void FlushPendingDescriptors(uint64_t completedFenceValue);
    ///@}

    /** @name ゲッター */
    ///@{
    IDXGISwapChain4* GetSwapChain() const {
        return swapChain_.Get();
    }
    /**
     * @brief SwapChainResource を取得する。
     * @return 取得された SwapChainResource
     */
    ID3D12Resource* GetSwapChainResource(UINT index) const {
        return swapChainResources_[index].Get();
    }
    /**
     * @brief DepthStencilResource を取得する。
     * @return 取得された DepthStencilResource
     */
    ID3D12Resource* GetDepthStencilResource() const {
        return depthStencilResource_.Get();
    }

    /**
     * @brief RTVDescriptorHeap を取得する。
     * @return 取得された RTVDescriptorHeap
     */
    ID3D12DescriptorHeap* GetRTVDescriptorHeap() const {
        return rtvDescriptorHeap_.Get();
    }
    /**
     * @brief DSVDescriptorHeap を取得する。
     * @return 取得された DSVDescriptorHeap
     */
    ID3D12DescriptorHeap* GetDSVDescriptorHeap() const {
        return dsvDescriptorHeap_.Get();
    }

    /**
     * @brief RTVCPUDescriptorHandle を取得する。
     * @return 取得された RTVCPUDescriptorHandle
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUDescriptorHandle(uint32_t index) const;
    /**
     * @brief RTVGPUDescriptorHandle を取得する。
     * @return 取得された RTVGPUDescriptorHandle
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetRTVGPUDescriptorHandle(uint32_t index) const;
    /**
     * @brief DSVCPUDescriptorHandle を取得する。
     * @return 取得された DSVCPUDescriptorHandle
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCPUDescriptorHandle(uint32_t index) const;
    /**
     * @brief DSVGPUDescriptorHandle を取得する。
     * @return 取得された DSVGPUDescriptorHandle
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetDSVGPUDescriptorHandle(uint32_t index) const;

    /**
     * @brief RtvDesc を取得する。
     * @return 取得された RtvDesc
     */
    const D3D12_RENDER_TARGET_VIEW_DESC& GetRtvDesc() const {
        return rtvDesc_;
    }
    /**
     * @brief SwapChainDesc を取得する。
     * @return 取得された SwapChainDesc
     */
    const DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const {
        return swapChainDesc_;
    }
    /**
     * @brief RtvHandles を取得する。
     * @return 取得された RtvHandles
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandles(UINT index) const {
        return rtvHandles_[index];
    }

    /**
     * @brief ティアリング（VSyncオフ時の低遅延描画）がサポートされているか取得する
     */
    bool IsTearingSupported() const {
        return isTearingSupported_;
    }
    ///@}

private:
    /**
     * @brief 指定したサイズ・フォーマットの深度ステンシルリソースを生成する
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width,
                                                                             int32_t height);

    /**
     * @brief 各種オブジェクトの生成処理
     */
    void CreateSwapChain(IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, HWND hwnd, int32_t width,
                         int32_t height);
    /**
     * @brief CreateDescriptorHeaps を実行する。
     */
    void CreateDescriptorHeaps(ID3D12Device* device);
    /**
     * @brief InitializeRenderTargets を実行する。
     */
    void InitializeRenderTargets(ID3D12Device* device);
    /**
     * @brief CreateDepthStencil を実行する。
     */
    void CreateDepthStencil(ID3D12Device* device, int32_t width, int32_t height);
    /**
     * @brief ReleaseSwapChainResources を実行する。
     */
    void ReleaseSwapChainResources();

private:
    // --- スワップチェーン ---
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2];
    bool isTearingSupported_ = false;

    // --- 深度ステンシル ---
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

    // --- デスクリプタヒープ ---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[4];
    uint32_t descriptorSizeRTV_ = 0;
    uint32_t descriptorSizeDSV_ = 0;

    // --- デスクリプタ再利用用 ---
    struct PendingDescriptor {
        uint64_t fenceValue;
        uint32_t index;
    };
    std::vector<uint32_t> freeRtvIndices_;
    std::vector<uint32_t> freeDsvIndices_;
    std::vector<PendingDescriptor> pendingFreeRtvs_;
    std::vector<PendingDescriptor> pendingFreeDsvs_;

    uint32_t nextRtvIndex_ = 4;
    uint32_t nextDsvIndex_ = 1;

    std::mutex descriptorMutex_;
};
