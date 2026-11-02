#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/DrawManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Core/Math/Math.h"
#include "Core/System/IrufemiEngine.h"

DebugPrimitiveRenderer::~DebugPrimitiveRenderer() {
    if (srvAllocator_ && dx_) {
        for (uint32_t& idx : sphereSrvIndex_) {
            if (idx != UINT32_MAX) {
                srvAllocator_->FreeAfterFence(idx, dx_->GetCurrentFrameFenceValue());
                idx = UINT32_MAX;
            }
        }
        for (uint32_t& idx : cubeSrvIndex_) {
            if (idx != UINT32_MAX) {
                srvAllocator_->FreeAfterFence(idx, dx_->GetCurrentFrameFenceValue());
                idx = UINT32_MAX;
            }
        }
    }
    if (dx_) {
        for (auto& buf : sphereInstanceBuffer_) {
            if (buf) {
                dx_->ReleaseAfterFence(buf);
                buf.Reset();
            }
        }
        for (auto& buf : cubeInstanceBuffer_) {
            if (buf) {
                dx_->ReleaseAfterFence(buf);
                buf.Reset();
            }
        }
    }
}

void DebugPrimitiveRenderer::Initialize(DirectXCommon* dx, DrawManager* drawM, DescriptorPool* srvAlloc) {
    dx_ = dx;
    drawManager_ = drawM;
    srvAllocator_ = srvAlloc;

    sphereSrvIndex_.fill(UINT32_MAX);
    cubeSrvIndex_.fill(UINT32_MAX);

    drawSphereInstances_.resize(maxSphereInstances_);
    simSphereInstances_.resize(maxSphereInstances_);
    drawCubeInstances_.resize(maxCubeInstances_);
    simCubeInstances_.resize(maxCubeInstances_);

    CreateSphereResource();
    CreateCubeResource();
}

void DebugPrimitiveRenderer::CreateSphereResource() {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    const int segments = 32;
    for (int ring = 0; ring < 3; ++ring) {
        for (int i = 0; i < segments; ++i) {
            float angle = static_cast<float>(i) / segments * Irufemi::Math::TwoPI;
            float c = std::cos(angle);
            float s = std::sin(angle);

            Irufemi::Vector3 pos;
            if (ring == 0) {
                pos = {c, s, 0.0f};
            } else if (ring == 1) {
                pos = {0.0f, c, s};
            } else {
                pos = {s, 0.0f, c};
            }

            VertexData vd{};
            vd.position = {pos.x, pos.y, pos.z, 1.0f};
            vd.color = {1.0f, 1.0f, 1.0f, 1.0f};
            vertices.push_back(vd);

            uint32_t baseIdx = ring * segments;
            indices.push_back(baseIdx + i);
            indices.push_back(baseIdx + (i + 1) % segments);
        }
    }

    sphereIndexCount_ = static_cast<uint32_t>(indices.size());

    // Create Vertex Buffer
    size_t vbSize = vertices.size() * sizeof(VertexData);
    sphereVertexResource_ = dx_->CreateBufferResource(vbSize);
    VertexData* mappedVB = nullptr;
    sphereVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVB));
    std::memcpy(mappedVB, vertices.data(), vbSize);
    sphereVertexResource_->Unmap(0, nullptr);

    sphereVBV_.BufferLocation = sphereVertexResource_->GetGPUVirtualAddress();
    sphereVBV_.SizeInBytes = static_cast<UINT>(vbSize);
    sphereVBV_.StrideInBytes = sizeof(VertexData);

    // Create Index Buffer
    size_t ibSize = indices.size() * sizeof(uint32_t);
    sphereIndexResource_ = dx_->CreateBufferResource(ibSize);
    uint32_t* mappedIB = nullptr;
    sphereIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIB));
    std::memcpy(mappedIB, indices.data(), ibSize);
    sphereIndexResource_->Unmap(0, nullptr);

    sphereIBV_.BufferLocation = sphereIndexResource_->GetGPUVirtualAddress();
    sphereIBV_.SizeInBytes = static_cast<UINT>(ibSize);
    sphereIBV_.Format = DXGI_FORMAT_R32_UINT;
}

