#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/System/IrufemiEngine.h"

SpriteRendererComponent::SpriteRendererComponent() {}
SpriteRendererComponent::~SpriteRendererComponent() {}

void SpriteRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void SpriteRendererComponent::OnAwake() {
    if (!sprite_) {
        sprite_ = std::make_unique<Sprite>();
        if (auto engine = GetEngine()) {
            sprite_->SetTextureManagerInstance(engine->GetTextureManager());
            sprite_->SetDrawManagerInstance(engine->GetDrawManager());
            sprite_->SetCameraManagerInstance(engine->GetCameraManager());
        }
        sprite_->Initialize(texturePath_);

        // 初期設定
        sprite_->SetAnchor(anchor_.x, anchor_.y);
        sprite_->SetFlip(isFlipX_, isFlipY_);
        sprite_->SetTopMost(isTopMost_);
        sprite_->SetColor(color_);
    }

    // テクスチャサイズを取得して初期サイズに設定（Deserializeで既にサイズが設定されていない場合のみ）
    if (size_.x == 640.0f && size_.y == 360.0f) { // デフォルト値の場合は上書き
        size_.x = sprite_->GetSize().x;
        size_.y = sprite_->GetSize().y;
    } else if (size_.x == 0.0f && size_.y == 0.0f) {
        size_.x = sprite_->GetSize().x;
        size_.y = sprite_->GetSize().y;
    }
}

void SpriteRendererComponent::OnSpawned() {
    SyncRenderState();
}

void SpriteRendererComponent::SyncRenderState() {
    if (GetTransform() && sprite_) {
        // SpriteはZ位置も保持できるが基本は2D
        sprite_->SetPosition(GetTransform()->GetWorldPosition().x, GetTransform()->GetWorldPosition().y,
                             GetTransform()->GetWorldPosition().z);
        // Spriteの回転はZ軸のみ
        sprite_->SetRotation(GetTransform()->GetWorldRotation().z);

        // TransformのScaleは、SpriteのBaseサイズに対するスケーリングとして扱う
        sprite_->SetSize(size_.x * GetTransform()->GetWorldScale().x, size_.y * GetTransform()->GetWorldScale().y);
        sprite_->Update();
    }
}

void SpriteRendererComponent::Update() {
    SyncRenderState();
}

void SpriteRendererComponent::Draw() {
    if (sprite_) {
        sprite_->Draw(); // SyncBeforeDrawはSprite内で呼ばれる
    }
}

void SpriteRendererComponent::SetTexture(const std::string& texturePath) {
    texturePath_ = texturePath;
    if (sprite_) {
        sprite_->SetTexture(texturePath_);
        // テクスチャ変更に合わせてサイズを更新
        size_.x = sprite_->GetSize().x;
        size_.y = sprite_->GetSize().y;
    }
}

void SpriteRendererComponent::SetAnchor(const Irufemi::Vector2& anchor) {
    anchor_ = anchor;
    if (sprite_) {
        sprite_->SetAnchor(anchor_.x, anchor_.y);
    }
}

void SpriteRendererComponent::SetTopMost(bool isTopMost) {
    isTopMost_ = isTopMost;
    if (sprite_) {
        sprite_->SetTopMost(isTopMost_);
    }
}

void SpriteRendererComponent::SetFlipX(bool isFlipX) {
    isFlipX_ = isFlipX;
    if (sprite_) {
        sprite_->SetFlip(isFlipX_, isFlipY_);
    }
}

void SpriteRendererComponent::SetFlipY(bool isFlipY) {
    isFlipY_ = isFlipY;
    if (sprite_) {
        sprite_->SetFlip(isFlipX_, isFlipY_);
    }
}

void SpriteRendererComponent::SetBaseSize(const Irufemi::Vector2& size) {
    size_ = size;
}

nlohmann::json SpriteRendererComponent::Serialize() {
    nlohmann::json j;
    j["texturePath"] = texturePath_;
    j["isTopMost"] = isTopMost_;
    j["isFlipX"] = isFlipX_;
    j["isFlipY"] = isFlipY_;
    j["anchor"] = nlohmann::json::array({anchor_.x, anchor_.y});
    j["size"] = nlohmann::json::array({size_.x, size_.y});
    j["color"] = nlohmann::json::array({color_.x, color_.y, color_.z, color_.w});
    return j;
}

void SpriteRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("texturePath")) {
        SetTexture(j["texturePath"]);
    }
    if (j.contains("isTopMost")) {
        isTopMost_ = j["isTopMost"];
    }
    if (j.contains("isFlipX")) {
        isFlipX_ = j["isFlipX"];
    }
    if (j.contains("isFlipY")) {
        isFlipY_ = j["isFlipY"];
    }
    if (j.contains("anchor") && j["anchor"].is_array() && j["anchor"].size() == 2) {
        anchor_.x = j["anchor"][0];
        anchor_.y = j["anchor"][1];
    }
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2) {
        size_.x = j["size"][0];
        size_.y = j["size"][1];
    }
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color_.x = j["color"][0];
        color_.y = j["color"][1];
        color_.z = j["color"][2];
        color_.w = j["color"][3];
    }

    // 反映
    if (sprite_) {
        sprite_->SetAnchor(anchor_.x, anchor_.y);
        sprite_->SetFlip(isFlipX_, isFlipY_);
        sprite_->SetTopMost(isTopMost_);
        sprite_->SetColor(color_);
        // サイズの反映はUpdateでscaleを考慮して行われるが、ベースサイズとして保持
    }
}
