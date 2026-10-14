#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/Utility/JsonUtility.h"
#include <cmath>

SkinnedMeshRendererComponent::SkinnedMeshRendererComponent() {
    animatedMesh_ = std::make_unique<AnimatedMeshObject>();
}

SkinnedMeshRendererComponent::~SkinnedMeshRendererComponent() {}

void SkinnedMeshRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void SkinnedMeshRendererComponent::OnAwake() {
    if (!animatedMesh_) {
        animatedMesh_ = std::make_unique<AnimatedMeshObject>();
    }
    if (!modelFilename_.empty()) {
        LoadModel(modelFilename_);
    }
}

void SkinnedMeshRendererComponent::OnSpawned() {
    SyncRenderState();
}

void SkinnedMeshRendererComponent::LoadModel(const std::string& filename) {
    modelFilename_ = filename;
    currentLoadedFilename_ = filename;
    animatedMesh_->Initialize(modelFilename_);
}

void SkinnedMeshRendererComponent::Update() {
    if (auto transform = GetTransform()) {
        animatedMesh_->SetTranslate(transform->GetWorldPosition());
        animatedMesh_->SetRotate(transform->GetWorldRotation());
        animatedMesh_->SetScale(transform->GetWorldScale());
    }

    // Animatorからのポーズがあれば適用、なければバインドポーズ(nullptr)
    animatedMesh_->Update(poseOverride_);

    // 次フレームのためにリセット（毎フレーム指定される想定）
    poseOverride_ = nullptr;
}

void SkinnedMeshRendererComponent::SyncRenderState() {
    if (!animatedMesh_) {
        return;
    }
    if (auto transform = GetTransform()) {
        animatedMesh_->SetTranslate(transform->GetWorldPosition());
        animatedMesh_->SetRotate(transform->GetWorldRotation());
        animatedMesh_->SetScale(transform->GetWorldScale());
    }
    // バインドポーズで初期ポーズ計算とRegisterComputeTaskを先行実行
    animatedMesh_->Update(poseOverride_);
}

void SkinnedMeshRendererComponent::Draw() {
    if (!isVisible_ || !gameObject_ || !gameObject_->GetIsActive()) {
        return;
    }
    if (animatedMesh_) {
        animatedMesh_->SetMaterialOverrides(materialOverrides_.empty() ? nullptr : &materialOverrides_);
        animatedMesh_->Draw();
    }
}

bool SkinnedMeshRendererComponent::Raycast(const Irufemi::Ray& ray, float& outDistance) const {
    if (!animatedMesh_) {
        return false;
    }
    auto cpuModel = animatedMesh_->GetCpuModel();
    if (!cpuModel) {
        return false;
    }

    auto transform = GetTransform();
    if (!transform) {
        return false;
    }

    // ローカルAABBから中心とサイズを取得
    Irufemi::Vector3 localCenter = (cpuModel->boundingBox.min + cpuModel->boundingBox.max) * 0.5f;
    Irufemi::Vector3 localHalfSize = (cpuModel->boundingBox.max - cpuModel->boundingBox.min) * 0.5f;

    Irufemi::OBB obb;

    // スケルトンのルートノードのTransform（スケール等）を適用する
    Irufemi::Matrix4x4 rootTransform = Irufemi::Math::MakeIdentity4x4();
    auto skeletonData = animatedMesh_->GetSkeletonData();
    if (skeletonData && !skeletonData->joints.empty()) {
        rootTransform = skeletonData->joints[0].bindLocalMatrix;
    }

    // ワールド行列を用いて中心点を変換
    Irufemi::Matrix4x4 wmat = rootTransform * transform->GetWorldMatrix();
    obb.center = Irufemi::Math::Transform(localCenter, wmat);

    // ワールド行列の各軸ベクトルを抽出して正規化（回転）＆スケール適用
    Irufemi::Vector3 xAxis = {wmat.m[0][0], wmat.m[0][1], wmat.m[0][2]};
    Irufemi::Vector3 yAxis = {wmat.m[1][0], wmat.m[1][1], wmat.m[1][2]};
    Irufemi::Vector3 zAxis = {wmat.m[2][0], wmat.m[2][1], wmat.m[2][2]};

    float lenX = Irufemi::Math::Length(xAxis);
    float lenY = Irufemi::Math::Length(yAxis);
    float lenZ = Irufemi::Math::Length(zAxis);

    if (lenX > 0.0001f) {
        obb.orientations[0] = Irufemi::Math::Normalize(xAxis);
    } else {
        obb.orientations[0] = {1.0f, 0.0f, 0.0f};
    }

    if (lenY > 0.0001f) {
        obb.orientations[1] = Irufemi::Math::Normalize(yAxis);
    } else {
        obb.orientations[1] = {0.0f, 1.0f, 0.0f};
    }

    if (lenZ > 0.0001f) {
        obb.orientations[2] = Irufemi::Math::Normalize(zAxis);
    } else {
        obb.orientations[2] = {0.0f, 0.0f, 1.0f};
    }

    obb.size.x = localHalfSize.x * lenX;
    obb.size.y = localHalfSize.y * lenY;
    obb.size.z = localHalfSize.z * lenZ;

    return Irufemi::Collision::IsCollision(ray, obb, outDistance);
}

