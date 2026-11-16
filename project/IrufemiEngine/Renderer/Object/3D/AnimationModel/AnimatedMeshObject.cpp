#include "Renderer/Object/3D/AnimationModel/AnimatedMeshObject.h"
#include "Core/Utility/ErrorUtility.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/Math.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/DrawManager.h"
#include "Framework/UI/DebugUI.h"
#include "Core/Math/Geometry/Frustum.h"
#include "Physics/Collision/Collision.h"
#include "Core/Shape/Sphere.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Renderer/Object/Batch/PrimitiveBatch.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/CameraManager.h"
#include <cmath>
#include <cassert>

namespace Irufemi {

AnimatedMeshObject::AnimatedMeshObject() {}
AnimatedMeshObject::~AnimatedMeshObject() {}

void AnimatedMeshObject::Initialize(const std::string& filename) {
    filename_ = filename;

    IRUFEMI_ASSERT(engine_ && "AnimatedMeshObject::Initialize: ModelManager is not set.");
    modelHandle_ = engine_->GetObjModelManager()->LoadModel(filename);

    meshResources_.clear(); // モデル切り替え時に古いリソースを確実に破棄する

    if (auto m = engine_->GetObjModelManager()->Resolve(modelHandle_)) {
        auto status = m->status.load();
        if (status == ManagedModel::LoadingStatus::Loaded && m->cpuModel) {
            InitializeResources();
        }
    }
}

void AnimatedMeshObject::InitializeResources() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel) {
        return;
    }

    if (transformCbIndex_ == static_cast<uint32_t>(-1)) {
        transformCbIndex_ = engine_->GetTransformBufferManager()->Allocate();
    }

    meshResources_.clear();
    for (size_t i = 0; i < m->gpuMeshes.size(); ++i) {
        auto res = std::make_unique<Object3DResource>();
        res->SetExternalTransformCbIndex(&transformCbIndex_);

        const auto& gpuMesh = m->gpuMeshes[i];
        res->SetVertexBufferView(gpuMesh->vertexBufferView);
        res->SetIndexBufferView(gpuMesh->indexBufferView);
        res->SetIndexCount(gpuMesh->indexCount);

        res->CreateResource();

        const auto& gpuMaterial = (i < m->gpuMaterials.size()) ? m->gpuMaterials[i] : nullptr;
        if (gpuMaterial) {
            res->SetTextureHandle(gpuMaterial->textureHandle);
        }
        meshResources_.push_back(std::move(res));
    }

    if (m && m->cpuModel) {
        skeletonData_ = AnimationManager::CreateSkeletonData(m->cpuModel->rootNode);
        internalPose_ = AnimationManager::CreateSkeletonPose(&skeletonData_);
        currentPose_ = &internalPose_;

        if (!m->cpuModel->skinClusterData.empty()) {
            skinCluster_ = engine_->GetAnimationManager()->CreateSkinCluster(skeletonData_, *m->cpuModel);
        }
    }

    Update();
}

void AnimatedMeshObject::Update(const SkeletonPose* externalPose) {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !engine_) {
        return;
    }

    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    if (m->status.load() != ManagedModel::LoadingStatus::Loaded) {
        return;
    }

    if (meshResources_.empty()) {
        InitializeResources();
    }
    if (meshResources_.empty()) {
        return;
    }

    // ポーズの差し替え
    if (externalPose) {
        currentPose_ = externalPose;
    } else {
        currentPose_ = &internalPose_;
    }

    worldMatrix_ = Irufemi::Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    if (!m->cpuModel->skinClusterData.empty()) {
        transformationMatrix_.world = worldMatrix_;
        Irufemi::Matrix4x4 worldForNormal = transformationMatrix_.world;
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;
        transformationMatrix_.WorldInverseTranspose = Irufemi::Math::Transpose(Irufemi::Math::Inverse(worldForNormal));
    } else {
        transformationMatrix_.world = localMatrix_ * worldMatrix_;
        Irufemi::Matrix4x4 worldForNormal = transformationMatrix_.world;
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;
        transformationMatrix_.WorldInverseTranspose = Irufemi::Math::Transpose(Irufemi::Math::Inverse(worldForNormal));
    }

    UpdateMaterials();
    isDirty_ = false;
    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetPerspectiveFovMatrix();

    if (!m->cpuModel->skinClusterData.empty() && engine_ && engine_->GetDrawManager()) {
        engine_->GetDrawManager()->RegisterComputeTask(this);
    }
}

void AnimatedMeshObject::SyncBeforeDraw() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();

    if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
        engine_->GetTransformBufferManager()->Update(transformCbIndex_, transformationMatrix_, frameIndex);
    }

    for (auto& res : meshResources_) {
        res->SyncBeforeDraw();
    }

    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (m && m->cpuModel && !m->cpuModel->skinClusterData.empty() && currentPose_) {
        AnimationManager::SkinClusterUpdate(skinCluster_, *currentPose_, frameIndex);
    }
}

