#include "Renderer/System/Core/LineResource.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Math.h"
#include "Core/System/IrufemiEngine.h"

namespace Irufemi {

LineResource::~LineResource() {
    Unmap();
    if (auto dxCommon = BaseResource::GetDirectXCommon()) {
        if (auto engine = dxCommon->GetEngine()) {
            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Free(materialCbIndex_);
            }
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetTransformBufferManager()->Free(transformCbIndex_);
            }
        }
    }
}

void LineResource::CreateResource() {
    auto* dxCommon = GetDxCommon();
    if (!dxCommon) {
        return;
    }

    // Irufemi::Line は基本 2 頂点
    if (!vertexResource_) {
        vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 2);
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeof(VertexData) * 2;
        vertexBufferView_.StrideInBytes = sizeof(VertexData);
    }

    if (!indexResource_) {
        indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 2);
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = sizeof(uint32_t) * 2;
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        indexCount_ = 2;
    }

    if (auto engine = dxCommon->GetEngine()) {
        materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();

        cpuMaterialData_.color = {1, 1, 1, 1};
        cpuMaterialData_.uvTransform = Irufemi::Math::MakeIdentity4x4();

        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }

        transformCbIndex_ = engine->GetTransformBufferManager()->Allocate();
    }
}

void LineResource::Map() {
    if (vertexResource_) {
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    }
    if (indexResource_) {
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    }
}

void LineResource::Unmap() {
    if (vertexResource_ && vertexData_) {
        vertexResource_->Unmap(0, nullptr);
        vertexData_ = nullptr;
    }
    if (indexResource_ && indexData_) {
        indexResource_->Unmap(0, nullptr);
        indexData_ = nullptr;
    }
}

void LineResource::UpdateTransform(const Camera& camera) {
    transformationMatrix_.world =
        Irufemi::Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    transformationMatrix_.WVP = Irufemi::Math::Multiply(
        transformationMatrix_.world, Irufemi::Math::Multiply(camera.GetViewMatrix(), camera.GetPerspectiveFovMatrix()));

    MarkAsDirty();
}

D3D12_GPU_VIRTUAL_ADDRESS LineResource::GetMaterialVAddress() const {
    if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
        return 0;
    }
    auto* dxCommon = BaseResource::GetDirectXCommon();
    if (!dxCommon || !dxCommon->GetEngine() || !dxCommon->GetEngine()->GetMaterialBufferManager()) {
        return 0;
    }
    return dxCommon->GetEngine()->GetMaterialBufferManager()->GetGPUVirtualAddress(materialCbIndex_,
                                                                                   dxCommon->GetFrameIndex());
}

D3D12_GPU_VIRTUAL_ADDRESS LineResource::GetTransformVAddress() const {
    if (transformCbIndex_ == static_cast<uint32_t>(-1)) {
        return 0;
    }
    auto* dxCommon = BaseResource::GetDirectXCommon();
    if (!dxCommon || !dxCommon->GetEngine() || !dxCommon->GetEngine()->GetTransformBufferManager()) {
        return 0;
    }
    return dxCommon->GetEngine()->GetTransformBufferManager()->GetGPUVirtualAddress(transformCbIndex_,
                                                                                    dxCommon->GetFrameIndex());
}

void LineResource::SyncBeforeDraw() {
    auto* dxCommon = BaseResource::GetDirectXCommon();
    if (!dxCommon) {
        return;
    }
    uint32_t frameIndex = dxCommon->GetFrameIndex();
    if (CheckAndClearDirty(frameIndex)) {
        if (auto engine = dxCommon->GetEngine()) {
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetTransformBufferManager()->Update(transformCbIndex_, transformationMatrix_, frameIndex);
            }
            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, frameIndex);
            }
        }
    }
}

} // namespace Irufemi
