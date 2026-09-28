#include "Framework/Component/Renderer/MeshRendererComponent.h"

#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/OBB.h"
#include <cmath>

MeshRendererComponent::MeshRendererComponent() {}
MeshRendererComponent::~MeshRendererComponent() {}

void MeshRendererComponent::LoadModel(const std::string& filename) {
    modelName_ = filename;
    currentLoadedFilename_ = filename;
    if (!obj_) {
        obj_ = std::make_unique<StaticModelObject>();
    }
    obj_->Initialize(modelName_);
    obj_->SetEnableLightingToAllMeshes(enableLighting_);
}

IRenderable* MeshRendererComponent::GetRenderable() {
    return obj_.get();
}

void MeshRendererComponent::OnRegisterProperties() {
    RegisterProperty("Model File", &modelName_)
        .SetTooltip("The path to the GLTF or OBJ file to load")
        .OnChanged([this]() {
            if (modelName_ != currentLoadedFilename_) {
                LoadModel(modelName_);
            }
        });
    RegisterProperty("Visible", &isVisible_);
    RegisterProperty("Cast Shadows", &castShadows_);
    RegisterProperty("Enable Lighting", &enableLighting_).OnChanged([this]() {
        SetEnableLighting(enableLighting_);
    });
}

void MeshRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void MeshRendererComponent::OnAwake() {
    if (!obj_) {
        obj_ = std::make_unique<StaticModelObject>();
    }
    if (!modelName_.empty()) {
        LoadModel(modelName_);
    }
}

void MeshRendererComponent::OnSpawned() {
    SyncRenderState();
}

void MeshRendererComponent::SetEnableEffectMask(bool enable) {
    if (obj_) {
        obj_->SetEnableEffectMask(enable);
    }
}

void MeshRendererComponent::SetCustomEffectType(int32_t type) {
    if (obj_) {
        obj_->SetCustomEffectType(type);
    }
}

void MeshRendererComponent::SetCustomEffectParam(float param) {
    if (obj_) {
        obj_->SetCustomEffectParam(param);
    }
}

void MeshRendererComponent::SetEnableLighting(bool enable) {
    enableLighting_ = enable;
    if (obj_) {
        obj_->SetEnableLightingToAllMeshes(enableLighting_);
    }
}

void MeshRendererComponent::Update() {
    // TransformComponent があれば、その座標を StaticModelObject に渡す（同期）
    if (GetTransform() && obj_) {
        obj_->SetTranslate(GetTransform()->GetWorldPosition());
        obj_->SetRotate(GetTransform()->GetWorldRotation());
        obj_->SetScale(GetTransform()->GetWorldScale());
    }

    // StaticModelObject の行列計算などを実行
    if (obj_) {
        obj_->Update();
    }
}

void MeshRendererComponent::SyncRenderState() {
    if (GetTransform() && obj_) {
        obj_->SetTranslate(GetTransform()->GetWorldPosition());
        obj_->SetRotate(GetTransform()->GetWorldRotation());
        obj_->SetScale(GetTransform()->GetWorldScale());
    }
    if (obj_) {
        obj_->Update();
    }
}

void MeshRendererComponent::Draw() {
    if (!isVisible_ || !gameObject_ || !gameObject_->GetIsActive()) {
        return;
    }
    // RenderGraph に向けて描画パケットを積む
    if (obj_) {
        obj_->Draw();
    }
}

Irufemi::Sphere MeshRendererComponent::GetWorldSphere() const {
    Irufemi::Sphere result = {Irufemi::Vector3{0, 0, 0}, 1.0f}; // default
    if (GetTransform()) {
        result.center = GetTransform()->GetWorldPosition();
        // StaticModelObject の cpuModel があれば正確な半径を取得
        // ここでは便宜上スケールの最大値を半径として扱う（もしくは定数）
        Irufemi::Vector3 worldScale = GetTransform()->GetWorldScale();
        float maxScale = std::fmax(worldScale.x, std::fmax(worldScale.y, worldScale.z));
        result.radius = maxScale;
    }
    return result;
}

bool MeshRendererComponent::Raycast(const Irufemi::Ray& ray, float& outDistance) const {
    if (!obj_ || !GetTransform()) {
        return false;
    }
    auto cpuModel = obj_->GetCpuModel();
    if (!cpuModel) {
        return false;
    }

    // ローカルAABBから中心とサイズを取得
    Irufemi::Vector3 localCenter = (cpuModel->boundingBox.min + cpuModel->boundingBox.max) * 0.5f;
    Irufemi::Vector3 localHalfSize = (cpuModel->boundingBox.max - cpuModel->boundingBox.min) * 0.5f;

    Irufemi::OBB obb;
    // ワールド行列を用いて中心点を変換
    const Irufemi::Matrix4x4& wmat = GetTransform()->GetWorldMatrix();
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

nlohmann::json MeshRendererComponent::Serialize() {
    nlohmann::json j;
    j["modelName"] = modelName_; // 正規キーに統一

    // デフォルト値(true)と異なる場合のみ出力（プレハブオーバーライド最小化・疎シリアライズ）
    if (!isVisible_) {
        j["isVisible"] = false;
    }
    if (!castShadows_) {
        j["castShadows"] = false;
    }
    if (!enableLighting_) {
        j["enableLighting"] = false;
    }
    return j;
}

void MeshRendererComponent::Deserialize(const nlohmann::json& j) {
    // modelName または Model File の両対応（後方互換性維持）
    if (j.contains("modelName")) {
        modelName_ = j["modelName"].get<std::string>();
        LoadModel(modelName_);
    } else if (j.contains("Model File")) {
        modelName_ = j["Model File"].get<std::string>();
        LoadModel(modelName_);
    }

    if (j.contains("isVisible")) {
        isVisible_ = j["isVisible"].get<bool>();
    } else {
        isVisible_ = true;
    }
    if (j.contains("castShadows")) {
        castShadows_ = j["castShadows"].get<bool>();
    } else {
        castShadows_ = true;
    }
    if (j.contains("enableLighting")) {
        enableLighting_ = j["enableLighting"].get<bool>();
    } else if (j.contains("Enable Lighting")) {
        enableLighting_ = j["Enable Lighting"].get<bool>();
    } else {
        enableLighting_ = true;
    }
    if (obj_) {
        obj_->SetEnableLightingToAllMeshes(enableLighting_);
    }
}

std::shared_ptr<Component> MeshRendererComponent::Clone() {
    auto clone = std::make_shared<MeshRendererComponent>();
    clone->CopyPropertiesFrom(this);
    clone->castShadows_ = this->castShadows_;
    clone->isVisible_ = this->isVisible_;
    clone->enableLighting_ = this->enableLighting_;
    if (!this->modelName_.empty()) {
        clone->LoadModel(this->modelName_);
    }
    return clone;
}
