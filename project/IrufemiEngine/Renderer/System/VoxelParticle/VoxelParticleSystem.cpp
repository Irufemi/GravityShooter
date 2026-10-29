#include "Core/Utility/ErrorUtility.h"
#define NOMINMAX
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/System/VoxelParticle/VoxelParticleSystem.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Math.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/UI/DebugUI.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Data/VertexData.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Resource/Model/ModelManager.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <cassert>
#include <cstdio>
#include "Core/Math/Geometry/OBB.h"
#include "Physics/Collision/Collision.h"
#include "Core/Shape/Sphere.h"
#include <Windows.h>
#include <algorithm>

IrufemiEngine* VoxelParticleSystem::engine_ = nullptr;

VoxelParticleSystem::~VoxelParticleSystem() {
    if (engine_) {
        if (auto* dxCommon = engine_->GetDirectXCommon()) {
            uint64_t fv = dxCommon->GetCurrentFrameFenceValue();
            if (auto* srvPool = dxCommon->GetSrvPool()) {
                srvPool->FreeAfterFence(voxelSrvIndex_, fv);
                srvPool->FreeAfterFence(particleUavIndex_, fv);
                srvPool->FreeAfterFence(particleSrvIndex_, fv);
                for (int i = 0; i < 3; ++i) {
                    srvPool->FreeAfterFence(emittersSrvIndex_[i], fv);
                }
            }
            dxCommon->ReleaseAfterFence(voxelBuffer_);
            dxCommon->ReleaseAfterFence(particleBuffer_);
            dxCommon->ReleaseAfterFence(cubeVertexBuffer_);
            dxCommon->ReleaseAfterFence(cubeIndexBuffer_);
            for (int i = 0; i < 3; ++i) {
                if (emittersMappedData_[i]) {
                    emittersBuffer_[i]->Unmap(0, nullptr);
                }
                dxCommon->ReleaseAfterFence(emittersBuffer_[i]);
            }
        }
    }
}

void VoxelParticleSystem::Initialize(const std::string& modelName, const Irufemi::Vector3Int& resolution) {
    IRUFEMI_ASSERT(engine_);
    status_.store(LoadingStatus::Loading);
    auto* modelManager = engine_->GetObjModelManager();
    auto asyncData = std::make_shared<AsyncLoadData>();
    asyncData->modelName = modelName;
    asyncData_ = asyncData;

    ResourceHandle modelHandle = modelManager->LoadModel(modelName);
    initializeFuture_ = modelManager->EnqueueTask([asyncData, modelName, resolution, modelManager, modelHandle]() {
        auto vModel = modelManager->GetVoxelizedModel(modelName, resolution);
        if (!vModel || vModel->voxels.empty()) {
            asyncData->status.store(LoadingStatus::Failed);
            /**
             * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
             */
            Log::OutPutLog(std::cerr, "[Voxel] ERROR: Voxel count is ZERO or failed to load.");
            return;
        }
        asyncData->voxelCount = static_cast<uint32_t>(vModel->voxels.size());
        asyncData->voxelModel = vModel;
        asyncData->status.store(LoadingStatus::ReadyToCreateResources);
    });
}

void VoxelParticleSystem::FinishInitialization() {
    if (status_.load() != LoadingStatus::ReadyToCreateResources) {
        return;
    }

    float voxelW = (voxelModel_->aabbMax.x - voxelModel_->aabbMin.x) / voxelModel_->resolution.x;
    float voxelH = (voxelModel_->aabbMax.y - voxelModel_->aabbMin.y) / voxelModel_->resolution.y;
    float voxelD = (voxelModel_->aabbMax.z - voxelModel_->aabbMin.z) / voxelModel_->resolution.z;

    CreateCubeMesh(voxelW, voxelH, voxelD);

    // 動的MaxInstances計算 (最大32)
    uint64_t targetVRAM = 100 * 1024 * 1024;
    uint64_t bytesPerInstance = static_cast<uint64_t>(voxelCount_) * sizeof(VoxelParticle);
    maxInstances_ = static_cast<uint32_t>(targetVRAM / bytesPerInstance);
    if (maxInstances_ == 0) {
        maxInstances_ = 1;
    }
    if (maxInstances_ > 32) {
        maxInstances_ = 32;
    }

    emittersData_.resize(maxInstances_);

    CreateResources();
    CreatePSO();

    needsInitialize_ = true;
    status_.store(LoadingStatus::Loaded);
}

