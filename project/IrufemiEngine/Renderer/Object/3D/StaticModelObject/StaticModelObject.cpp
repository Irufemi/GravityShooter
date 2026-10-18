#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h" // リネーム済み
#include "Core/Utility/ErrorUtility.h"
#include <filesystem>
#include <algorithm>
#include <Windows.h>
#include "Core/Math/Math.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"
#include "Framework/UI/DebugUI.h"
#include "Resource/Model/ModelManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Camera/CameraManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Renderer/Compute/IComputeTask.h"

// 静的メンバ定義

StaticModelObject::~StaticModelObject() {}

void StaticModelObject::CalculateNodeTransforms(const Node& node, const Irufemi::Matrix4x4& parentMatrix) {
    Irufemi::Matrix4x4 globalMatrix = node.localMatrix * parentMatrix;
    nodeGlobalTransforms_[node.name] = globalMatrix;

    for (const auto& child : node.children) {
        CalculateNodeTransforms(child, globalMatrix);
    }
}

void StaticModelObject::Initialize(const std::string& filename) {

    IRUFEMI_ASSERT(engine_ && "StaticModelObject::Initialize: Engine is not set.");
    // 描画中のリソース破棄（Use-After-Free）を防ぐため、次フレームのUpdateで切り替えるフラグと変数を設定
    nextModelHandle_ = engine_->GetObjModelManager()->LoadModel(filename);
    isModelChanged_ = true;
    isResourceInitialized_ = false;
}

void StaticModelObject::InitializeResources() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel) {
        return;
    }

    isResourceInitialized_ = true;

    if (transformCbIndex_ == static_cast<uint32_t>(-1)) {
        if (engine_) {
            transformCbIndex_ = engine_->GetTransformBufferManager()->Allocate();
        }
    }

    // インスタンス固有の各メッシュ用リソースを生成
    meshResources_.clear();
    for (size_t i = 0; i < m->gpuMeshes.size(); ++i) {
        auto res = std::make_unique<Object3DResource>();

        // 外部の変換行列リソースを借用（削除：各リソースに固有のTransformを持たせるため）
        // res->SetExternalTransformCbIndex(&transformCbIndex_);

        // メッシュ固有の View を設定
        const auto& gpuMesh = m->gpuMeshes[i];
        res->SetVertexBufferView(gpuMesh->vertexBufferView);
        res->SetIndexBufferView(gpuMesh->indexBufferView);
        res->SetIndexCount(gpuMesh->indexCount);

        // マテリアルリソース等の生成
        res->CreateResource();

        // 初期テクスチャハンドルを共有データからコピー
        const auto& gpuMaterial = (i < m->gpuMaterials.size()) ? m->gpuMaterials[i] : nullptr;
        if (gpuMaterial) {
            res->SetTextureHandle(gpuMaterial->textureHandle);
        }

        meshResources_.push_back(std::move(res));
    }

    // Skeleton と SkinCluster の初期化 (スキンがある場合)
    if (m && m->cpuModel && !m->cpuModel->skinClusterData.empty()) {
        skeletonData_ = AnimationManager::CreateSkeletonData(m->cpuModel->rootNode);
        skeletonPose_ = AnimationManager::CreateSkeletonPose(&skeletonData_);
        skinCluster_ = engine_->GetAnimationManager()->CreateSkinCluster(skeletonData_, *m->cpuModel);

        // 静的モデルなのでバインドポーズのままスケルトンを1度更新しておく
        AnimationManager::SkeletonUpdate(skeletonPose_);
    }

    // 初回Updateを呼んでおく
    Update();
}

