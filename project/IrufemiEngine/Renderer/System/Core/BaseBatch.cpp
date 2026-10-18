#include "Core/Utility/ErrorUtility.h"
#include "Renderer/System/Core/BaseBatch.h"
#include "Renderer/Camera/CameraManager.h"
#include "Core/Shape/Sphere.h"
#include <cassert>
#include <cstring>
#include "Core/Math/Math.h"
#include "Physics/Collision/Collision.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include "Resource/Texture/TextureManager.h"
DirectXCommon* BaseBatch::dx_ = nullptr;
TextureManager* BaseBatch::textureManager_ = nullptr;
DrawManager* BaseBatch::drawManager_ = nullptr;
DescriptorPool* BaseBatch::srvPool_ = nullptr;

BaseBatch::BaseBatch() {
    instancingSrvIndex_.fill(UINT32_MAX);
    outputInstanceSrvIndex_.fill(UINT32_MAX);
    outputInstanceUavIndex_.fill(UINT32_MAX);
    indirectCommandUavIndex_.fill(UINT32_MAX);
}

BaseBatch::~BaseBatch() {
    if (srvPool_ && dx_) {
        auto freeIndex = [&](uint32_t& idx) {
            if (idx != UINT32_MAX) {
                srvPool_->FreeAfterFence(idx, dx_->GetCurrentFrameFenceValue());
                idx = UINT32_MAX;
            }
        };
        for (auto& idx : instancingSrvIndex_) {
            freeIndex(idx);
        }
        for (auto& idx : outputInstanceSrvIndex_) {
            freeIndex(idx);
        }
        for (auto& idx : outputInstanceUavIndex_) {
            freeIndex(idx);
        }
        for (auto& idx : indirectCommandUavIndex_) {
            freeIndex(idx);
        }
    }
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
            engine->GetMaterialBufferManager()->Free(materialCbIndex_);
        }
    }
    if (textureManager_ && textureHandle_.IsValid()) {
        textureManager_->ReleaseTexture(textureHandle_);
    }
}

void BaseBatch::SetColor(const Irufemi::Vector4& color) {
    cpuMaterialData_.color = color;
    hasCustomColor_ = true;
}

void BaseBatch::SetEnvironmentCoefficient(float coefficient) {
    cpuMaterialData_.environmentCoefficient = coefficient;
}

void BaseBatch::SetInstanceColor(uint32_t index, const Irufemi::Vector4& color) {
    if (index < instanceColors_.size()) {
        instanceColors_[index] = color;
        instanceDirty_ = true;
    } else {
        IRUFEMI_ASSERT(false && "BaseBatch::SetInstanceColor: index out of range");
    }
}

void BaseBatch::SetAllInstanceColor(const Irufemi::Vector4& color) {
    for (auto& c : instanceColors_) {
        c = color;
    }
    for (auto& c : instanceWorldColors_) {
        c = color;
    }
    instanceDirty_ = true;
}

void BaseBatch::AddInstance(const Irufemi::Transform& t, int32_t effectType, float effectParam, bool enableMask) {
    instances_.push_back(t);
    instanceColors_.push_back({1.0f, 1.0f, 1.0f, 1.0f});
    instanceEffects_.push_back({static_cast<float>(effectType), effectParam, enableMask ? 1.0f : 0.0f, 0.0f});
    instanceDirty_ = true;
}

void BaseBatch::AddInstance(const Irufemi::Transform& t, const Irufemi::Vector4& color, int32_t effectType,
                            float effectParam, bool enableMask) {
    instances_.push_back(t);
    instanceColors_.push_back(color);
    instanceEffects_.push_back({static_cast<float>(effectType), effectParam, enableMask ? 1.0f : 0.0f, 0.0f});
    instanceDirty_ = true;
}

void BaseBatch::AddInstance(const Irufemi::Vector3& center, float scale, const Irufemi::Vector3& rotate,
                            int32_t effectType, float effectParam, bool enableMask) {
    Irufemi::Transform t;
    t.translate = center;
    t.scale = {scale, scale, scale};
    t.rotate = rotate;
    instances_.push_back(t);
    instanceColors_.push_back({1.0f, 1.0f, 1.0f, 1.0f});
    instanceEffects_.push_back({static_cast<float>(effectType), effectParam, enableMask ? 1.0f : 0.0f, 0.0f});
    instanceDirty_ = true;
}

