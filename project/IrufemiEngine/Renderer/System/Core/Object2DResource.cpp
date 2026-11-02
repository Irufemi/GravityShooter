#include "Renderer/System/Core/Object2DResource.h"
#include "Core/Math/Math.h"
#include "Renderer/Camera/Camera.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/System/IrufemiEngine.h"
#include "Resource/Texture/TextureManager.h"

Object2DResource::~Object2DResource() {
    Unmap();
    if (auto dxCommon = BaseResource::GetDirectXCommon()) {
        if (vertexResource_) {
            dxCommon->ReleaseAfterFence(std::move(vertexResource_));
        }
        if (indexResource_) {
            dxCommon->ReleaseAfterFence(std::move(indexResource_));
        }

        if (auto engine = dxCommon->GetEngine()) {
            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Free(materialCbIndex_);
            }
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetTransformBufferManager()->Free(transformCbIndex_);
            }
        }
    }
    auto* tm = textureManager_;
    if (!tm) {
        if (auto dxCommon = BaseResource::GetDirectXCommon()) {
            if (auto engine = dxCommon->GetEngine()) {
                tm = engine->GetTextureManager();
            }
        }
    }
    if (tm && textureHandle_.IsValid()) {
        tm->ReleaseTexture(textureHandle_);
    }
}

void Object2DResource::CreateResource() {
    auto* dxCommon = GetDxCommon();
    if (!dxCommon) {
        return;
    }

    if (!vertexDataList_.empty()) {
        if (vertexCapacity_ < vertexDataList_.size()) {
            if (vertexResource_) {
                Unmap();
                dxCommon->ReleaseAfterFence(std::move(vertexResource_));
            }
            vertexCapacity_ = static_cast<uint32_t>(vertexDataList_.size() + 32);
            vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertexCapacity_);
            vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
            vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertexCapacity_);
            vertexBufferView_.StrideInBytes = sizeof(VertexData);
        }
    }

    if (!indexDataList_.empty()) {
        if (indexCapacity_ < indexDataList_.size()) {
            if (indexResource_) {
                Unmap();
                dxCommon->ReleaseAfterFence(std::move(indexResource_));
            }
            indexCapacity_ = static_cast<uint32_t>(indexDataList_.size() + 64);
            indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indexCapacity_);
            indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
            indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexCapacity_);
            indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        }
        indexCount_ = static_cast<uint32_t>(indexDataList_.size());
    }

    if (auto engine = dxCommon->GetEngine()) {
        if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
            materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();

            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
            }
        }

        if (transformCbIndex_ == static_cast<uint32_t>(-1)) {
            transformCbIndex_ = engine->GetTransformBufferManager()->Allocate();
        }
    }

    ResolveCustomPSO();
}

void Object2DResource::Map() {
    if (vertexResource_) {
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    }
    if (indexResource_) {
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    }
}

void Object2DResource::Unmap() {
    if (vertexResource_ && vertexData_) {
        vertexResource_->Unmap(0, nullptr);
        vertexData_ = nullptr;
    }
    if (indexResource_ && indexData_) {
        indexResource_->Unmap(0, nullptr);
        indexData_ = nullptr;
    }
}

void Object2DResource::UpdateTransform(const Camera& camera) {

    transformationMatrix_.world =
        Irufemi::Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // 2D なので正射影行列を掛ける
    transformationMatrix_.WVP = Irufemi::Math::Multiply(transformationMatrix_.world, camera.GetOrthographicMatrix());

    // CPU側のマテリアルキャッシュにのみ反映させる
    cpuMaterialData_.uvTransform =
        Irufemi::Math::MakeAffineMatrix(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);

    MarkAsDirty();
}

D3D12_GPU_VIRTUAL_ADDRESS Object2DResource::GetTransformVAddress() const {
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

D3D12_GPU_VIRTUAL_ADDRESS Object2DResource::GetMaterialVAddress() const {
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

void Object2DResource::SyncBeforeDraw() {
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

            // テクスチャのインデックスを解決して反映
            auto* tm = textureManager_;
            if (!tm) {
                tm = engine->GetTextureManager();
            }
            if (tm) {
                cpuMaterialData_.textureIndex = tm->GetSrvIndex(textureHandle_);
            } else {
                cpuMaterialData_.textureIndex = 0;
            }

            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, frameIndex);
            }
        }

        // 頂点・インデックスデータの更新があればGPUに転送
        if (vertexData_ && !vertexDataList_.empty()) {
            std::memcpy(vertexData_, vertexDataList_.data(), sizeof(VertexData) * vertexDataList_.size());
        }
        if (indexData_ && !indexDataList_.empty()) {
            std::memcpy(indexData_, indexDataList_.data(), sizeof(uint32_t) * indexDataList_.size());
        }
    }
}