void VoxelParticleSystem::EnsureInitialized() {
    if (status_.load() == LoadingStatus::Loaded || status_.load() == LoadingStatus::Failed) {
        return;
    }
    if (initializeFuture_.valid()) {
        initializeFuture_.wait();
    }
    if (asyncData_) {
        auto s = asyncData_->status.load();
        if (s == LoadingStatus::ReadyToCreateResources) {
            voxelModel_ = std::move(asyncData_->voxelModel);
            voxelCount_ = asyncData_->voxelCount;
            status_.store(LoadingStatus::ReadyToCreateResources);
            asyncData_.reset();
            FinishInitialization();
        } else if (s == LoadingStatus::Failed) {
            status_.store(LoadingStatus::Failed);
            Log::OutPutLog(std::cout,
                           "[VoxelParticleSystem] Voxelization FAILED for model: " + asyncData_->modelName + "\n");
            asyncData_.reset();
        }
    }
}

void VoxelParticleSystem::Update(float deltaTime) {
    if (asyncData_) {
        auto s = asyncData_->status.load();
        if (s == LoadingStatus::ReadyToCreateResources) {
            voxelModel_ = std::move(asyncData_->voxelModel);
            voxelCount_ = asyncData_->voxelCount;
            status_.store(LoadingStatus::ReadyToCreateResources);

            asyncData_.reset();
            FinishInitialization();
        } else if (s == LoadingStatus::Failed) {
            status_.store(LoadingStatus::Failed);
            Log::OutPutLog(std::cout,
                           "[VoxelParticleSystem] Voxelization FAILED for model: " + asyncData_->modelName + "\n");
            asyncData_.reset();
        }
    }

    if (status_.load() != LoadingStatus::Loaded || voxelCount_ == 0) {
        return;
    }

    UpdateBuffers();

    static float totalTime = 0.0f;
    totalTime += deltaTime;
    perFrameData_.time = totalTime;
    perFrameData_.deltaTime = deltaTime;

    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
    perFrameBuffer_.Update(perFrameData_, frameIndex);

    voxelSystemCbData_.voxelCount = voxelCount_;
    voxelSystemCbBuffer_.Update(voxelSystemCbData_, frameIndex);

    if (engine_ && engine_->GetDrawManager()) {
        engine_->GetDrawManager()->RegisterComputeTask(this);
    }
}

void VoxelParticleSystem::UpdateEmitterData(uint32_t index, const VoxelEmitter& data) {
    if (index < emittersData_.size()) {
        emittersData_[index] = data;
    }
}

void VoxelParticleSystem::UpdateBuffers() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
    if (emittersMappedData_[frameIndex]) {
        std::memcpy(emittersMappedData_[frameIndex], emittersData_.data(), sizeof(VoxelEmitter) * maxInstances_);
    }
}

bool VoxelParticleSystem::IsInFrustum(uint32_t index) const {
    if (!engine_ || index >= maxInstances_) {
        return false;
    }
    if (emittersData_[index].emit == 0 && emittersData_[index].lifeTime == 0.0f) {
        return false;
    }

    auto* camManager = engine_->GetCameraManager();
    if (!camManager) {
        return true;
    }
    Camera* activeCam = camManager->GetActiveCamera();
    if (!activeCam) {
        return true;
    }

    Irufemi::Sphere sphere;
    sphere.center = emittersData_[index].emitPosition;
    sphere.radius = 80.0f;

    return Irufemi::Collision::IsCollision(activeCam->GetFrustum(), sphere);
}

void VoxelParticleSystem::DispatchCompute() {
    if (status_.load() != LoadingStatus::Loaded || !voxelBuffer_ || !engine_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = engine_->GetCommandList();
    auto* dxCommon = engine_->GetDirectXCommon();
    uint32_t frameIndex = dxCommon->GetFrameIndex();

    ID3D12DescriptorHeap* ppHeaps[] = {dxCommon->GetSrvPool()->GetHeap()};
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    commandList->SetComputeRootSignature(dxCommon->GetComputeRootSignature());

    uint32_t totalThreadCount = voxelCount_ * maxInstances_;
    uint32_t dispatchGroups = (totalThreadCount + 63) / 64;

    if (needsInitialize_) {
        commandList->SetPipelineState(initializePSO_.Get());
        commandList->SetComputeRootDescriptorTable(0, voxelSrvHandleGPU_);    // t0 (gVoxels)
        commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_); // u0 (gParticles)
        commandList->Dispatch(dispatchGroups, 1, 1);
        DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());
        needsInitialize_ = false;
    }

    // Emit
    commandList->SetPipelineState(emitPSO_.Get());
    commandList->SetComputeRootDescriptorTable(0, voxelSrvHandleGPU_);                                       // t0
    commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);                                    // u0
    commandList->SetComputeRootConstantBufferView(4, voxelSystemCbBuffer_.GetGPUVirtualAddress(frameIndex)); // b0
    commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex));      // b1
    // t1: gEmitters (ComputeRootDescriptorTable 1 is used for textures, wait, we must set t1 properly)
    // Engine's compute root signature might not map t1 properly if we don't have a slot.
    // Wait! Compute root signature in DirectXCommon:
    //   0: DescriptorTable(t0)
    //   1: DescriptorTable(t1)  (in my GPUParticle architecture, this might be available!)
    //   2: DescriptorTable(t2)
    //   3: DescriptorTable(u0)
    //   4: CBV(b0)
    //   5: CBV(b1)
    //   6: CBV(b2)
    commandList->SetComputeRootDescriptorTable(1, emittersSrvHandleGPU_[frameIndex]); // t1
    commandList->Dispatch(dispatchGroups, 1, 1);
    DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());

    // Update
    commandList->SetPipelineState(updatePSO_.Get());
    commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);                                    // u0
    commandList->SetComputeRootConstantBufferView(4, voxelSystemCbBuffer_.GetGPUVirtualAddress(frameIndex)); // b0
    commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex));      // b1
    commandList->SetComputeRootDescriptorTable(1, emittersSrvHandleGPU_[frameIndex]);                        // t1
    commandList->Dispatch(dispatchGroups, 1, 1);
    DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());
}

