#define NOMINMAX
#include "RHI/DirectX12/DescriptorAllocator.h"
#include "Core/Utility/ErrorUtility.h"

DescriptorAllocator::DescriptorAllocator(ID3D12DescriptorHeap* heap, uint32_t descriptorSize, uint32_t baseIndex)
    : descriptorSize_(descriptorSize), baseIndex_(baseIndex) {
    heap_ = heap;
    auto desc = heap_->GetDesc();
    capacity_ = desc.NumDescriptors;
    inUse_.assign(capacity_, false);
    for (uint32_t i = 0; i < baseIndex_; ++i) {
        inUse_[i] = true;
    }
    nextIndex_ = baseIndex_;
}

uint32_t DescriptorAllocator::Allocate() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!freeList_.empty()) {
        uint32_t idx = freeList_.back();
        freeList_.pop_back();
        inUse_[idx] = true;
        return idx;
    }
    if (nextIndex_ < capacity_) {
        uint32_t idx = nextIndex_++;
        inUse_[idx] = true;
        return idx;
    }
    IRUFEMI_ASSERT_MSG(false, "Descriptor heap exhausted in Allocate()!");
    return kInvalid;
}

void DescriptorAllocator::Free(uint32_t index) {
    if (index == kInvalid) {
        return;
    }
    std::lock_guard<std::mutex> lk(mutex_);
    IRUFEMI_ASSERT_MSG(index >= baseIndex_ && index < capacity_, "Descriptor index out of range in Free!");
    IRUFEMI_ASSERT_MSG(inUse_[index], "Descriptor double free detected in Free!");
    inUse_[index] = false;
    freeList_.push_back(index);
}

void DescriptorAllocator::FreeAfterFence(uint32_t index, uint64_t safeFence) {
    if (index == kInvalid) {
        return;
    }
    std::lock_guard<std::mutex> lk(mutex_);
    IRUFEMI_ASSERT_MSG(index >= baseIndex_ && index < capacity_, "Descriptor index out of range in FreeAfterFence!");
    IRUFEMI_ASSERT_MSG(inUse_[index], "Descriptor double free detected in FreeAfterFence!");
    inUse_[index] = false;
    pending_.push(Pending{safeFence, index});
}

void DescriptorAllocator::GarbageCollect(uint64_t completedFence) {
    std::lock_guard<std::mutex> lk(mutex_);
    while (!pending_.empty() && pending_.top().fence <= completedFence) {
        freeList_.push_back(pending_.top().index);
        pending_.pop();
    }
}

void DescriptorAllocator::RebuildFreeListExcept(const std::vector<uint32_t>& used) {
    std::lock_guard<std::mutex> lk(mutex_);
    freeList_.clear();
    inUse_.assign(capacity_, false);
    for (uint32_t i = 0; i < baseIndex_; ++i) {
        inUse_[i] = true;
    }
    for (uint32_t uIdx : used) {
        if (uIdx < capacity_) {
            inUse_[uIdx] = true;
        }
    }

    size_t u = 0, uCount = used.size();
    for (uint32_t idx = baseIndex_; idx < capacity_; ++idx) {
        while (u < uCount && used[u] < idx) {
            ++u;
        }
        if (u < uCount && used[u] == idx) {
            continue;
        }
        freeList_.push_back(idx);
    }
    if (uCount > 0) {
        nextIndex_ = std::max(baseIndex_, used.back() + 1);
    } else {
        nextIndex_ = baseIndex_;
    }
    if (nextIndex_ > capacity_) {
        nextIndex_ = capacity_;
    }
}

void DescriptorAllocator::ReservePrefix(uint32_t count) {
    std::lock_guard<std::mutex> lk(mutex_);
    baseIndex_ = (std::min)(capacity_, count);
    for (uint32_t i = 0; i < baseIndex_; ++i) {
        inUse_[i] = true;
    }
    if (nextIndex_ < baseIndex_) {
        nextIndex_ = baseIndex_;
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(uint32_t index) const {
    IRUFEMI_ASSERT_MSG(index != kInvalid && index < capacity_, "Invalid descriptor index passed to GetCPUHandle!");
    D3D12_CPU_DESCRIPTOR_HANDLE h = heap_->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(uint32_t index) const {
    IRUFEMI_ASSERT_MSG(index != kInvalid && index < capacity_, "Invalid descriptor index passed to GetGPUHandle!");
    D3D12_GPU_DESCRIPTOR_HANDLE h = heap_->GetGPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<UINT64>(descriptorSize_) * index;
    return h;
}