void DebugPrimitiveRenderer::CreateCubeResource() {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices = {
        0, 1, 1, 2, 2, 3, 3, 0, // Bottom
        4, 5, 5, 6, 6, 7, 7, 4, // Top
        0, 4, 1, 5, 2, 6, 3, 7  // Pillars
    };

    Irufemi::Vector3 positions[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.5f, -0.5f},  {0.5f, 0.5f, -0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},
    };

    for (int i = 0; i < 8; ++i) {
        VertexData vd{};
        vd.position = {positions[i].x, positions[i].y, positions[i].z, 1.0f};
        vd.color = {1.0f, 1.0f, 1.0f, 1.0f};
        vertices.push_back(vd);
    }

    cubeIndexCount_ = static_cast<uint32_t>(indices.size());

    // Create Vertex Buffer
    size_t vbSize = vertices.size() * sizeof(VertexData);
    cubeVertexResource_ = dx_->CreateBufferResource(vbSize);
    VertexData* mappedVB = nullptr;
    cubeVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVB));
    std::memcpy(mappedVB, vertices.data(), vbSize);
    cubeVertexResource_->Unmap(0, nullptr);

    cubeVBV_.BufferLocation = cubeVertexResource_->GetGPUVirtualAddress();
    cubeVBV_.SizeInBytes = static_cast<UINT>(vbSize);
    cubeVBV_.StrideInBytes = sizeof(VertexData);

    // Create Index Buffer
    size_t ibSize = indices.size() * sizeof(uint32_t);
    cubeIndexResource_ = dx_->CreateBufferResource(ibSize);
    uint32_t* mappedIB = nullptr;
    cubeIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIB));
    std::memcpy(mappedIB, indices.data(), ibSize);
    cubeIndexResource_->Unmap(0, nullptr);

    cubeIBV_.BufferLocation = cubeIndexResource_->GetGPUVirtualAddress();
    cubeIBV_.SizeInBytes = static_cast<UINT>(ibSize);
    cubeIBV_.Format = DXGI_FORMAT_R32_UINT;
}

void DebugPrimitiveRenderer::ClearInstances() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeDrawSphereCount_ = 0;
    activeSimSphereCount_ = 0;
    activeDrawCubeCount_ = 0;
    activeSimCubeCount_ = 0;
}

void DebugPrimitiveRenderer::ClearDrawInstances() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeDrawSphereCount_ = 0;
    activeDrawCubeCount_ = 0;
}

void DebugPrimitiveRenderer::ClearSimulationInstances() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeSimSphereCount_ = 0;
    activeSimCubeCount_ = 0;
}

void DebugPrimitiveRenderer::BeginSimulationFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeSimSphereCount_ = 0;
    activeSimCubeCount_ = 0;
    isSimulating_ = true;
}

void DebugPrimitiveRenderer::EndSimulationFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    isSimulating_ = false;
}

void DebugPrimitiveRenderer::AddSphere(const Irufemi::Vector3& center, float radius, const Irufemi::Vector4& color,
                                       DebugCategory category) {
    std::lock_guard<std::mutex> lock(mutex_);
    Irufemi::Matrix4x4 world =
        Irufemi::Math::MakeScaleMatrix({radius, radius, radius}) * Irufemi::Math::MakeTranslateMatrix(center);

    if (isSimulating_) {
        if (activeSimSphereCount_ < maxSphereInstances_) {
            auto& instance = simSphereInstances_[activeSimSphereCount_];
            instance.world = world;
            instance.color = color;
            instance.category = category;
            activeSimSphereCount_++;
        }
    } else {
        if (activeDrawSphereCount_ < maxSphereInstances_) {
            auto& instance = drawSphereInstances_[activeDrawSphereCount_];
            instance.world = world;
            instance.color = color;
            instance.category = category;
            activeDrawSphereCount_++;
        }
    }
}