nlohmann::json SkinnedMeshRendererComponent::Serialize() {
    nlohmann::json j = nlohmann::json::object();
    j["Model File"] = modelFilename_;
    j["isVisible"] = isVisible_;

    if (!materialOverrides_.empty()) {
        nlohmann::json matOverridesJson = nlohmann::json::object();
        for (const auto& [meshIdx, mat] : materialOverrides_) {
            nlohmann::json m = nlohmann::json::object();
            m["color"] = Irufemi::JsonUtility::ToJson(mat.color);
            m["ambient"] = Irufemi::JsonUtility::ToJson(mat.ambient);
            m["specular"] = Irufemi::JsonUtility::ToJson(mat.specular);
            m["roughness"] = mat.roughness;
            m["metallic"] = mat.metallic;
            m["alpha"] = mat.alpha;
            m["enableLighting"] = mat.enableLighting;
            m["lightingMode"] = mat.lightingMode;
            m["useClampSampler"] = mat.useClampSampler;
            m["environmentCoefficient"] = mat.environmentCoefficient;
            m["alphaReference"] = mat.alphaReference;
            m["textureFilePath"] = mat.textureFilePath;
            m["normalMapFilePath"] = mat.normalMapFilePath;
            m["enableEffectMask"] = mat.enableEffectMask;
            m["customEffectType"] = mat.customEffectType;
            m["customEffectParam"] = mat.customEffectParam;
            matOverridesJson[std::to_string(meshIdx)] = m;
        }
        j["materialOverrides"] = matOverridesJson;
    }
    return j;
}

void SkinnedMeshRendererComponent::OnRegisterProperties() {
    RegisterProperty("Model File", &modelFilename_)
        .SetTooltip("The path to the GLTF or OBJ file to load")
        .OnChanged([this]() {
            if (modelFilename_ != currentLoadedFilename_) {
                LoadModel(modelFilename_);
            }
        });
}

void SkinnedMeshRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("Model File")) {
        modelFilename_ = j["Model File"].get<std::string>();
        if (!modelFilename_.empty() && modelFilename_ != currentLoadedFilename_) {
            LoadModel(modelFilename_);
        }
    }
    if (j.contains("isVisible")) {
        isVisible_ = j["isVisible"].get<bool>();
    }

    if (j.contains("materialOverrides") && j["materialOverrides"].is_object()) {
        materialOverrides_.clear();
        for (auto it = j["materialOverrides"].begin(); it != j["materialOverrides"].end(); ++it) {
            size_t meshIdx = std::stoull(it.key());
            const auto& m = it.value();
            ObjMaterial mat;
            if (m.contains("color")) {
                mat.color = Irufemi::JsonUtility::ToVector4(m["color"]);
            }
            if (m.contains("ambient")) {
                mat.ambient = Irufemi::JsonUtility::ToVector3(m["ambient"]);
            }
            if (m.contains("specular")) {
                mat.specular = Irufemi::JsonUtility::ToVector3(m["specular"]);
            }
            if (m.contains("roughness")) {
                mat.roughness = m["roughness"].get<float>();
            }
            if (m.contains("metallic")) {
                mat.metallic = m["metallic"].get<float>();
            }
            if (m.contains("alpha")) {
                mat.alpha = m["alpha"].get<float>();
            }
            if (m.contains("enableLighting")) {
                mat.enableLighting = m["enableLighting"].get<bool>();
            }
            if (m.contains("lightingMode")) {
                mat.lightingMode = m["lightingMode"].get<int32_t>();
            }
            if (m.contains("useClampSampler")) {
                mat.useClampSampler = m["useClampSampler"].get<int32_t>();
            }
            if (m.contains("environmentCoefficient")) {
                mat.environmentCoefficient = m["environmentCoefficient"].get<float>();
            }
            if (m.contains("alphaReference")) {
                mat.alphaReference = m["alphaReference"].get<float>();
            }
            if (m.contains("textureFilePath")) {
                mat.textureFilePath = m["textureFilePath"].get<std::string>();
            }
            if (m.contains("normalMapFilePath")) {
                mat.normalMapFilePath = m["normalMapFilePath"].get<std::string>();
            }
            if (m.contains("enableEffectMask")) {
                mat.enableEffectMask = m["enableEffectMask"].get<bool>();
            }
            if (m.contains("customEffectType")) {
                mat.customEffectType = m["customEffectType"].get<int32_t>();
            }
            if (m.contains("customEffectParam")) {
                mat.customEffectParam = m["customEffectParam"].get<float>();
            }

            SetMaterialOverride(meshIdx, mat);
        }
    }
}

std::shared_ptr<Component> SkinnedMeshRendererComponent::Clone() {
    auto clone = std::make_shared<SkinnedMeshRendererComponent>();
    clone->CopyPropertiesFrom(this);
    clone->isVisible_ = this->isVisible_;
    clone->materialOverrides_ = this->materialOverrides_;
    if (!this->modelFilename_.empty()) {
        clone->LoadModel(this->modelFilename_);
    }
    return clone;
}

void SkinnedMeshRendererComponent::SetEnableEffectMask(bool enable) {
    if (animatedMesh_) {
        animatedMesh_->SetEnableEffectMask(enable);
    }
}

void SkinnedMeshRendererComponent::SetCustomEffectType(int32_t type) {
    if (animatedMesh_) {
        animatedMesh_->SetCustomEffectType(type);
    }
}

void SkinnedMeshRendererComponent::SetCustomEffectParam(float param) {
    if (animatedMesh_) {
        animatedMesh_->SetCustomEffectParam(param);
    }
}
