#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include <cassert>

namespace Irufemi {

void DescriptorPool::Initialize(ID3D12Device* device) {
    IRUFEMI_ASSERT(device);
    device_ = device;

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = kMaxSRVCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
    ASSERT_IF_FAILED(hr);

    descriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    baseIndex_ = 0;
    nextIndex_ = 0;
}

uint32_t DescriptorPool::Allocate(uint32_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 単発確保かつフリーリストがある場合はそこから返す
    if (count == 1 && !freeList_.empty()) {
        uint32_t index = freeList_.back();
        freeList_.pop_back();
        return index;
    }

    // 複数確保、またはフリーリストが空の場合は nextIndex_ から末尾を切り出す
    // (nextIndex_ から確保することで確実に連続性が保証される)
    if (nextIndex_ + count > kMaxSRVCount) {
        IRUFEMI_ASSERT(false && "DescriptorPool is full.");
        return kInvalid;
    }

    uint32_t index = nextIndex_;
    nextIndex_ += count;
    return index;
}

void DescriptorPool::Free(uint32_t index) {
    if (index == kInvalid) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    freeList_.push_back(index);
}

void DescriptorPool::FreeAfterFence(uint32_t index, uint64_t safeFence) {
    if (index == kInvalid) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push({safeFence, index});
}

void DescriptorPool::GarbageCollect(uint64_t completedFence) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pending_.empty() && pending_.top().fence <= completedFence) {
        freeList_.push_back(pending_.top().index);
        pending_.pop();
    }
}

void DescriptorPool::RebuildFreeListExcept(const std::vector<uint32_t>& usedSortedUnique) {
    std::lock_guard<std::mutex> lock(mutex_);
    freeList_.clear();
    pending_ = {};

    uint32_t usedIdx = 0;
    for (uint32_t i = baseIndex_; i < nextIndex_; ++i) {
        bool isUsed = false;
        while (usedIdx < usedSortedUnique.size() && usedSortedUnique[usedIdx] <= i) {
            if (usedSortedUnique[usedIdx] == i) {
                isUsed = true;
            }
            usedIdx++;
        }
        if (!isUsed) {
            freeList_.push_back(i);
        }
    }
}

void DescriptorPool::ReservePrefix(uint32_t count) {
    IRUFEMI_ASSERT(nextIndex_ == 0 && freeList_.empty()); // 最初に呼ぶこと
    baseIndex_ = count;
    nextIndex_ = count;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorPool::GetCPUHandle(uint32_t index) const {
    IRUFEMI_ASSERT(index < kMaxSRVCount);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * descriptorSize_;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorPool::GetGPUHandle(uint32_t index) const {
    IRUFEMI_ASSERT(index < kMaxSRVCount);
    D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * descriptorSize_;
    return handle;
}

uint32_t DescriptorPool::GetIndexFromGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) const {
    if (gpuHandle.ptr == 0) {
        return kInvalid;
    }
    uint64_t basePtr = heap_->GetGPUDescriptorHandleForHeapStart().ptr;
    if (gpuHandle.ptr < basePtr) {
        return kInvalid;
    }
    uint32_t offset = static_cast<uint32_t>(gpuHandle.ptr - basePtr);
    return offset / descriptorSize_;
}

void DescriptorPool::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format,
                                           UINT mipLevels) {
    IRUFEMI_ASSERT(pResource);
    IRUFEMI_ASSERT(srvIndex < kMaxSRVCount);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    device_->CreateShaderResourceView(pResource, &srvDesc, GetCPUHandle(srvIndex));
}

void DescriptorPool::CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
                                                  UINT structureByteStride) {
    IRUFEMI_ASSERT(pResource);
    IRUFEMI_ASSERT(srvIndex < kMaxSRVCount);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device_->CreateShaderResourceView(pResource, &srvDesc, GetCPUHandle(srvIndex));
}

} // namespace Irufemi