void BaseBatch::AddInstance(const Irufemi::Vector3& center, float scale, const Irufemi::Vector3& rotate,
                            const Irufemi::Vector4& color, int32_t effectType, float effectParam, bool enableMask) {
    Irufemi::Transform t;
    t.translate = center;
    t.scale = {scale, scale, scale};
    t.rotate = rotate;
    instances_.push_back(t);
    instanceColors_.push_back(color);
    instanceEffects_.push_back({static_cast<float>(effectType), effectParam, enableMask ? 1.0f : 0.0f, 0.0f});
    instanceDirty_ = true;
}

void BaseBatch::AddInstanceWorld(const Irufemi::Matrix4x4& world, const Irufemi::Vector4& color, int32_t effectType,
                                 float effectParam, bool enableMask) {
    instanceWorlds_.push_back(world);
    instanceWorldColors_.push_back(color);
    instanceWorldEffects_.push_back({static_cast<float>(effectType), effectParam, enableMask ? 1.0f : 0.0f, 0.0f});
    instanceDirty_ = true;
}

void BaseBatch::UpdateInstance(uint32_t index, const Irufemi::Transform& t) {
    if (index < instances_.size()) {
        instances_[index] = t;
        instanceDirty_ = true;
    } else {
        IRUFEMI_ASSERT(false && "BaseBatch::UpdateInstance: index out of range");
    }
}

void BaseBatch::ClearInstances() {
    instances_.clear();
    instanceColors_.clear();
    instanceEffects_.clear();
    instanceWorlds_.clear();
    instanceWorldColors_.clear();
    instanceWorldEffects_.clear();
    instanceDirty_ = true;
}

