#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"

#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Object/PrimitiveManager.h"
#include "Core/Type/PrimitiveType.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/System/IrufemiEngine.h"
#include <cmath>

PrimitiveRendererComponent::PrimitiveRendererComponent() {}
PrimitiveRendererComponent::~PrimitiveRendererComponent() {}

void PrimitiveRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void PrimitiveRendererComponent::OnAwake() {
    if (!primitive_) {
        primitive_ = std::make_unique<Primitive3DObject>();
        if (auto engine = GetEngine()) {
            primitive_->SetTextureManagerInstance(engine->GetTextureManager());
            primitive_->SetDrawManagerInstance(engine->GetDrawManager());
            primitive_->SetEngineInstance(engine);
        }
        // 設定された形状（デフォルトはCube）で初期化
        primitive_->Initialize(static_cast<Irufemi::PrimitiveType>(currentTypeIndex_));
        primitive_->SetIsTransparent(isTransparent_);
        if (enableEffectMask_) {
            primitive_->SetEnableEffectMask(enableEffectMask_);
            primitive_->SetCustomEffectType(customEffectType_);
            primitive_->SetCustomEffectParam(customEffectParam_);
        }
    }
}

void PrimitiveRendererComponent::OnSpawned() {
    SyncRenderState();
}

void PrimitiveRendererComponent::SyncRenderState() {
    if (GetTransform() && primitive_) {
        primitive_->SetPosition(GetTransform()->GetWorldPosition());
        primitive_->SetRotate(GetTransform()->GetWorldRotation());
        primitive_->SetScale(GetTransform()->GetWorldScale());
        primitive_->Update();
    }
}

void PrimitiveRendererComponent::Update() {
    SyncRenderState();
}

void PrimitiveRendererComponent::Draw() {
    if (!gameObject_ || !gameObject_->GetIsActive()) {
        return;
    }
    if (primitive_) {
        primitive_->Draw();
    }
}

IRenderable* PrimitiveRendererComponent::GetRenderable() {
    return primitive_.get();
}

void PrimitiveRendererComponent::SetShape(Irufemi::PrimitiveType type) {
    currentTypeIndex_ = static_cast<int>(type);
    if (primitive_) {
        primitive_->SetShape(type);
    }
}

void PrimitiveRendererComponent::SetColor(const Irufemi::Vector4& color) {
    if (primitive_) {
        primitive_->SetColor(color);
    }
}

void PrimitiveRendererComponent::SetTexture(const std::string& texturePath) {
    if (primitive_) {
        primitive_->SetTexture(texturePath);
    }
}

void PrimitiveRendererComponent::SetEnableLighting(bool enable) {
    if (primitive_) {
        primitive_->GetMaterial().enableLighting = enable;
    }
}

void PrimitiveRendererComponent::SetLightingMode(int mode) {
    if (primitive_) {
        primitive_->GetMaterial().lightingMode = mode;
    }
}

void PrimitiveRendererComponent::SetMetallic(float metallic) {
    if (primitive_) {
        primitive_->GetMaterial().metallic = metallic;
    }
}

void PrimitiveRendererComponent::SetRoughness(float roughness) {
    if (primitive_) {
        primitive_->GetMaterial().roughness = roughness;
    }
}

void PrimitiveRendererComponent::SetAlphaReference(float alphaRef) {
    if (primitive_) {
        primitive_->GetMaterial().alphaReference = alphaRef;
    }
}

void PrimitiveRendererComponent::SetUseClampSampler(int32_t useClamp) {
    if (primitive_) {
        primitive_->GetMaterial().useClampSampler = useClamp;
    }
}

void PrimitiveRendererComponent::SetIsTransparent(bool isTransparent) {
    isTransparent_ = isTransparent;
    if (primitive_) {
        primitive_->SetIsTransparent(isTransparent);
    }
}

void PrimitiveRendererComponent::SetEnableEffectMask(bool enable) {
    enableEffectMask_ = enable;
    if (primitive_) {
        primitive_->SetEnableEffectMask(enable);
    }
}

void PrimitiveRendererComponent::SetCustomEffectType(int32_t type) {
    customEffectType_ = type;
    if (primitive_) {
        primitive_->SetCustomEffectType(type);
    }
}

void PrimitiveRendererComponent::SetCustomEffectParam(float param) {
    customEffectParam_ = param;
    if (primitive_) {
        primitive_->SetCustomEffectParam(param);
    }
}

