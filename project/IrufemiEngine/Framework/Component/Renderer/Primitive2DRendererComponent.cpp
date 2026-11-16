#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Resource/Texture/TextureManager.h"

using namespace Irufemi;

Primitive2DRendererComponent::Primitive2DRendererComponent() {}
Primitive2DRendererComponent::~Primitive2DRendererComponent() {}

void Primitive2DRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void Primitive2DRendererComponent::OnAwake() {
    if (!primitive_) {
        primitive_ = std::make_unique<Primitive2DObject>();
        primitive_->Initialize(static_cast<Irufemi::Primitive2DType>(currentTypeIndex_));

        // 初期設定の適用
        if (!texturePath_.empty()) {
            primitive_->SetTexture(texturePath_);
        }
        primitive_->SetSize(size_);
        primitive_->SetPivot(pivot_);
        primitive_->SetColor(color_);
        primitive_->SetThickness(thickness_);
        primitive_->SetSubdivision(subdivision_);
        primitive_->SetTopMost(isTopMost_);
    }
}

void Primitive2DRendererComponent::OnSpawned() {
    SyncRenderState();
}

void Primitive2DRendererComponent::SyncRenderState() {
    if (GetTransform() && primitive_) {
        // Primitive2DObjectは主に画面空間での描画を想定しているため、
        // Transformのx, yをポジションとし、zをソート順等の奥行きとして渡す
        primitive_->SetPosition(GetTransform()->GetWorldPosition());

        // 2DなのでZ軸回転のみサポート
        primitive_->SetRotationZ(GetTransform()->GetWorldRotation().z);

        // TransformのScaleは、コンポーネントが保持するベースサイズ(size_)に対する乗数として適用
        Irufemi::Vector2 finalSize = {size_.x * GetTransform()->GetWorldScale().x,
                                      size_.y * GetTransform()->GetWorldScale().y};
        primitive_->SetSize(finalSize);
        primitive_->Update();
    }
}

void Primitive2DRendererComponent::Update() {
    SyncRenderState();
}

void Primitive2DRendererComponent::Draw() {
    if (primitive_) {
        primitive_->Draw();
    }
}

void Primitive2DRendererComponent::SetShape(Irufemi::Primitive2DType type) {
    currentTypeIndex_ = static_cast<int>(type);
    if (primitive_) {
        primitive_->SetShape(type);
    }
}

void Primitive2DRendererComponent::SetColor(const Irufemi::Vector4& color) {
    color_ = color;
    if (primitive_) {
        primitive_->SetColor(color_);
    }
}

void Primitive2DRendererComponent::SetTexture(const std::string& texturePath) {
    texturePath_ = texturePath;
    if (primitive_) {
        primitive_->SetTexture(texturePath_);
    }
}

void Primitive2DRendererComponent::SetPivot(const Irufemi::Vector2& pivot) {
    pivot_ = pivot;
    if (primitive_) {
        primitive_->SetPivot(pivot_);
    }
}

void Primitive2DRendererComponent::SetSize(const Irufemi::Vector2& size) {
    size_ = size;
    // Updateで最終的なサイズが再計算されるが、初期値として直接セットしておく
    if (primitive_ && GetTransform()) {
        Irufemi::Vector2 finalSize = {size_.x * GetTransform()->GetWorldScale().x,
                                      size_.y * GetTransform()->GetWorldScale().y};
        primitive_->SetSize(finalSize);
    } else if (primitive_) {
        primitive_->SetSize(size_);
    }
}

void Primitive2DRendererComponent::SetThickness(float thickness) {
    thickness_ = thickness;
    if (primitive_) {
        primitive_->SetThickness(thickness_);
    }
}

void Primitive2DRendererComponent::SetSubdivision(int subdivision) {
    subdivision_ = subdivision;
    if (primitive_) {
        primitive_->SetSubdivision(subdivision_);
    }
}

void Primitive2DRendererComponent::SetTopMost(bool isTopMost) {
    isTopMost_ = isTopMost;
    if (primitive_) {
        primitive_->SetTopMost(isTopMost_);
    }
}

nlohmann::json Primitive2DRendererComponent::Serialize() {
    nlohmann::json j;
    j["currentTypeIndex"] = currentTypeIndex_;
    j["texturePath"] = texturePath_;
    j["isTopMost"] = isTopMost_;
    j["size"] = nlohmann::json::array({size_.x, size_.y});
    j["pivot"] = nlohmann::json::array({pivot_.x, pivot_.y});
    j["color"] = nlohmann::json::array({color_.x, color_.y, color_.z, color_.w});
    j["thickness"] = thickness_;
    j["subdivision"] = subdivision_;
    return j;
}

void Primitive2DRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("currentTypeIndex")) {
        currentTypeIndex_ = j["currentTypeIndex"];
    }
    if (j.contains("texturePath")) {
        texturePath_ = j["texturePath"];
    }
    if (j.contains("isTopMost")) {
        isTopMost_ = j["isTopMost"];
    }

    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2) {
        size_.x = j["size"][0];
        size_.y = j["size"][1];
    }
    if (j.contains("pivot") && j["pivot"].is_array() && j["pivot"].size() == 2) {
        pivot_.x = j["pivot"][0];
        pivot_.y = j["pivot"][1];
    }
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color_.x = j["color"][0];
        color_.y = j["color"][1];
        color_.z = j["color"][2];
        color_.w = j["color"][3];
    }
    if (j.contains("thickness")) {
        thickness_ = j["thickness"];
    }
    if (j.contains("subdivision")) {
        subdivision_ = j["subdivision"];
    }

    // すでにインスタンス化されている場合はパラメータを適用
    if (primitive_) {
        primitive_->SetShape(static_cast<Irufemi::Primitive2DType>(currentTypeIndex_));
        if (!texturePath_.empty()) {
            primitive_->SetTexture(texturePath_);
        }
        primitive_->SetSize(size_);
        primitive_->SetPivot(pivot_);
        primitive_->SetColor(color_);
        primitive_->SetThickness(thickness_);
        primitive_->SetSubdivision(subdivision_);
        primitive_->SetTopMost(isTopMost_);
    }
}
