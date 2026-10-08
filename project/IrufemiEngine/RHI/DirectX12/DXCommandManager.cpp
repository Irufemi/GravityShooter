#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/DXCommandManager.h"
#include "RHI/DirectX12/DirectXCommon.h" // kMaxFramesInFlight を使用するため
#include <cassert>

void DXCommandManager::Initialize(ID3D12Device* device) {
    IRUFEMI_ASSERT(device != nullptr);
    device_ = device;

    // --- コマンドキューの生成 ---
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    // --- メインのコマンドアロケータ生成 ---
    commandAllocators_.resize(kMaxFramesInFlight);
    fenceValues_.resize(kMaxFramesInFlight, 0);

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(commandAllocators_[i].GetAddressOf()));
        ASSERT_IF_FAILED(hr);
    }

    // --- メインのコマンドリスト生成 ---
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0].Get(), nullptr,
                                    IID_PPV_ARGS(commandList_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    commandList_->Close();

    // --- 同期用フェンスとイベントの生成 ---
    hr = device_->CreateFence(fenceValues_[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    IRUFEMI_ASSERT(fenceEvent_ != nullptr);

    // --- 転送専用(Upload)コマンド系の生成 ---
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(uploadCommandAllocator_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadCommandAllocator_.Get(), nullptr,
                                    IID_PPV_ARGS(uploadCommandList_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    uploadCommandList_->Close();

    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(uploadFence_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
}

void DXCommandManager::Finalize() {
    // 全フレームの完了を待機
    if (commandQueue_ && fence_) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            WaitForGPU();
        }
    }

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    uploadFence_.Reset();
    uploadCommandList_.Reset();
    uploadCommandAllocator_.Reset();

    fence_.Reset();
    commandList_.Reset();
    for (auto& allocator : commandAllocators_) {
        allocator.Reset();
    }
    commandAllocators_.clear();
    fenceValues_.clear();

    commandQueue_.Reset();
    device_.Reset();
}

void DXCommandManager::WaitForGPU() {
    if (commandQueue_ && fence_ && fenceEvent_) {
        uint64_t fv = IncrementGlobalFence();
        commandQueue_->Signal(fence_.Get(), fv);
        if (fence_->GetCompletedValue() < fv) {
            fence_->SetEventOnCompletion(fv, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }
}

void DXCommandManager::ExecuteUploadCommands(std::function<void(ID3D12GraphicsCommandList*)> commands) {
    std::lock_guard<std::mutex> lock(uploadMutex_);
    uploadCommandAllocator_->Reset();
    uploadCommandList_->Reset(uploadCommandAllocator_.Get(), nullptr);

    commands(uploadCommandList_.Get());

    uploadCommandList_->Close();
    ID3D12CommandList* ppCommandLists[] = {uploadCommandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, ppCommandLists);

    uploadFenceValue_++;
    commandQueue_->Signal(uploadFence_.Get(), uploadFenceValue_);

    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (uploadFence_->GetCompletedValue() < uploadFenceValue_) {
        uploadFence_->SetEventOnCompletion(uploadFenceValue_, event);
        WaitForSingleObject(event, INFINITE);
    }
    CloseHandle(event);
}

ID3D12CommandAllocator* DXCommandManager::GetCommandAllocator(uint32_t frameIndex) const {
    if (frameIndex < commandAllocators_.size()) {
        return commandAllocators_[frameIndex].Get();
    }
    return nullptr;
}

uint64_t DXCommandManager::GetFenceValue(uint32_t frameIndex) const {
    if (frameIndex < fenceValues_.size()) {
        return fenceValues_[frameIndex];
    }
    return 0;
}

void DXCommandManager::SetFenceValue(uint32_t frameIndex, uint64_t value) {
    if (frameIndex < fenceValues_.size()) {
        fenceValues_[frameIndex] = value;
    }
}

void DXCommandManager::AdvanceFenceValue(uint32_t frameIndex, uint64_t delta) {
    if (frameIndex < fenceValues_.size()) {
        fenceValues_[frameIndex] += delta;
    }
}