void PrimitiveRendererComponent::RebuildMesh() {
    if (!primitive_) {
        return;
    }

    Irufemi::PrimitiveType type = static_cast<Irufemi::PrimitiveType>(currentTypeIndex_);
    PrimitiveData data;

    switch (type) {
    case Irufemi::PrimitiveType::Sphere:
    case Irufemi::PrimitiveType::IcoSphere:
        data = PrimitiveManager::CreateSphere(radius_, subdivisions_);
        break;
    case Irufemi::PrimitiveType::Cylinder:
        data = PrimitiveManager::CreateCylinder(bottomRadius_, topRadius_, height_, subdivisions_, hasTop_, hasBottom_);
        break;
    case Irufemi::PrimitiveType::Cone:
        data = PrimitiveManager::CreateCone(radius_, height_, subdivisions_);
        break;
    case Irufemi::PrimitiveType::Torus:
        data = PrimitiveManager::CreateTorus(torusMajorRadius_, torusMinorRadius_, torusMajorSegments_,
                                             torusMinorSegments_);
        break;
    case Irufemi::PrimitiveType::Circle:
        data = PrimitiveManager::CreateCircle(radius_, subdivisions_);
        break;
    case Irufemi::PrimitiveType::Cube:
    case Irufemi::PrimitiveType::Plane:
    case Irufemi::PrimitiveType::Triangle:
    case Irufemi::PrimitiveType::Tetra:
    default:
        // これらの基本図形は標準リソースに戻す
        primitive_->SetShape(type);
        return;
    }

    primitive_->ReinitializeMesh(data);
}

nlohmann::json PrimitiveRendererComponent::Serialize() {
    nlohmann::json j;
    j["currentTypeIndex"] = currentTypeIndex_;
    if (isTransparent_) {
        j["isTransparent"] = true;
    }

    // それぞれのデフォルト値と異なる場合のみ出力
    if (radius_ != 1.0f) {
        j["radius"] = radius_;
    }
    if (subdivisions_ != 16) {
        j["subdivisions"] = subdivisions_;
    }
    if (height_ != 1.0f) {
        j["height"] = height_;
    }
    if (topRadius_ != 1.0f) {
        j["topRadius"] = topRadius_;
    }
    if (bottomRadius_ != 1.0f) {
        j["bottomRadius"] = bottomRadius_;
    }
    if (hasTop_ != true) {
        j["hasTop"] = hasTop_;
    }
    if (hasBottom_ != true) {
        j["hasBottom"] = hasBottom_;
    }
    if (torusMajorRadius_ != 1.0f) {
        j["torusMajorRadius"] = torusMajorRadius_;
    }
    if (torusMinorRadius_ != 0.3f) {
        j["torusMinorRadius"] = torusMinorRadius_;
    }
    if (torusMajorSegments_ != 32) {
        j["torusMajorSegments"] = torusMajorSegments_;
    }
    if (torusMinorSegments_ != 16) {
        j["torusMinorSegments"] = torusMinorSegments_;
    }

    if (primitive_) {
        const auto& mat = primitive_->GetMaterial();
        nlohmann::json matJson;

        // テクスチャはデフォルトの uvChecker.png なら省略
        if (mat.texturePath != "resources/uvChecker.png") {
            matJson["texturePath"] = mat.texturePath;
        }

        if (mat.color.x != 1.0f || mat.color.y != 1.0f || mat.color.z != 1.0f || mat.color.w != 1.0f) {
            matJson["color"] = nlohmann::json::array({mat.color.x, mat.color.y, mat.color.z, mat.color.w});
        }
        if (mat.enableLighting != true) {
            matJson["enableLighting"] = mat.enableLighting;
        }
        if (mat.lightingMode != 3) {
            matJson["lightingMode"] = mat.lightingMode;
        }
        if (mat.metallic != 0.0f) {
            matJson["metallic"] = mat.metallic;
        }
        if (mat.roughness != 0.5f) {
            matJson["roughness"] = mat.roughness;
        }
        if (mat.alphaReference != 0.0f) {
            matJson["alphaReference"] = mat.alphaReference;
        }
        if (mat.useClampSampler != 0) {
            matJson["useClampSampler"] = mat.useClampSampler;
        }

        if (!matJson.empty()) {
            j["material"] = matJson;
        }
    }

    return j;
}

void PrimitiveRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("currentTypeIndex")) {
        currentTypeIndex_ = j["currentTypeIndex"];
    }
    if (j.contains("isTransparent")) {
        isTransparent_ = j["isTransparent"];
    }
    if (j.contains("radius")) {
        radius_ = j["radius"];
    }
    if (j.contains("subdivisions")) {
        subdivisions_ = j["subdivisions"];
    }
    if (j.contains("height")) {
        height_ = j["height"];
    }
    if (j.contains("topRadius")) {
        topRadius_ = j["topRadius"];
    }
    if (j.contains("bottomRadius")) {
        bottomRadius_ = j["bottomRadius"];
    }
    if (j.contains("hasTop")) {
        hasTop_ = j["hasTop"];
    }
    if (j.contains("hasBottom")) {
        hasBottom_ = j["hasBottom"];
    }
    if (j.contains("torusMajorRadius")) {
        torusMajorRadius_ = j["torusMajorRadius"];
    }
    if (j.contains("torusMinorRadius")) {
        torusMinorRadius_ = j["torusMinorRadius"];
    }
    if (j.contains("torusMajorSegments")) {
        torusMajorSegments_ = j["torusMajorSegments"];
    }
    if (j.contains("torusMinorSegments")) {
        torusMinorSegments_ = j["torusMinorSegments"];
    }

    if (!primitive_) {
        primitive_ = std::make_unique<Primitive3DObject>();
        primitive_->Initialize(static_cast<Irufemi::PrimitiveType>(currentTypeIndex_));
    }
    primitive_->SetIsTransparent(isTransparent_);

    // 形状を再構築
    Irufemi::PrimitiveType types[] = {Irufemi::PrimitiveType::Sphere, Irufemi::PrimitiveType::Plane,
                                      Irufemi::PrimitiveType::Cube,   Irufemi::PrimitiveType::Cylinder,
                                      Irufemi::PrimitiveType::Cone,   Irufemi::PrimitiveType::Torus};
    if (currentTypeIndex_ >= 0 && currentTypeIndex_ < 6) {
        SetShape(types[currentTypeIndex_]);
    }

    // カスタムパラメータでメッシュを生成し直す
    RebuildMesh();

    // マテリアル情報の復元
    if (j.contains("material") && primitive_) {
        const auto& matJson = j["material"];
        auto& mat = primitive_->GetMaterial();

        if (matJson.contains("texturePath")) {
            mat.texturePath = matJson["texturePath"];
        }
        if (matJson.contains("color") && matJson["color"].is_array() && matJson["color"].size() == 4) {
            mat.color.x = matJson["color"][0];
            mat.color.y = matJson["color"][1];
            mat.color.z = matJson["color"][2];
            mat.color.w = matJson["color"][3];
        }
        if (matJson.contains("enableLighting")) {
            mat.enableLighting = matJson["enableLighting"];
        }
        if (matJson.contains("lightingMode")) {
            mat.lightingMode = matJson["lightingMode"];
        }
        if (matJson.contains("metallic")) {
            mat.metallic = matJson["metallic"];
        }
        if (matJson.contains("roughness")) {
            mat.roughness = matJson["roughness"];
        }
        if (matJson.contains("alphaReference")) {
            mat.alphaReference = matJson["alphaReference"];
        }
        if (matJson.contains("useClampSampler")) {
            mat.useClampSampler = matJson["useClampSampler"];
        }

        primitive_->SetTexture(mat.texturePath);
    }
}

Irufemi::Sphere PrimitiveRendererComponent::GetWorldSphere() const {
    Irufemi::Sphere result = {Irufemi::Vector3{0, 0, 0}, 1.0f}; // default
    if (GetTransform()) {
        result.center = GetTransform()->GetWorldPosition();

        // 形状に応じて大まかな半径を決定
        float baseRadius = radius_;
        if (static_cast<Irufemi::PrimitiveType>(currentTypeIndex_) == Irufemi::PrimitiveType::Cube) {
            baseRadius = 1.0f; // Cubeは1x1x1なので対角線の半分は約0.866だが余裕を持つ
        }

        Irufemi::Vector3 worldScale = GetTransform()->GetWorldScale();
        float maxScale = std::fmax(worldScale.x, std::fmax(worldScale.y, worldScale.z));
        result.radius = baseRadius * maxScale * 2.0f; // 安全マージン
    }
    return result;
}

bool PrimitiveRendererComponent::Raycast(const Irufemi::Ray& ray, float& outDistance) const {
    if (!primitive_ || !GetTransform()) {
        return false;
    }

    // プリミティブ形状の基本AABB（一辺1のキューブ）
    Irufemi::Vector3 localHalfSize = {0.5f, 0.5f, 0.5f};

    Irufemi::OBB obb;
    obb.center = GetTransform()->GetWorldPosition();

    const Irufemi::Matrix4x4& wmat = GetTransform()->GetWorldMatrix();
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
