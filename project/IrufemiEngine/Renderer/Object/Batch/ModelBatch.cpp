#include "Core/Utility/ErrorUtility.h"
#include "Renderer/Object/Batch/ModelBatch.h"
#include <cassert>
#include "Core/System/IrufemiEngine.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"

ModelManager* ModelBatch::modelManager_ = nullptr;

void ModelBatch::Initialize(const std::string& objFilename) {
    IRUFEMI_ASSERT(modelManager_ && "ModelBatch::Initialize: ModelManager is not set.");
    modelHandle_ = modelManager_->LoadModel(objFilename);
    isResourcesInitialized_ = false;

    if (auto m = modelManager_->Resolve(modelHandle_)) {
        if (m->status.load() == ManagedModel::LoadingStatus::Loaded && m->cpuModel) {
            InitializeResources();
        }
    }
}

void ModelBatch::InitializeResources() {
    auto m = modelManager_ ? modelManager_->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel || m->cpuModel->meshes.empty()) {
        return;
    }
    const auto& mesh = m->cpuModel->meshes.front();

    CreateMaterialResources(mesh);
    EnsureSharedTexture(mesh);

    isResourcesInitialized_ = true;
}

const GpuMesh* ModelBatch::GetGpuMesh() const {
    auto m = modelManager_ ? modelManager_->Resolve(modelHandle_) : nullptr;
    if (m && !m->gpuMeshes.empty()) {
        return m->gpuMeshes.front().get();
    }
    return nullptr;
}

void ModelBatch::CreateMaterialResources(const ObjMesh& mesh) {
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
            materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();
        }
    }
    if (!hasCustomColor_) {
        cpuMaterialData_.color = mesh.material.color;
    }
    cpuMaterialData_.enableLighting = mesh.material.enableLighting;
    cpuMaterialData_.uvTransform = mesh.material.uvTransform;
    cpuMaterialData_.metallic = mesh.material.metallic;
    cpuMaterialData_.roughness = mesh.material.roughness;
    cpuMaterialData_.environmentCoefficient = 0.0f;
    cpuMaterialData_.hasTexture = !mesh.material.textureFilePath.empty();
    cpuMaterialData_.lightingMode = mesh.material.enableLighting ? 3 : 0;
    if (cpuMaterialData_.color.w <= 0.0f) {
        cpuMaterialData_.color.w = 1.0f;
    }

    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void ModelBatch::EnsureSharedTexture(const ObjMesh& mesh) {
    if (textureHandle_.IsValid()) {
        textureManager_->ReleaseTexture(textureHandle_);
    }
    if (!mesh.material.textureFilePath.empty()) {
        textureHandle_ = textureManager_->LoadTexture(mesh.material.textureFilePath);
    } else {
        textureHandle_ = ResourceHandle();
    }
}

float ModelBatch::GetBoundingSphereRadius() const {
    auto m = modelManager_ ? modelManager_->Resolve(modelHandle_) : nullptr;
    return m && m->cpuModel ? m->cpuModel->boundingSphere.radius : 0.0f;
}

void ModelBatch::Draw() {
    // If not initialized, attempt to init
    if (!isResourcesInitialized_) {
        auto m = modelManager_ ? modelManager_->Resolve(modelHandle_) : nullptr;
        if (m && m->status.load() == ManagedModel::LoadingStatus::Loaded && m->cpuModel) {
            InitializeResources();
        } else {
            return; // Not loaded yet
        }
    }

    if (!GetGpuMesh() || GetGpuMesh()->vertexCount == 0 || (instances_.empty() && instanceWorlds_.empty())) {
        return;
    }

    SyncBeforeDraw();

    RenderPackets::ModelBatchPacket p{};
    p.gpuMesh = GetGpuMesh();
    p.materialAddress = GetMaterialVAddress();

    if (useGPUCulling_) {
        p.useGPUCulling = true;
        p.instancingSrvHandleGPU = GetOutputInstancesSrvHandleGPU();
        p.indirectCommandBuffer = GetIndirectCommandBuffer();
        p.indirectCommandUploadBuffer = GetIndirectCommandUploadBuffer();
        p.indirectCommandUav = GetIndirectCommandUavHandleGPU();
        p.cullingDataAddress = GetCullingDataAddress();
        p.inputInstancesSrv = GetInstancingSrvHandleGPU();
        p.outputInstancesUav = GetOutputInstancesUavHandleGPU();
        p.outputInstancesBuffer = GetOutputInstanceBuffer();
        p.maxInstanceCount = GetMaxInstanceCount();
        p.instanceCount = GetMaxInstanceCount(); // CPU側での最大数をセットしておく(描画には使用されない)

        // UploadBufferへ描画引数の初期値を書き込む
        if (ID3D12Resource* uploadBuffer = GetIndirectCommandUploadBuffer()) {
            D3D12_DRAW_INDEXED_ARGUMENTS args{};
            args.IndexCountPerInstance =
                GetGpuMesh()->indexCount > 0 ? GetGpuMesh()->indexCount : GetGpuMesh()->vertexCount;
            args.InstanceCount = 0; // 初期値は0
            args.StartIndexLocation = 0;
            args.BaseVertexLocation = 0;
            args.StartInstanceLocation = 0;

            uint8_t* dst = nullptr;
            if (SUCCEEDED(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dst)))) {
                std::memcpy(dst, &args, sizeof(args));
                uploadBuffer->Unmap(0, nullptr);
            }
        }
    } else {
        p.useGPUCulling = false;
        p.instancingSrvHandleGPU = GetInstancingSrvHandleGPU();
        p.instanceCount = GetInstanceCount();
    }
    p.blendMode = GetBlendMode();
    p.depthWrite = GetDepthWrite();
    p.cullMode = GetCullMode();
    p.castShadows = GetCastShadows();
    p.customPSO = GetCustomPSO();
    p.customCBVAddress = GetCustomCBVAddress();

    drawManager_->SubmitModelBatch(p);
}

void ModelBatch::Draw(bool /*isUI*/) {
    // Overloaded draw for UI, if necessary
    Draw();
}
