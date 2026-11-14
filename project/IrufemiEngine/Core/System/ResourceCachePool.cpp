#include "Core/System/ResourceCachePool.h"

using namespace Irufemi;

ResourceCachePool::ResourceCachePool(size_t maxMemoryBytes, size_t initialCapacity)
    : maxMemoryBytes_(maxMemoryBytes), currentMemoryUsage_(0), currentTimeCounter_(0) {
    slots_.resize(initialCapacity);
    // 初期状態のフリーリスト構築（逆順に入れて pop_back で 0 から取り出せるようにする）
    freeIndices_.reserve(initialCapacity);
    for (int i = static_cast<int>(initialCapacity) - 1; i >= 0; --i) {
        freeIndices_.push_back(i);
    }
}

ResourceCachePool::~ResourceCachePool() {}

void ResourceCachePool::SetMemoryBudget(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxMemoryBytes_ = maxBytes;
}

ResourceHandle ResourceCachePool::AllocateSlot(size_t memorySize) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t index = 0;
    if (!freeIndices_.empty()) {
        index = freeIndices_.back();
        freeIndices_.pop_back();
    } else {
        // 空きがなければ拡張
        index = static_cast<uint32_t>(slots_.size());
        slots_.emplace_back();
    }

    // スロットの初期化（世代は維持または+1）
    ResourceSlotMeta& slot = slots_[index];
    slot.generation++; // 新しい世代へ
    slot.refCount = 1; // 確保した段階で1とする（確保した人が持つ）
    slot.lastAccessTime = ++currentTimeCounter_;
    slot.memorySize = memorySize;
    slot.isLoaded = false;

    currentMemoryUsage_ += memorySize;

    return ResourceHandle{index, slot.generation};
}

void ResourceCachePool::ReleaseSlot(ResourceHandle handle) {
    if (!handle.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < slots_.size() && slots_[handle.index].generation == handle.generation) {
        if (slots_[handle.index].refCount > 0) {
            slots_[handle.index].refCount--;
        }
    }
}

void ResourceCachePool::RetainSlot(ResourceHandle handle) {
    if (!handle.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < slots_.size() && slots_[handle.index].generation == handle.generation) {
        slots_[handle.index].refCount++;
        slots_[handle.index].lastAccessTime = ++currentTimeCounter_;
    }
}

void ResourceCachePool::TouchSlot(ResourceHandle handle) {
    if (!handle.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < slots_.size() && slots_[handle.index].generation == handle.generation) {
        slots_[handle.index].lastAccessTime = ++currentTimeCounter_;
    }
}

bool ResourceCachePool::IsValid(ResourceHandle handle) const {
    if (!handle.IsValid()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index >= slots_.size()) {
        return false;
    }
    return slots_[handle.index].generation == handle.generation;
}

void ResourceCachePool::UpdateSlotSize(ResourceHandle handle, size_t newSize) {
    if (!handle.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < slots_.size() && slots_[handle.index].generation == handle.generation) {
        currentMemoryUsage_ -= slots_[handle.index].memorySize;
        slots_[handle.index].memorySize = newSize;
        currentMemoryUsage_ += newSize;
    }
}

void ResourceCachePool::SetLoaded(ResourceHandle handle, bool loaded) {
    if (!handle.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < slots_.size() && slots_[handle.index].generation == handle.generation) {
        slots_[handle.index].isLoaded = loaded;
    }
}

bool ResourceCachePool::IsLoaded(ResourceHandle handle) const {
    if (!handle.IsValid()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index >= slots_.size()) {
        return false;
    }
    return slots_[handle.index].generation == handle.generation && slots_[handle.index].isLoaded;
}

void ResourceCachePool::EnforceMemoryBudget(const ResourcePurgeCallback& purgeCallback) {
    if (maxMemoryBytes_ == 0 || !purgeCallback) {
        return;
    }

    std::vector<uint32_t> indicesToPurge;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 予算を超えている間ループ
        while (currentMemoryUsage_ > maxMemoryBytes_) {
            uint32_t oldestIndex = 0xFFFFFFFF;
            uint64_t oldestTime = UINT64_MAX;

            for (uint32_t i = 0; i < slots_.size(); ++i) {
                // memorySize > 0 は有効なスロットを意味する（解放済みスロットは0になる）
                if (slots_[i].memorySize > 0 && slots_[i].refCount == 0) {
                    if (slots_[i].lastAccessTime < oldestTime) {
                        oldestTime = slots_[i].lastAccessTime;
                        oldestIndex = i;
                    }
                }
            }

            // パージできるものが見つからなければ終了（すべて参照されている）
            if (oldestIndex == 0xFFFFFFFF) {
                break;
            }

            // 古いものをパージ対象にする
            ResourceSlotMeta& targetSlot = slots_[oldestIndex];
            currentMemoryUsage_ -= targetSlot.memorySize;

            targetSlot.memorySize = 0;
            targetSlot.isLoaded = false;
            // 世代は次にAllocateされる時に増えるので、ここでは何もしない

            freeIndices_.push_back(oldestIndex);
            indicesToPurge.push_back(oldestIndex);
        }
    }

    // デッドロックを避けるため、ミューテックスのロックを外してからコールバックを呼ぶ
    for (uint32_t index : indicesToPurge) {
        purgeCallback(index);
    }
}

void ResourceCachePool::ClearAll(const ResourcePurgeCallback& purgeCallback) {
    std::vector<uint32_t> indicesToPurge;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (uint32_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i].memorySize > 0) {
                currentMemoryUsage_ -= slots_[i].memorySize;
                slots_[i].memorySize = 0;
                slots_[i].isLoaded = false;
                slots_[i].refCount = 0;
                freeIndices_.push_back(i);

                indicesToPurge.push_back(i);
            }
        }
    }

    if (purgeCallback) {
        for (uint32_t index : indicesToPurge) {
            purgeCallback(index);
        }
    }
}