void AnimatedMeshObject::DispatchCompute() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel || m->cpuModel->skinClusterData.empty() || !engine_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    isCulled_ = false;
    if (isCullingEnabled_) {
        float maxScale = (std::max)({transform_.scale.x, transform_.scale.y, transform_.scale.z});
        Irufemi::Sphere boundingSphere;
        boundingSphere.center = transform_.translate;
        boundingSphere.radius = m->cpuModel->boundingSphere.radius * maxScale * 1.5f;
        if (!Irufemi::Collision::IsCollision(activeCam->GetFrustum(), boundingSphere)) {
            isCulled_ = true;
            return;
        }
    }

    engine_->GetDrawManager()->DispatchSkinning(skinCluster_, m, skinCluster_.mappedSkinningInformation->numVertices);
    lastSkinnedFrameIndex_ = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
}

void AnimatedMeshObject::Draw() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel || !engine_ || meshResources_.empty()) {
        return;
    }
    if (isCulled_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    if (isCullingEnabled_ && m->cpuModel->skinClusterData.empty()) {
        float maxScale = (std::max)({transform_.scale.x, transform_.scale.y, transform_.scale.z});
        Irufemi::Sphere boundingSphere;
        boundingSphere.center = transform_.translate;
        boundingSphere.radius = m->cpuModel->boundingSphere.radius * maxScale * 1.5f;
        if (!Irufemi::Collision::IsCollision(activeCam->GetFrustum(), boundingSphere)) {
            return;
        }
    }

    bool cameraChanged =
        (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Irufemi::Matrix4x4)) != 0 ||
         std::memcmp(&lastProjectionMatrix_, &activeCam->GetPerspectiveFovMatrix(), sizeof(Irufemi::Matrix4x4)) != 0);

    if (isDirtyBuffer_[BaseResource::GetDirectXCommon()->GetFrameIndex()] || cameraChanged) {
        Update(currentPose_);
    }

    SyncBeforeDraw();

    uint32_t vertexOffset = 0;
    drawVbvs_.resize(meshResources_.size());
    for (size_t i = 0; i < meshResources_.size(); ++i) {
        auto& res = meshResources_[i];
        if (!m->cpuModel->skinClusterData.empty()) {
            drawVbvs_[i] = skinCluster_.skinnedVertexBufferView[lastSkinnedFrameIndex_];
            drawVbvs_[i].BufferLocation += vertexOffset * sizeof(VertexData);
            drawVbvs_[i].SizeInBytes = static_cast<UINT>(m->cpuModel->meshes[i].vertices.size() * sizeof(VertexData));

            engine_->GetDrawManager()->SubmitStandard3D(
                res.get(), &drawVbvs_[i], castShadows_,
                skinCluster_.skinnedVertexResource[lastSkinnedFrameIndex_].Get());

            vertexOffset += static_cast<uint32_t>(m->cpuModel->meshes[i].vertices.size());
        } else {
            engine_->GetDrawManager()->SubmitStandard3D(res.get(), nullptr, castShadows_);
        }
    }
}

void AnimatedMeshObject::DrawOutlineMask() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !engine_ || !engine_->GetDrawManager() || meshResources_.empty() || isCulled_) {
        return;
    }
    uint32_t vertexOffset = 0;
    outlineVbvs_.resize(meshResources_.size());
    for (size_t i = 0; i < meshResources_.size(); ++i) {
        auto& res = meshResources_[i];
        if (!m->cpuModel->skinClusterData.empty()) {
            outlineVbvs_[i] = skinCluster_.skinnedVertexBufferView[lastSkinnedFrameIndex_];
            outlineVbvs_[i].BufferLocation += vertexOffset * sizeof(VertexData);
            outlineVbvs_[i].SizeInBytes =
                static_cast<UINT>(m->cpuModel->meshes[i].vertices.size() * sizeof(VertexData));

            engine_->GetDrawManager()->SubmitOutlineMask(
                res.get(), &outlineVbvs_[i], skinCluster_.skinnedVertexResource[lastSkinnedFrameIndex_].Get());

            vertexOffset += static_cast<uint32_t>(m->cpuModel->meshes[i].vertices.size());
        } else {
            engine_->GetDrawManager()->SubmitOutlineMask(res.get(), nullptr, nullptr);
        }
    }
}

void AnimatedMeshObject::Debug([[maybe_unused]] const char* objName) {
#if defined USE_IMGUI
    // ポインタアドレスをIDとして含めることで、同じモデルを使用する複数オブジェクトでImGuiのID衝突（およびそれに伴う頂点バッファ崩壊クラッシュ）を防ぐ
    std::string name =
        std::string("AnimMesh: ") + objName + "###AnimMesh_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    ImGui::Begin(name.c_str());
    if (engine_) {
        auto* ui_ = engine_->GetDebugUI();
        ui_->DebugTransform(transform_);
        ImGui::Checkbox("Frustum Culling", &isCullingEnabled_);
        ImGui::ColorEdit4("Color", &color_.x);
        ui_->DebugMaterialOverrides(&environmentCoefficient_, &lightingModeOverride_, &useClampSamplerOverride_,
                                    &enableLightingOverride_, "##AmOverrides");
    }
    ImGui::End();
#endif
}

const SkeletonData* AnimatedMeshObject::GetSkeletonData() const {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel || m->cpuModel->skinClusterData.empty()) {
        return nullptr;
    }
    return &skeletonData_;
}

SkeletonPose* AnimatedMeshObject::GetInternalSkeletonPose() {
    return &internalPose_;
}

} // namespace Irufemi