void DebugPrimitiveRenderer::AddCube(const Irufemi::Matrix4x4& transform, const Irufemi::Vector4& color,
                                     DebugCategory category) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isSimulating_) {
        if (activeSimCubeCount_ < maxCubeInstances_) {
            auto& instance = simCubeInstances_[activeSimCubeCount_];
            instance.world = transform;
            instance.color = color;
            instance.category = category;
            activeSimCubeCount_++;
        }
    } else {
        if (activeDrawCubeCount_ < maxCubeInstances_) {
            auto& instance = drawCubeInstances_[activeDrawCubeCount_];
            instance.world = transform;
            instance.color = color;
            instance.category = category;
            activeDrawCubeCount_++;
        }
    }
}

void DebugPrimitiveRenderer::Update() {
    BuildInstanceBuffer();
}

void DebugPrimitiveRenderer::EnsureInstancingSRVs() {
    uint32_t frameIndex = dx_->GetFrameIndex();

    // Irufemi::Sphere
    if (sphereSrvIndex_[frameIndex] == UINT32_MAX) {
        sphereSrvIndex_[frameIndex] = srvAllocator_->Allocate();
        sphereSrvGPU_[frameIndex] = srvAllocator_->GetGPUHandle(sphereSrvIndex_[frameIndex]);
    }

    // Cube
    if (cubeSrvIndex_[frameIndex] == UINT32_MAX) {
        cubeSrvIndex_[frameIndex] = srvAllocator_->Allocate();
        cubeSrvGPU_[frameIndex] = srvAllocator_->GetGPUHandle(cubeSrvIndex_[frameIndex]);
    }
}

