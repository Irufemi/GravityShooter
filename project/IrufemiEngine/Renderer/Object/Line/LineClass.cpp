#include "Renderer/Object/Line/LineClass.h"
#include "Renderer/Camera/CameraManager.h"

#include "Renderer/Camera/Camera.h"
#include "Renderer/DrawManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Core/Math/Math.h"
#include "Core/System/IrufemiEngine.h"

DirectXCommon* Line3DBatch::dx_ = nullptr;
DrawManager* Line3DBatch::drawManager_ = nullptr;
DescriptorPool* Line3DBatch::srvAllocator_ = nullptr;
IrufemiEngine* Line3DBatch::engine_ = nullptr;

Line3DBatch::~Line3DBatch() {
    if (srvAllocator_ && dx_) {
        for (uint32_t& idx : instancingSrvIndex_) {
            if (idx != UINT32_MAX) {
                srvAllocator_->FreeAfterFence(idx, dx_->GetCurrentFrameFenceValue());
                idx = UINT32_MAX;
            }
        }
    }
}

void Line3DBatch::Initialize() {
    instances_.resize(maxInstances_);

    baseLineResource_ = std::make_unique<LineResource>();
    baseLineResource_->CreateResource();
    baseLineResource_->Map();

    // 基準となる線の頂点データ (0,0,0) -> (1,0,0)
    // VertexData: position, texcoord, normal, color
    baseLineResource_->GetVertexData()[0] = {
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    baseLineResource_->GetVertexData()[1] = {
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

    baseLineResource_->GetIndexData()[0] = 0;
    baseLineResource_->GetIndexData()[1] = 1;
}

void Line3DBatch::Update() {
    activeCount_ = 0;
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].active) {
            if (activeCount_ < i) {
                instances_[activeCount_] = instances_[i];
            }
            activeCount_++;
        }
    }
}

void Line3DBatch::AddInstance(const Irufemi::Vector3& start, const Irufemi::Vector3& end, const Irufemi::Vector4& color,
                              float life) {
    if (activeCount_ < maxInstances_) {
        auto& instance = instances_[activeCount_];
        instance.start = start;
        instance.end = end;
        instance.color = color;
        instance.life = life;
        instance.age = 0.0f;
        instance.active = true;
        activeCount_++;
    }
}

void Line3DBatch::ClearInstances() {
    for (size_t i = 0; i < activeCount_; ++i) {
        instances_[i].active = false;
    }
    activeCount_ = 0;
}

void Line3DBatch::BuildInstanceBuffer(bool force) {
    if (activeCount_ == 0 && !force) {
        return;
    }

    CreateOrResizeInstanceBuffer(static_cast<uint32_t>(activeCount_));
    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;
    if (!instanceBuffer_[frameIndex] || !instanceData_[frameIndex] || !engine_) {
        return;
    }

    for (size_t i = 0; i < activeCount_; ++i) {
        const auto& inst = instances_[i];
        instanceData_[frameIndex][i].start = {inst.start.x, inst.start.y, inst.start.z, 1.0f};
        instanceData_[frameIndex][i].end = {inst.end.x, inst.end.y, inst.end.z, 1.0f};
        instanceData_[frameIndex][i].color = inst.color;
    }
}

void Line3DBatch::SyncBeforeDraw() {
    if (isDirty_) {
        BuildInstanceBuffer();
    }
}

void Line3DBatch::Draw() {
    if (activeCount_ == 0) {
        return;
    }
    BuildInstanceBuffer();
    baseLineResource_->SyncBeforeDraw();
    drawManager_->SubmitLineInstanced(baseLineResource_.get(), GetInstancingSrvHandleGPU(), GetInstanceCountU32(),
                                      depthWrite_);
}

void Line3DBatch::CreateOrResizeInstanceBuffer(uint32_t instanceCount) {
    if (instanceCount == 0) {
        return;
    }
    uint32_t frameIndex = dx_->GetFrameIndex();

    if (instanceCount > instanceCapacity_[frameIndex]) {
        if (instanceBuffer_[frameIndex]) {
            instanceBuffer_[frameIndex]->Unmap(0, nullptr);
            instanceData_[frameIndex] = nullptr;
            instanceBuffer_[frameIndex].Reset();
        }
        instanceCapacity_[frameIndex] = instanceCount;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = sizeof(InstanceData) * instanceCapacity_[frameIndex];
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dx_->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(instanceBuffer_[frameIndex].GetAddressOf()));
        if (FAILED(hr)) {
            instanceBuffer_[frameIndex].Reset();
            instanceCapacity_[frameIndex] = 0;
            return;
        }

        instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_[frameIndex]));
        EnsureInstancingSRV();
    }
}

void Line3DBatch::EnsureInstancingSRV() {
    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;
    if (!instanceBuffer_[frameIndex]) {
        return;
    }

    if (instancingSrvIndex_[frameIndex] == UINT32_MAX) {
        instancingSrvIndex_[frameIndex] = srvAllocator_->Allocate();
        instancingSrvCPU_[frameIndex] = srvAllocator_->GetCPUHandle(instancingSrvIndex_[frameIndex]);
        instancingSrvGPU_[frameIndex] = srvAllocator_->GetGPUHandle(instancingSrvIndex_[frameIndex]);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = instanceCapacity_[frameIndex];
    srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    dx_->GetDevice()->CreateShaderResourceView(instanceBuffer_[frameIndex].Get(), &srvDesc,
                                               instancingSrvCPU_[frameIndex]);
}