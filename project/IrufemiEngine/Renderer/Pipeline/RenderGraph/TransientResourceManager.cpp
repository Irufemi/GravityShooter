#include "Renderer/Pipeline/RenderGraph/TransientResourceManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/Utility/DXUtility.h"
#include <stdexcept>

#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#endif

void TransientResourceManager::Initialize(DirectXCommon* dxCommon, uint64_t heapSizeInBytes) {
    dxCommon_ = dxCommon;
    heapSize_ = heapSizeInBytes;

    D3D12_HEAP_DESC heapDesc{};
    heapDesc.SizeInBytes = heapSize_;
    // テクスチャ（RT/DSV）やバッファの両方を配置可能にするアライメント設定
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.CreationNodeMask = 1;
    heapDesc.Properties.VisibleNodeMask = 1;
    heapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

    HRESULT hr = dxCommon_->GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap_));
    HR_CHECK(hr, "Failed to create Transient Resource Heap.");
}

void TransientResourceManager::Finalize() {
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }
    resourcePool_.clear();
    heap_.Reset();
}

void TransientResourceManager::ClearCache() {
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }
    resourcePool_.clear();
}

void TransientResourceManager::ResetForFrame() {
    // キャッシュしたリソースを使用可能状態にリセットする
    for (auto& res : resourcePool_) {
        res.inUse = false;
    }
}

ID3D12Resource* TransientResourceManager::AcquirePlacedResource(const D3D12_RESOURCE_DESC& desc, uint64_t offset,
                                                                D3D12_RESOURCE_STATES initialState,
                                                                const D3D12_CLEAR_VALUE* clearValue,
                                                                D3D12_RESOURCE_STATES* outCurrentState) {
    if (!heap_) {
        return nullptr;
    }

    // 1. キャッシュから検索
    for (auto& res : resourcePool_) {
        bool clearValueMatch = false;
        if (!clearValue && !res.hasClearValue) {
            clearValueMatch = true;
        } else if (clearValue && res.hasClearValue && res.clearValue.Format == clearValue->Format) {
            clearValueMatch = true; // 色の完全一致までは今回は省略可能
        }

        if (!res.inUse && res.offset == offset && res.desc.Width == desc.Width && res.desc.Height == desc.Height &&
            res.desc.Format == desc.Format && res.desc.Flags == desc.Flags &&
            res.desc.DepthOrArraySize == desc.DepthOrArraySize && clearValueMatch) {

            res.inUse = true;
            if (outCurrentState) {
                *outCurrentState = res.currentState;
            }
            return res.resource.Get();
        }
    }

    // 2. 見つからなければ新規作成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = dxCommon_->GetDevice()->CreatePlacedResource(heap_.Get(), offset, &desc, initialState, clearValue,
                                                              IID_PPV_ARGS(&resource));

    if (SUCCEEDED(hr)) {
        D3D12_RESOURCE_ALLOCATION_INFO allocInfo = dxCommon_->GetDevice()->GetResourceAllocationInfo(0, 1, &desc);

        CachedResource cache;
        cache.desc = desc;
        cache.offset = offset;
        cache.resource = resource;
        cache.inUse = true;
        cache.hasClearValue = (clearValue != nullptr);
        if (clearValue) {
            cache.clearValue = *clearValue;
        }
        cache.allocationSize = allocInfo.SizeInBytes;
        cache.currentState = initialState;
        if (outCurrentState) {
            *outCurrentState = initialState;
        }
        resourcePool_.push_back(cache);
        return resource.Get();
    }

    // エラー時はログを出力
    HR_CHECK(hr, "TransientResourceManager: CreatePlacedResource failed!");

    return nullptr;
}

void TransientResourceManager::UpdateResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (!resource) {
        return;
    }
    for (auto& res : resourcePool_) {
        if (res.resource.Get() == resource) {
            res.currentState = state;
            return;
        }
    }
}

#ifdef USE_IMGUI
void TransientResourceManager::DebugUI() {
    if (ImGui::CollapsingHeader("Transient Resource Aliasing Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Heap Size: %llu MB", heapSize_ / (1024 * 1024));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = 30.0f;

        // Background for the heap
        drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(50, 50, 50, 255));
        drawList->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(200, 200, 200, 255));

        // Draw each placed resource
        for (const auto& res : resourcePool_) {
            if (!res.inUse) {
                continue;
            }

            float startRatio = static_cast<float>(res.offset) / static_cast<float>(heapSize_);
            float sizeRatio = static_cast<float>(res.allocationSize) / static_cast<float>(heapSize_);

            float x0 = p.x + startRatio * width;
            float x1 = x0 + sizeRatio * width;

            // Calculate a color based on format and size
            ImU32 color = IM_COL32((res.offset * 123) % 200 + 55, (res.allocationSize * 321) % 200 + 55, 150, 200);

            drawList->AddRectFilled(ImVec2(x0, p.y + 2), ImVec2(x1, p.y + height - 2), color);
            drawList->AddRect(ImVec2(x0, p.y + 2), ImVec2(x1, p.y + height - 2), IM_COL32(255, 255, 255, 255));

            if (ImGui::IsMouseHoveringRect(ImVec2(x0, p.y), ImVec2(x1, p.y + height))) {
                ImGui::BeginTooltip();
                ImGui::Text("Format: %d", res.desc.Format);
                ImGui::Text("Size: %llu x %llu", res.desc.Width, res.desc.Height);
                ImGui::Text("Offset: %llu", res.offset);
                ImGui::Text("Allocated Size: %llu bytes", res.allocationSize);
                ImGui::EndTooltip();
            }
        }

        ImGui::Dummy(ImVec2(width, height + 10.0f));
    }
}
#endif