void VoxelParticleSystem::Draw() {
    if (status_.load() != LoadingStatus::Loaded || !voxelBuffer_ || !engine_) {
        return;
    }
    if (engine_->GetDrawManager()->IsShadowPass()) {
        return;
    }

    auto* dxCommon = engine_->GetDirectXCommon();
    uint32_t frameIndex = dxCommon->GetFrameIndex();

    bool anyActive = false;
    for (uint32_t i = 0; i < maxInstances_; ++i) {
        if (emittersData_[i].emit != 0 || emittersData_[i].lifeTime > 0.0f) {
            if (IsInFrustum(i)) {
                anyActive = true;
                break;
            }
        }
    }
    if (!anyActive) {
        return;
    }

    engine_->GetDrawManager()->SubmitVoxelParticle(
        voxelCount_ * maxInstances_, cubeVertexBufferView_, cubeIndexBufferView_, cubeIndexCount_,
        voxelSystemCbBuffer_.GetGPUVirtualAddress(frameIndex), emittersSrvHandleGPU_[frameIndex], particleSrvHandleGPU_,
        particleBuffer_.Get(), drawPSO_.Get());
}

void VoxelParticleSystem::CreateCubeMesh(float sizeX, float sizeY, float sizeZ) {
    auto* dxCommon = engine_->GetDirectXCommon();
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    const float hx = sizeX * 0.5f;
    const float hy = sizeY * 0.5f;
    const float hz = sizeZ * 0.5f;
    vertices = {
        {{-hx, -hy, -hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-hx, hy, -hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{hx, hy, -hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{hx, -hy, -hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-hx, hy, -hz, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-hx, -hy, -hz, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{hx, -hy, -hz, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{hx, hy, -hz, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{-hx, -hy, -hz, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{hx, -hy, -hz, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{-hx, hy, -hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{hx, hy, -hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    };
    indices = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };
    cubeIndexCount_ = static_cast<uint32_t>(indices.size());

    cubeVertexBuffer_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());
    VertexData* vertexData = nullptr;
    cubeVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    cubeVertexBuffer_->Unmap(0, nullptr);
    cubeVertexBufferView_.BufferLocation = cubeVertexBuffer_->GetGPUVirtualAddress();
    cubeVertexBufferView_.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertices.size());
    cubeVertexBufferView_.StrideInBytes = sizeof(VertexData);

    cubeIndexBuffer_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices.size());
    uint32_t* indexData = nullptr;
    cubeIndexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
    cubeIndexBuffer_->Unmap(0, nullptr);
    cubeIndexBufferView_.BufferLocation = cubeIndexBuffer_->GetGPUVirtualAddress();
    cubeIndexBufferView_.SizeInBytes = sizeof(uint32_t) * static_cast<UINT>(indices.size());
    cubeIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void VoxelParticleSystem::CreateResources() {
    auto* dxCommon = engine_->GetDirectXCommon();
    auto* srvPool = dxCommon->GetSrvPool();
    auto* device = engine_->GetDevice();

    voxelBuffer_ = dxCommon->CreateBufferResource(sizeof(Irufemi::Voxel) * voxelCount_);
    Irufemi::Voxel* voxelData = nullptr;
    voxelBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&voxelData));
    std::memcpy(voxelData, voxelModel_->voxels.data(), sizeof(Irufemi::Voxel) * voxelCount_);
    voxelBuffer_->Unmap(0, nullptr);

    voxelSrvIndex_ = srvPool->Allocate();
    voxelSrvHandleCPU_ = srvPool->GetCPUHandle(voxelSrvIndex_);
    voxelSrvHandleGPU_ = srvPool->GetGPUHandle(voxelSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC voxelSrvDesc{};
    voxelSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    voxelSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    voxelSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    voxelSrvDesc.Buffer.FirstElement = 0;
    voxelSrvDesc.Buffer.NumElements = voxelCount_;
    voxelSrvDesc.Buffer.StructureByteStride = sizeof(Irufemi::Voxel);
    device->CreateShaderResourceView(voxelBuffer_.Get(), &voxelSrvDesc, voxelSrvHandleCPU_);

    uint32_t totalVoxels = voxelCount_ * maxInstances_;
    particleBuffer_ = dxCommon->CreateUAVBufferResource(sizeof(VoxelParticle) * totalVoxels);

    particleUavIndex_ = srvPool->Allocate();
    particleUavHandleCPU_ = srvPool->GetCPUHandle(particleUavIndex_);
    particleUavHandleGPU_ = srvPool->GetGPUHandle(particleUavIndex_);

    D3D12_UNORDERED_ACCESS_VIEW_DESC particleUavDesc{};
    particleUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    particleUavDesc.Buffer.FirstElement = 0;
    particleUavDesc.Buffer.NumElements = totalVoxels;
    particleUavDesc.Buffer.StructureByteStride = sizeof(VoxelParticle);
    device->CreateUnorderedAccessView(particleBuffer_.Get(), nullptr, &particleUavDesc, particleUavHandleCPU_);

    particleSrvIndex_ = srvPool->Allocate();
    particleSrvHandleCPU_ = srvPool->GetCPUHandle(particleSrvIndex_);
    particleSrvHandleGPU_ = srvPool->GetGPUHandle(particleSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC particleSrvDesc{};
    particleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    particleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    particleSrvDesc.Buffer.FirstElement = 0;
    particleSrvDesc.Buffer.NumElements = totalVoxels;
    particleSrvDesc.Buffer.StructureByteStride = sizeof(VoxelParticle);
    device->CreateShaderResourceView(particleBuffer_.Get(), &particleSrvDesc, particleSrvHandleCPU_);

    for (int i = 0; i < 3; ++i) {
        emittersBuffer_[i] = dxCommon->CreateBufferResource(sizeof(VoxelEmitter) * maxInstances_);
        emittersBuffer_[i]->Map(0, nullptr, reinterpret_cast<void**>(&emittersMappedData_[i]));

        emittersSrvIndex_[i] = srvPool->Allocate();
        emittersSrvHandleCPU_[i] = srvPool->GetCPUHandle(emittersSrvIndex_[i]);
        emittersSrvHandleGPU_[i] = srvPool->GetGPUHandle(emittersSrvIndex_[i]);

        D3D12_SHADER_RESOURCE_VIEW_DESC emitterSrvDesc{};
        emitterSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        emitterSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        emitterSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        emitterSrvDesc.Buffer.FirstElement = 0;
        emitterSrvDesc.Buffer.NumElements = maxInstances_;
        emitterSrvDesc.Buffer.StructureByteStride = sizeof(VoxelEmitter);
        device->CreateShaderResourceView(emittersBuffer_[i].Get(), &emitterSrvDesc, emittersSrvHandleCPU_[i]);
    }

    perFrameBuffer_.Initialize(dxCommon);
    voxelSystemCbBuffer_.Initialize(dxCommon);
}

void VoxelParticleSystem::CreatePSO() {
    auto* dxCommon = engine_->GetDirectXCommon();
    auto* psoManager = dxCommon->GetPSOManager();

    initializePSO_ = psoManager->GetComputePSO("VoxelParticleInitialize");
    emitPSO_ = psoManager->GetComputePSO("VoxelParticleEmit");
    updatePSO_ = psoManager->GetComputePSO("VoxelParticleUpdate");
    IRUFEMI_ASSERT(initializePSO_ && emitPSO_ && updatePSO_);

    drawPSO_ = psoManager->GetPSO("VoxelParticle", Irufemi::BlendMode::kBlendModeNormal, PSOManager::DepthWrite::Enable,
                                  PSOManager::CullMode::Back);
    IRUFEMI_ASSERT(drawPSO_);
}

void VoxelParticleSystem::Debug([[maybe_unused]] const char* name) {
#ifdef USE_IMGUI
    if (ImGui::TreeNode(name)) {
        ImGui::Text("Voxel Count: %u, Max Instances: %u", voxelCount_, maxInstances_);
        ImGui::TreePop();
    }
#endif
}
