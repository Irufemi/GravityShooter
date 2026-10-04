#include "Core/Profiler/GpuProfiler.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/Utility/ErrorUtility.h"

#pragma comment(lib, "d3d12.lib")

void GpuProfiler::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;

    auto device = dxCommon_->GetDevice();
    auto commandQueue = dxCommon_->GetCommandQueue();

    // GPUのタイマー周波数を取得
    commandQueue->GetTimestampFrequency(&gpuFrequency_);

    // クエリヒープの作成
    // 3フレーム(kMaxFramesInFlight) x 2クエリ(開始・終了) = 6
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = kMaxFramesInFlight * 2;
    queryHeapDesc.NodeMask = 0;

    HRESULT hr = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
    IRUFEMI_ASSERT(SUCCEEDED(hr));

    // クエリ結果格納用のリードバックバッファを作成
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(uint64_t) * kMaxFramesInFlight * 2;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                         nullptr, IID_PPV_ARGS(&queryResultBuffer_));
    IRUFEMI_ASSERT(SUCCEEDED(hr));

    // リードバックバッファを永続マップ (Persistent Mapping) してCPUポインタをキャッシュ
    D3D12_RANGE readRange = {0, sizeof(uint64_t) * kMaxFramesInFlight * 2};
    hr = queryResultBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&mappedData_));
    IRUFEMI_ASSERT(SUCCEEDED(hr));

    isInitialized_ = true;
}

GpuProfiler::~GpuProfiler() {
    Finalize();
}

void GpuProfiler::Finalize() {
    if (queryResultBuffer_ && mappedData_) {
        D3D12_RANGE writeRange = {0, 0};
        queryResultBuffer_->Unmap(0, &writeRange);
        mappedData_ = nullptr;
    }
    queryResultBuffer_.Reset();
    queryHeap_.Reset();
    dxCommon_ = nullptr;
    isInitialized_ = false;
}

void GpuProfiler::StartFrame(ID3D12GraphicsCommandList* commandList) {
    if (!isInitialized_) {
        return;
    }

    uint32_t frameIndex = dxCommon_->GetFrameIndex();
    uint32_t startIndex = frameIndex * 2;

    // 現在のフレームの開始タイムスタンプを記録
    commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startIndex);

    // ストールを防ぐため、安全にGPU処理が終わっている（現在のフェンスで待機完了した）フレームの結果をリードバックする
    uint32_t safeFrameIndex = frameIndex;

    // 永続マップ済みポインタから直接読み取る（毎フレームのMap/Unmapオーバーヘッドを完全排除）
    if (mappedData_) {
        uint64_t startTimestamp = mappedData_[safeFrameIndex * 2];
        uint64_t endTimestamp = mappedData_[safeFrameIndex * 2 + 1];

        if (endTimestamp > startTimestamp && startTimestamp > 0) {
            uint64_t delta = endTimestamp - startTimestamp;
            lastGpuTimeMs_ = (static_cast<float>(delta) / static_cast<float>(gpuFrequency_)) * 1000.0f;
        }
    }
}

void GpuProfiler::EndFrame(ID3D12GraphicsCommandList* commandList) {
    if (!isInitialized_) {
        return;
    }

    uint32_t frameIndex = dxCommon_->GetFrameIndex();
    uint32_t startIndex = frameIndex * 2;
    uint32_t endIndex = startIndex + 1;

    // 終了タイムスタンプを記録
    commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endIndex);

    // クエリヒープのデータをリードバックバッファに解決(Resolve)する
    commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startIndex, 2, queryResultBuffer_.Get(),
                                  startIndex * sizeof(uint64_t));
}