void StaticModelObject::Update() {
    // 描画キューにポインタが積まれた後にリソースが破棄されないよう、Updateのタイミングでモデルを切り替える
    if (isModelChanged_) {
        if (nextModelHandle_.IsValid()) {
            if (auto nextM = engine_->GetObjModelManager()->Resolve(nextModelHandle_)) {
                auto status = nextM->status.load();
                if (status == ManagedModel::LoadingStatus::Loaded && nextM->cpuModel) {
                    modelHandle_ = nextModelHandle_;
                    isModelChanged_ = false;
                    nextModelHandle_ = ResourceHandle();
                    InitializeResources();
                } else if (status == ManagedModel::LoadingStatus::Failed) {
                    isModelChanged_ = false;
                    nextModelHandle_ = ResourceHandle();
                }
            }
        } else {
            isModelChanged_ = false;
        }
    }

    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !engine_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // 非同期ロードが終わっていればメッシュを構築する (遅延初期化)
    if (m->status.load() == ManagedModel::LoadingStatus::Loaded && !isResourceInitialized_) {
        InitializeResources();
    }

    // まだリソースが準備できていない場合はスキップ
    if (!isResourceInitialized_) {
        return;
    }

    // オブジェクト全体のワールド行列を計算
    transformationMatrix_.world =
        Irufemi::Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Irufemi::Matrix4x4 objectWorld = transformationMatrix_.world;

    // rootNodeの行列を適用(モデルデータに階層情報があれば)
    if (m->cpuModel) {
        if (!m->cpuModel->skinClusterData.empty()) {
            // スキニングモデルの場合、rootNodeの行列はSkeleton内で処理されるため
            // World行列には適用しない（二重適用を防ぐ）
        } else {
            transformationMatrix_.world = m->cpuModel->rootNode.localMatrix * objectWorld;
        }

        // 階層全体のトランスフォームを計算
        nodeGlobalTransforms_.clear();
        CalculateNodeTransforms(m->cpuModel->rootNode, Irufemi::Math::MakeIdentity4x4());
    }

    // 各メッシュごとにノード階層を考慮したワールド行列を割り当てる
    for (size_t i = 0; i < meshResources_.size(); ++i) {
        auto& res = meshResources_[i];
        Irufemi::Matrix4x4 meshWorld = objectWorld; // 基準はRootを含まないもの

        if (m->cpuModel && m->cpuModel->skinClusterData.empty()) {
            // 非スキニング時のみ、ノード階層を考慮する
            const std::string& nodeName = m->cpuModel->meshes[i].nodeName;
            auto it = nodeGlobalTransforms_.find(nodeName);
            if (it != nodeGlobalTransforms_.end()) {
                meshWorld = it->second * objectWorld;
            } else {
                meshWorld = transformationMatrix_.world; // fallback
            }
        } else {
            meshWorld = transformationMatrix_.world; // スキニング時は fallback
        }

        res->GetTransformationMatrix().world = meshWorld;

        Irufemi::Matrix4x4 worldForNormal = meshWorld;
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;
        res->GetTransformationMatrix().WorldInverseTranspose =
            Irufemi::Math::Transpose(Irufemi::Math::Inverse(worldForNormal));

        res->MarkAsDirty();
    }

    // マテリアル情報をGPUへ転送
    UpdateMaterials();

    isDirty_ = false;
    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetPerspectiveFovMatrix();

    // スキニングモデルの場合は Compute Shader の実行を予約する
    if (m->cpuModel && !m->cpuModel->skinClusterData.empty() && engine_ && engine_->GetDrawManager()) {
        engine_->GetDrawManager()->RegisterComputeTask(this);
    }
}

void StaticModelObject::SyncBeforeDraw() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();

    if (CheckAndClearDirty(frameIndex)) {
        // 変換行列の更新 (全メッシュで共有のバッファ)
        if (engine_) {
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine_->GetTransformBufferManager()->Update(transformCbIndex_, transformationMatrix_, frameIndex);
            }
        }
    }

    // 各メッシュのマテリアル等の更新
    for (auto& res : meshResources_) {
        res->SyncBeforeDraw();
    }

    // --- SkinCluster のマルチバッファ同期 ---
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (m && m->cpuModel && !m->cpuModel->skinClusterData.empty()) {
        AnimationManager::SkinClusterUpdate(skinCluster_, skeletonPose_, frameIndex);
    }
}

#include "Physics/Collision/Collision.h"
#include "Core/Shape/Sphere.h"

