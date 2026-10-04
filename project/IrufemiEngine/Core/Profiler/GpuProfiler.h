#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

class DirectXCommon;

/**
 * @class GpuProfiler
 * @brief DirectX12のタイムスタンプクエリを用いて、GPUの描画にかかった時間を計測するクラス
 */
class GpuProfiler {
public:
    GpuProfiler() = default;
    ~GpuProfiler();

    // コピー禁止
    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    /**
     * @brief 初期化処理。クエリヒープと結果格納用バッファを作成する
     * @param dxCommon DirectX共通基盤
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief 終了処理。クエリヒープおよびリードバックバッファを明示的に解放する
     */
    void Finalize();

    /**
     * @brief コマンドリストの先頭（描画開始時）で呼び出し、開始タイムスタンプを記録する
     */
    void StartFrame(ID3D12GraphicsCommandList* commandList);

    /**
     * @brief コマンドリストの末尾（描画終了時）で呼び出し、終了タイムスタンプを記録し、
     *        バッファへの解決（Resolve）を行う
     */
    void EndFrame(ID3D12GraphicsCommandList* commandList);

    /**
     * @brief 直前に取得完了したGPUフレーム時間（ミリ秒）を返す
     */
    float GetLastFrameGpuTimeMs() const {
        return lastGpuTimeMs_;
    }

private:
private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> queryResultBuffer_;
    uint64_t* mappedData_ = nullptr; ///< 永続マップ (Persistent Mapping) されたCPUポインタ

    DirectXCommon* dxCommon_ = nullptr;

    uint64_t gpuFrequency_ = 0;
    float lastGpuTimeMs_ = 0.0f;
    bool isInitialized_ = false;

    static const uint32_t kMaxFramesInFlight = 3;
};