void BaseBatch::CreateOrResizeInstanceBuffer(uint32_t instanceCount) {
    const UINT stride = useGPUCulling_ ? sizeof(TransformData) : sizeof(InstanceData);
    const UINT sizeInBytes = std::max<UINT>(stride * instanceCount, stride);
    uint32_t frameIndex = dx_->GetFrameIndex();

    if (!instanceBuffer_[frameIndex] || instanceBuffer_[frameIndex]->GetDesc().Width < sizeInBytes) {
        instanceBuffer_[frameIndex] = dx_->CreateBufferResource(sizeInBytes);

        if (instancingSrvIndex_[frameIndex] == UINT32_MAX) {
            IRUFEMI_ASSERT(srvPool_);
            uint32_t idx = srvPool_->Allocate();
            if (idx == DescriptorPool::kInvalid) {
                /**
                 * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
                 */
                Log::OutPutLog(std::cerr, "BaseBatch SRV allocate failed");
                return;
            }
            instancingSrvIndex_[frameIndex] = idx;
            instancingSrvCPU_[frameIndex] = srvPool_->GetCPUHandle(idx);
            instancingSrvGPU_[frameIndex] = srvPool_->GetGPUHandle(idx);
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.FirstElement = 0;
        srv.Buffer.NumElements = instanceCount;
        srv.Buffer.StructureByteStride = stride;
        srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        dx_->GetDevice()->CreateShaderResourceView(instanceBuffer_[frameIndex].Get(), &srv,
                                                   instancingSrvCPU_[frameIndex]);
    }
}

void BaseBatch::CreateGPUCullingBuffers(uint32_t instanceCount) {
    const UINT stride = sizeof(InstanceData);
    const UINT sizeInBytes = std::max<UINT>(stride * instanceCount, stride);
    uint32_t frameIndex = dx_->GetFrameIndex();

    // 1. Output Buffer (SRV + UAV)
    if (!outputInstanceBuffer_[frameIndex] || outputInstanceBuffer_[frameIndex]->GetDesc().Width < sizeInBytes) {
        outputInstanceBuffer_[frameIndex] = dx_->CreateUAVBufferResource(sizeInBytes);

        IRUFEMI_ASSERT(srvPool_);
        if (outputInstanceSrvIndex_[frameIndex] == UINT32_MAX) {
            uint32_t idx = srvPool_->Allocate();
            outputInstanceSrvIndex_[frameIndex] = idx;
            outputInstanceSrvCPU_[frameIndex] = srvPool_->GetCPUHandle(idx);
            outputInstanceSrvGPU_[frameIndex] = srvPool_->GetGPUHandle(idx);
        }
        if (outputInstanceUavIndex_[frameIndex] == UINT32_MAX) {
            uint32_t idx = srvPool_->Allocate();
            outputInstanceUavIndex_[frameIndex] = idx;
            outputInstanceUavCPU_[frameIndex] = srvPool_->GetCPUHandle(idx);
            outputInstanceUavGPU_[frameIndex] = srvPool_->GetGPUHandle(idx);
        }

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.FirstElement = 0;
        srv.Buffer.NumElements = instanceCount;
        srv.Buffer.StructureByteStride = stride;
        srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        dx_->GetDevice()->CreateShaderResourceView(outputInstanceBuffer_[frameIndex].Get(), &srv,
                                                   outputInstanceSrvCPU_[frameIndex]);

        // UAV
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = DXGI_FORMAT_UNKNOWN;
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.FirstElement = 0;
        uav.Buffer.NumElements = instanceCount;
        uav.Buffer.StructureByteStride = stride;
        uav.Buffer.CounterOffsetInBytes = 0;
        uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        dx_->GetDevice()->CreateUnorderedAccessView(outputInstanceBuffer_[frameIndex].Get(), nullptr, &uav,
                                                    outputInstanceUavCPU_[frameIndex]);
    }

    // 2. Command Buffer (RWStructuredBuffer<uint> 5要素)
    // ExecuteIndirect用: D3D12_DRAW_INDEXED_ARGUMENTS = 20 bytes
    // ComputeShaderからUAVとして書き込むために D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS が必要
    // また、byte address/structured buffer として扱うためにサイズに余裕を持たせてもよいが、5*4=20バイトで固定。
    const UINT commandBufferSize = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    if (!indirectCommandBuffer_[frameIndex]) {
        // ComputeShaderでInterlockedAddや書き込みを行うためUAVバッファとして作成
        indirectCommandBuffer_[frameIndex] = dx_->CreateUAVBufferResource(commandBufferSize);

        if (indirectCommandUavIndex_[frameIndex] == UINT32_MAX) {
            uint32_t idx = srvPool_->Allocate();
            indirectCommandUavIndex_[frameIndex] = idx;
            indirectCommandUavCPU_[frameIndex] = srvPool_->GetCPUHandle(idx);
            indirectCommandUavGPU_[frameIndex] = srvPool_->GetGPUHandle(idx);
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = DXGI_FORMAT_UNKNOWN;
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.FirstElement = 0;
        uav.Buffer.NumElements = commandBufferSize / 4; // uint array
        uav.Buffer.StructureByteStride = 4;
        uav.Buffer.CounterOffsetInBytes = 0;
        uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        dx_->GetDevice()->CreateUnorderedAccessView(indirectCommandBuffer_[frameIndex].Get(), nullptr, &uav,
                                                    indirectCommandUavCPU_[frameIndex]);
    }

    if (!indirectCommandUploadBuffer_[frameIndex]) {
        indirectCommandUploadBuffer_[frameIndex] = dx_->CreateBufferResource(commandBufferSize);
    }

    // 3. Culling Data (Constant Buffer)
    const UINT cullingDataSize = (sizeof(CullingData) + 255) & ~255;
    if (!cullingDataBuffer_[frameIndex]) {
        cullingDataBuffer_[frameIndex] = dx_->CreateBufferResource(cullingDataSize);
    }
}

void BaseBatch::BuildInstanceBuffer(bool force) {
    if (instances_.empty() && instanceWorlds_.empty()) {
        visibleInstanceCount_ = 0;
        return;
    }
    if (!force && !instanceDirty_) {
        return;
    }

    const UINT totalCount = static_cast<UINT>(instances_.size() + instanceWorlds_.size());

    const Irufemi::Frustum* frustum = nullptr;

    Camera* activeCamera = nullptr;
    if (dx_ && dx_->GetEngine() && dx_->GetEngine()->GetCameraManager()) {
        activeCamera = dx_->GetEngine()->GetCameraManager()->GetActiveCamera();
    }

    if (activeCamera) {
        frustum = &activeCamera->GetFrustum();
    }

    float modelRadius = GetBoundingSphereRadius();

    CreateOrResizeInstanceBuffer(totalCount);
    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;

    if (useGPUCulling_) {
        std::vector<TransformData> temp;
        temp.reserve(totalCount);
        for (size_t i = 0; i < instances_.size(); ++i) {
            TransformData td{};
            td.position =
                Irufemi::Vector4(instances_[i].translate.x, instances_[i].translate.y, instances_[i].translate.z, 0.0f);
            td.rotation =
                Irufemi::Vector4(instances_[i].rotate.x, instances_[i].rotate.y, instances_[i].rotate.z, 0.0f);
            td.scale = Irufemi::Vector4(instances_[i].scale.x, instances_[i].scale.y, instances_[i].scale.z, 0.0f);
            td.color = instanceColors_[i];
            td.customEffect =
                (i < instanceEffects_.size()) ? instanceEffects_[i] : Irufemi::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            temp.push_back(td);
        }
        for (size_t i = 0; i < instanceWorlds_.size(); ++i) {
            TransformData td{};
            td.position = Irufemi::Vector4(instanceWorlds_[i].m[3][0], instanceWorlds_[i].m[3][1],
                                           instanceWorlds_[i].m[3][2], 0.0f);
            Irufemi::Vector3 euler = Irufemi::Math::ExtractEulerFromMatrix(instanceWorlds_[i]);
            td.rotation = Irufemi::Vector4(euler.x, euler.y, euler.z, 0.0f);
            float sx = Irufemi::Math::Length(
                Irufemi::Vector3{instanceWorlds_[i].m[0][0], instanceWorlds_[i].m[0][1], instanceWorlds_[i].m[0][2]});
            float sy = Irufemi::Math::Length(
                Irufemi::Vector3{instanceWorlds_[i].m[1][0], instanceWorlds_[i].m[1][1], instanceWorlds_[i].m[1][2]});
            float sz = Irufemi::Math::Length(
                Irufemi::Vector3{instanceWorlds_[i].m[2][0], instanceWorlds_[i].m[2][1], instanceWorlds_[i].m[2][2]});
            td.scale = Irufemi::Vector4(sx, sy, sz, 0.0f);
            td.color = instanceWorldColors_[i];
            td.customEffect = (i < instanceWorldEffects_.size()) ? instanceWorldEffects_[i]
                                                                 : Irufemi::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            temp.push_back(td);
        }

        uint8_t* dst = nullptr;
        HRESULT hr = instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&dst));
        ASSERT_IF_FAILED(hr);
        if (!temp.empty()) {
            std::memcpy(dst, temp.data(), temp.size() * sizeof(TransformData));
        }
        instanceBuffer_[frameIndex]->Unmap(0, nullptr);
        visibleInstanceCount_ = static_cast<uint32_t>(temp.size());

        CreateGPUCullingBuffers(totalCount);

        // Update CullingData Buffer
        if (cullingDataBuffer_[frameIndex]) {
            uint8_t* cullDst = nullptr;
            hr = cullingDataBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&cullDst));
            if (SUCCEEDED(hr)) {
                CullingData cullData{};
                cullData.maxInstanceCount = totalCount;
                cullData.localRadius = modelRadius;
                cullData.time = dx_->GetEngine() ? dx_->GetEngine()->GetGameTime() : 0.0f;

                if (frustum) {
                    for (int i = 0; i < 6; ++i) {
                        cullData.planes[i] = {frustum->planes[i].normal.x, frustum->planes[i].normal.y,
                                              frustum->planes[i].normal.z, -frustum->planes[i].distance};
                    }
                } else {
                    for (int i = 0; i < 6; ++i) {
                        cullData.planes[i] = {0, 0, 0, -10000.0f};
                    }
                }

                std::memcpy(cullDst, &cullData, sizeof(CullingData));
                cullingDataBuffer_[frameIndex]->Unmap(0, nullptr);
            }
        }
    } else {
        std::vector<InstanceData> temp;
        temp.reserve(totalCount);
        for (size_t i = 0; i < instances_.size(); ++i) {
            const auto& inst = instances_[i];
            if (isCullingEnabled_ && activeCamera && frustum) {
                float maxScale = (std::max)({inst.scale.x, inst.scale.y, inst.scale.z});
                Irufemi::Sphere boundingSphere;
                boundingSphere.center = inst.translate;
                boundingSphere.radius = modelRadius * maxScale * 1.1f;
                if (!Irufemi::Collision::IsCollision(*frustum, boundingSphere)) {
                    continue;
                }
            }

            InstanceData data;
            Irufemi::Matrix4x4 world = Irufemi::Math::MakeAffineMatrix(inst.scale, inst.rotate, inst.translate);
            Irufemi::Matrix4x4 worldForNormal = world;
            worldForNormal.m[3][0] = 0.0f;
            worldForNormal.m[3][1] = 0.0f;
            worldForNormal.m[3][2] = 0.0f;
            worldForNormal.m[3][3] = 1.0f;
            data.WVP = Irufemi::Math::MakeIdentity4x4();
            data.World = world;
            data.WorldInverseTranspose = Irufemi::Math::Transpose(Irufemi::Math::Inverse(worldForNormal));
            data.color = instanceColors_[i];
            data.customEffect =
                (i < instanceEffects_.size()) ? instanceEffects_[i] : Irufemi::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            temp.push_back(data);
        }
        for (size_t i = 0; i < instanceWorlds_.size(); ++i) {
            InstanceData data;
            data.WVP = Irufemi::Math::MakeIdentity4x4();
            data.World = instanceWorlds_[i];
            Irufemi::Matrix4x4 worldForNormal = instanceWorlds_[i];
            worldForNormal.m[3][0] = 0.0f;
            worldForNormal.m[3][1] = 0.0f;
            worldForNormal.m[3][2] = 0.0f;
            worldForNormal.m[3][3] = 1.0f;
            data.WorldInverseTranspose = Irufemi::Math::Transpose(Irufemi::Math::Inverse(worldForNormal));
            data.color = instanceWorldColors_[i];
            data.customEffect = (i < instanceWorldEffects_.size()) ? instanceWorldEffects_[i]
                                                                   : Irufemi::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            temp.push_back(data);
        }

        visibleInstanceCount_ = static_cast<uint32_t>(temp.size());
        if (visibleInstanceCount_ == 0) {
            instanceDirty_ = false;
            return;
        }

        uint8_t* dst = nullptr;
        HRESULT hr = instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&dst));
        ASSERT_IF_FAILED(hr);
        if (!temp.empty()) {
            std::memcpy(dst, temp.data(), temp.size() * sizeof(InstanceData));
        }
        instanceBuffer_[frameIndex]->Unmap(0, nullptr);
    }

    instanceDirty_ = false;
}

void BaseBatch::SyncBeforeDraw() {
    if (instanceDirty_) {
        BuildInstanceBuffer(false);
    }

    // [Bindless] テクスチャのインデックスを解決して反映
    if (textureManager_) {
        cpuMaterialData_.textureIndex = textureManager_->GetSrvIndex(textureHandle_);
        cpuMaterialData_.envMapIndex = textureManager_->GetWhiteCubeMapSrvIndex();
    } else {
        cpuMaterialData_.textureIndex = 0;
        cpuMaterialData_.envMapIndex = 0;
    }

    if (auto engine = dx_->GetEngine()) {
        uint32_t frameIndex = dx_->GetFrameIndex();
        if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, frameIndex);
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS BaseBatch::GetMaterialVAddress() const {
    if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
        return 0;
    }
    return dx_->GetEngine()->GetMaterialBufferManager()->GetGPUVirtualAddress(materialCbIndex_, dx_->GetFrameIndex());
}
