#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Renderer/Object/Batch/ModelBatch.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/System/IrufemiEngine.h"
#include <cmath>

ModelBatchRendererComponent::ModelBatchRendererComponent() {}
ModelBatchRendererComponent::~ModelBatchRendererComponent() {}

void ModelBatchRendererComponent::LoadModel(const std::string& filename) {
    modelName_ = filename;
    if (batch_ && !modelName_.empty()) {
        batch_->Initialize(modelName_);
        batch_->SetColor(color_);
    }
}

void ModelBatchRendererComponent::Initialize() {
    OnAwake();
}

void ModelBatchRendererComponent::OnAwake() {
    if (!batch_) {
        batch_ = std::make_unique<ModelBatch>();
    }
    if (!modelName_.empty()) {
        batch_->Initialize(modelName_);
    }

    batch_->SetColor(color_);
    // キャッシュされたGPUカリング設定を反映
    batch_->SetUseGPUCulling(useGPUCulling_);
}

void ModelBatchRendererComponent::Update() {
    // ModelBatchは個別ではなく、複数描画を管理するため
    // Transform更新はここでは行わない、または親の座標自体も一つのInstanceとして扱うかになります。
    // 基本的には外部から AddInstance() を呼ばれることを想定するため、ここでは何もしません。
}

void ModelBatchRendererComponent::Draw() {
    if (!gameObject_ || !gameObject_->GetIsActive()) {
        return;
    }
    if (batch_) {
        batch_->Draw();
        if (autoClearEveryFrame_) {
            batch_->ClearInstances();
        }
    }
}

IRenderable* ModelBatchRendererComponent::GetRenderable() {
    return batch_.get();
}

Irufemi::Sphere ModelBatchRendererComponent::GetWorldSphere() const {
    Irufemi::Sphere result = {Irufemi::Vector3{0, 0, 0}, 1.0f}; // default
    if (GetTransform()) {
        result.center = GetTransform()->GetWorldPosition();
        Irufemi::Vector3 worldScale = GetTransform()->GetWorldScale();
        float maxScale = std::fmax(worldScale.x, std::fmax(worldScale.y, worldScale.z));
        result.radius = maxScale;
    }
    return result;
}

bool ModelBatchRendererComponent::Raycast(const Irufemi::Ray& ray, float& outDistance) const {
    if (!batch_ || !GetTransform()) {
        return false;
    }

    // プレイモード中は、バッチ親オブジェクトのダミー当たり判定による誤選択を防止する
    if (auto engine = GetEngine()) {
        if (engine->IsPlayMode()) {
            return false;
        }
    }

    // バッチ全体のAABBや個々のインスタンスとのRaycastは重いため、
    // エディタ等での簡易選択用として親のTransformにのみ当たり判定を付ける
    Irufemi::Vector3 localHalfSize = {0.5f, 0.5f, 0.5f};

    Irufemi::OBB obb;
    const Irufemi::Matrix4x4& wmat = GetTransform()->GetWorldMatrix();
    obb.center = GetTransform()->GetWorldPosition();

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

nlohmann::json ModelBatchRendererComponent::Serialize() {
    nlohmann::json j;
    j["modelName"] = modelName_;
    j["autoClearEveryFrame"] = autoClearEveryFrame_;
    j["color"] = {color_.x, color_.y, color_.z, color_.w};
    return j;
}

void ModelBatchRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("modelName")) {
        std::string modelName = j["modelName"];
        LoadModel(modelName);
    }
    if (j.contains("autoClearEveryFrame")) {
        autoClearEveryFrame_ = j["autoClearEveryFrame"];
    }
    if (j.contains("color")) {
        auto& c = j["color"];
        if (c.is_array() && c.size() >= 4) {
            SetColor({c[0], c[1], c[2], c[3]});
        }
    }
}

void ModelBatchRendererComponent::OnRegisterProperties() {
    RegisterProperty("Color", &color_).SetTooltip("Base material color multiplier (RGBA)");
}

void ModelBatchRendererComponent::SetColor(const Irufemi::Vector4& color) {
    color_ = color;
    if (batch_) {
        batch_->SetColor(color_);
    }
}

void ModelBatchRendererComponent::AddInstance(const Irufemi::Transform& t, int32_t effectType, float effectParam,
                                              bool enableMask) {
    if (batch_) {
        batch_->AddInstance(t, effectType, effectParam, enableMask);
    }
}

void ModelBatchRendererComponent::AddInstanceWorld(const Irufemi::Matrix4x4& world, int32_t effectType,
                                                   float effectParam, bool enableMask) {
    if (batch_) {
        batch_->AddInstanceWorld(world, color_, effectType, effectParam, enableMask);
    }
}

void ModelBatchRendererComponent::AddInstanceWorld(const Irufemi::Matrix4x4& world, const Irufemi::Vector4& color,
                                                   int32_t effectType, float effectParam, bool enableMask) {
    if (batch_) {
        batch_->AddInstanceWorld(world, color, effectType, effectParam, enableMask);
    }
}

void ModelBatchRendererComponent::ClearInstances() {
    if (batch_) {
        batch_->ClearInstances();
    }
}

void ModelBatchRendererComponent::SetUseGPUCulling(bool use) {
    useGPUCulling_ = use;
    if (batch_) {
        batch_->SetUseGPUCulling(use);
    }
}