void StaticModelObject::Draw() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !engine_ || !engine_->GetDrawManager()) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // カメラの行列が変更されたか、オブジェクト自体が変更されたかチェック
    bool cameraChanged =
        (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Irufemi::Matrix4x4)) != 0 ||
         std::memcmp(&lastProjectionMatrix_, &activeCam->GetPerspectiveFovMatrix(), sizeof(Irufemi::Matrix4x4)) != 0);

    if (isDirty_ || cameraChanged) {
        Update();
    }

    // --- 描画直前のバッファ同期 ---
    SyncBeforeDraw();

    // 視錐台カリング
    if (isCullingEnabled_ && m->cpuModel) {
        const Irufemi::Sphere& modelSphere = m->cpuModel->boundingSphere;

        // ワールド空間の境界球を計算
        Irufemi::Sphere worldSphere;
        worldSphere.center = Irufemi::Math::Transform(modelSphere.center, transformationMatrix_.world);

        // スケールの最大値を適用して半径を変換
        float maxScale = (std::max)({transform_.scale.x, transform_.scale.y, transform_.scale.z});
        worldSphere.radius = modelSphere.radius * maxScale * 1.1f; // 10% マージン

        // 判定
        if (!Irufemi::Collision::IsCollision(activeCam->GetFrustum(), worldSphere)) {
            return; // 描画スキップ
        }
    }

    // モデル内の全メッシュを描画
    for (auto& res : meshResources_) {
        if (m->cpuModel && !m->cpuModel->skinClusterData.empty()) {
            engine_->GetDrawManager()->SubmitStandard3D(
                res.get(), &skinCluster_.skinnedVertexBufferView[lastSkinnedFrameIndex_], castShadows_);
        } else {
            engine_->GetDrawManager()->SubmitStandard3D(res.get(), nullptr, castShadows_);
        }
    }
}

void StaticModelObject::DrawOutlineMask() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !engine_ || !engine_->GetDrawManager() || meshResources_.empty()) {
        return;
    }
    for (auto& res : meshResources_) {
        engine_->GetDrawManager()->SubmitOutlineMask(res.get(), nullptr);
    }
}
void StaticModelObject::DispatchCompute() {
    auto m = engine_ ? engine_->GetObjModelManager()->Resolve(modelHandle_) : nullptr;
    if (!m || !m->cpuModel || m->cpuModel->skinClusterData.empty() || !engine_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    if (isCullingEnabled_) {
        float maxScale = (std::max)({transform_.scale.x, transform_.scale.y, transform_.scale.z});
        Irufemi::Sphere boundingSphere;
        boundingSphere.center = transform_.translate;
        boundingSphere.radius = m->cpuModel->boundingSphere.radius * maxScale * 1.5f;
        if (!Irufemi::Collision::IsCollision(activeCam->GetFrustum(), boundingSphere)) {
            return; // 視錐台カリングされている場合はComputeもスキップ
        }
    }

    engine_->GetDrawManager()->DispatchSkinning(skinCluster_, m, skinCluster_.mappedSkinningInformation->numVertices);
    lastSkinnedFrameIndex_ = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
}

void StaticModelObject::Debug([[maybe_unused]] const char* objName) {
#if defined USE_IMGUI
    std::string name = std::string("Obj: ") + objName;
    ImGui::Begin(name.c_str());
    DebugTab();
    ImGui::End();
#endif
}

void StaticModelObject::DebugTab() {
#if defined USE_IMGUI
    if (engine_) {
        auto ui_ = engine_->GetDebugUI();
        ImGui::Checkbox("Frustum Culling", &isCullingEnabled_);
        ui_->DebugTransform(transform_);
        ImGui::ColorEdit4("Color", &color_.x); // インスタンスカラーを編集
        ui_->DebugMaterialOverrides(&environmentCoefficient_, &lightingModeOverride_, &useClampSamplerOverride_,
                                    &enableLightingOverride_, "##OcOverrides");

        // ImGuiでマテリアルを編集
        auto m = engine_->GetObjModelManager()->Resolve(modelHandle_);
        if (m && m->cpuModel) {
            for (size_t i = 0; i < m->cpuModel->meshes.size(); ++i) {
                std::string materialLabel = "Mesh " + std::to_string(i) + " Material";
                if (ImGui::TreeNode(materialLabel.c_str())) {
                    ObjMaterial* mat = GetMaterial(i);
                    if (mat) {
                        // unique_id を渡してコントロールIDの衝突を避ける
                        std::string unique_id = "##" + std::to_string(i);
                        ui_->DebugObjMaterial(mat, unique_id.c_str());

                        // ※テクスチャやアセットパスの設定は MeshRendererComponent / InspectorPanel で行われます
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
#endif
}