void DebugPrimitiveRenderer::BuildInstanceBuffer() {
    uint32_t frameIndex = dx_->GetFrameIndex();
    visibleSphereCount_[frameIndex] = 0;
    visibleCubeCount_[frameIndex] = 0;

    if (!isEnabled_) {
        return;
    }

    size_t totalSpheres = 0;
    size_t totalCubes = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        totalSpheres = activeSimSphereCount_ + activeDrawSphereCount_;
        totalCubes = activeSimCubeCount_ + activeDrawCubeCount_;
    }

    if (totalSpheres == 0 && totalCubes == 0) {
        return;
    }

    lastUpdateFrameIndex_ = frameIndex;

    Camera* activeCam = dx_->GetEngine()->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Irufemi::Sphere Buffer
    if (totalSpheres > 0) {
        if (totalSpheres > sphereInstanceCapacity_[frameIndex]) {
            if (sphereInstanceBuffer_[frameIndex]) {
                sphereInstanceBuffer_[frameIndex]->Unmap(0, nullptr);
                dx_->ReleaseAfterFence(sphereInstanceBuffer_[frameIndex]);
                sphereInstanceBuffer_[frameIndex].Reset();
            }
            uint32_t doubled = sphereInstanceCapacity_[frameIndex] * 2;
            uint32_t newCapacity =
                static_cast<uint32_t>(totalSpheres) > doubled ? static_cast<uint32_t>(totalSpheres) : doubled;
            if (newCapacity < 64) {
                newCapacity = 64;
            }
            sphereInstanceCapacity_[frameIndex] = newCapacity;
            size_t size = sizeof(GPUInstanceData) * sphereInstanceCapacity_[frameIndex];
            sphereInstanceBuffer_[frameIndex] = dx_->CreateBufferResource(size);
            sphereInstanceBuffer_[frameIndex]->Map(0, nullptr,
                                                   reinterpret_cast<void**>(&sphereInstanceDataMap_[frameIndex]));

            EnsureInstancingSRVs();
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = sphereInstanceCapacity_[frameIndex];
            srvDesc.Buffer.StructureByteStride = sizeof(GPUInstanceData);

            dx_->GetDevice()->CreateShaderResourceView(sphereInstanceBuffer_[frameIndex].Get(), &srvDesc,
                                                       srvAllocator_->GetCPUHandle(sphereSrvIndex_[frameIndex]));
        }

        size_t count = 0;
        for (size_t i = 0; i < activeSimSphereCount_; ++i) {
            if (categoryMask_ & static_cast<uint32_t>(simSphereInstances_[i].category)) {
                sphereInstanceDataMap_[frameIndex][count].world = simSphereInstances_[i].world;
                sphereInstanceDataMap_[frameIndex][count].color = simSphereInstances_[i].color;
                count++;
            }
        }
        for (size_t i = 0; i < activeDrawSphereCount_; ++i) {
            if (categoryMask_ & static_cast<uint32_t>(drawSphereInstances_[i].category)) {
                sphereInstanceDataMap_[frameIndex][count].world = drawSphereInstances_[i].world;
                sphereInstanceDataMap_[frameIndex][count].color = drawSphereInstances_[i].color;
                count++;
            }
        }
        visibleSphereCount_[frameIndex] = count;
    }

    // Cube Buffer
    if (totalCubes > 0) {
        if (totalCubes > cubeInstanceCapacity_[frameIndex]) {
            if (cubeInstanceBuffer_[frameIndex]) {
                cubeInstanceBuffer_[frameIndex]->Unmap(0, nullptr);
                dx_->ReleaseAfterFence(cubeInstanceBuffer_[frameIndex]);
                cubeInstanceBuffer_[frameIndex].Reset();
            }
            uint32_t doubled = cubeInstanceCapacity_[frameIndex] * 2;
            uint32_t newCapacity =
                static_cast<uint32_t>(totalCubes) > doubled ? static_cast<uint32_t>(totalCubes) : doubled;
            if (newCapacity < 64) {
                newCapacity = 64;
            }
            cubeInstanceCapacity_[frameIndex] = newCapacity;
            size_t size = sizeof(GPUInstanceData) * cubeInstanceCapacity_[frameIndex];
            cubeInstanceBuffer_[frameIndex] = dx_->CreateBufferResource(size);
            cubeInstanceBuffer_[frameIndex]->Map(0, nullptr,
                                                 reinterpret_cast<void**>(&cubeInstanceDataMap_[frameIndex]));

            EnsureInstancingSRVs();
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = cubeInstanceCapacity_[frameIndex];
            srvDesc.Buffer.StructureByteStride = sizeof(GPUInstanceData);

            dx_->GetDevice()->CreateShaderResourceView(cubeInstanceBuffer_[frameIndex].Get(), &srvDesc,
                                                       srvAllocator_->GetCPUHandle(cubeSrvIndex_[frameIndex]));
        }

        size_t count = 0;
        for (size_t i = 0; i < activeSimCubeCount_; ++i) {
            if (categoryMask_ & static_cast<uint32_t>(simCubeInstances_[i].category)) {
                cubeInstanceDataMap_[frameIndex][count].world = simCubeInstances_[i].world;
                cubeInstanceDataMap_[frameIndex][count].color = simCubeInstances_[i].color;
                count++;
            }
        }
        for (size_t i = 0; i < activeDrawCubeCount_; ++i) {
            if (categoryMask_ & static_cast<uint32_t>(drawCubeInstances_[i].category)) {
                cubeInstanceDataMap_[frameIndex][count].world = drawCubeInstances_[i].world;
                cubeInstanceDataMap_[frameIndex][count].color = drawCubeInstances_[i].color;
                count++;
            }
        }
        visibleCubeCount_[frameIndex] = count;
    }
}

void DebugPrimitiveRenderer::Draw() {
    if (!isEnabled_) {
        return;
    }

    uint32_t frameIndex = dx_->GetFrameIndex();

    if (visibleSphereCount_[frameIndex] > 0) {
        RenderPackets::DebugPrimitivePacket packet{};
        packet.vertexBufferView = sphereVBV_;
        packet.indexBufferView = sphereIBV_;
        packet.indexCount = sphereIndexCount_;
        packet.instanceCount = static_cast<UINT>(visibleSphereCount_[frameIndex]);
        packet.instancingSrvHandleGPU = sphereSrvGPU_[frameIndex];
        drawManager_->SubmitDebugPrimitive(packet);
    }

    if (visibleCubeCount_[frameIndex] > 0) {
        RenderPackets::DebugPrimitivePacket packet{};
        packet.vertexBufferView = cubeVBV_;
        packet.indexBufferView = cubeIBV_;
        packet.indexCount = cubeIndexCount_;
        packet.instanceCount = static_cast<UINT>(visibleCubeCount_[frameIndex]);
        packet.instancingSrvHandleGPU = cubeSrvGPU_[frameIndex];
        drawManager_->SubmitDebugPrimitive(packet);
    }
}
