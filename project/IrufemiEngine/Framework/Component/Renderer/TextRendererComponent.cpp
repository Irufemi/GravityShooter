#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/Utility/StringUtility.h"
#include "Physics/Collision/Collision.h"
#include "Core/Shape/Sphere.h"
#include "Core/Utility/Log.h"
#include "Core/System/IrufemiEngine.h"
#include <algorithm>
#include <iostream>

TextRendererComponent::TextRendererComponent() {}

TextRendererComponent::~TextRendererComponent() {}

void TextRendererComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void TextRendererComponent::OnAwake() {
    if (!textObj_) {
        textObj_ = std::make_unique<Text>();
        if (auto engine = GetEngine()) {
            textObj_->SetFontManagerInstance(engine->GetFontManager());
            textObj_->SetDrawManagerInstance(engine->GetDrawManager());
            textObj_->SetCameraManagerInstance(engine->GetCameraManager());
        }
    }
    // リフレクション変数との同期を確実に保証
    text_ = ConvertString(textU8_);
    alignment_ = static_cast<TextAlignment>(alignmentInt_);
    verticalAlignment_ = static_cast<TextVerticalAlignment>(verticalAlignmentInt_);

    textObj_->Initialize(fontId_);
    textObj_->SetText(text_);
    textObj_->SetBaseScale(baseScale_);
    textObj_->SetColor(color_);
    textObj_->SetTopMost(isTopMost_);
    textObj_->SetAlignment(alignment_);
    textObj_->SetVerticalAlignment(verticalAlignment_);
    // ロード画面中に生成を終わらせるため、初期化時に強制アップデート（非同期タスク待ち・頂点生成）
    textObj_->Update();
}

void TextRendererComponent::OnSpawned() {
    SyncRenderState();
}

void TextRendererComponent::SyncRenderState() {
    if (!GetTransform() || !textObj_) {
        return;
    }
    // Transformの変更をTextオブジェクトに反映
    textObj_->SetPosition(GetTransform()->GetWorldPosition().x, GetTransform()->GetWorldPosition().y,
                          GetTransform()->GetWorldPosition().z);
    textObj_->SetRotation(GetTransform()->GetWorldRotation().z); // 2DなのでZ軸回転
    textObj_->SetScale(GetTransform()->GetWorldScale().x, GetTransform()->GetWorldScale().y);
    textObj_->Update();
}

void TextRendererComponent::Update() {
    if (!GetTransform() || !textObj_) {
        return;
    }

    // Reflection / Deserialize sync
    std::wstring newText = ConvertString(textU8_);
    if (text_ != newText) {
        SetText(newText);
    }
    if (textObj_->GetFontId() != fontId_) {
        SetFontId(fontId_);
    }
    if (textObj_->GetBaseScale() != baseScale_) {
        SetBaseScale(baseScale_);
    }
    if (textObj_->GetColor() != color_) {
        SetColor(color_);
    }
    if (textObj_->IsTopMost() != isTopMost_) {
        SetTopMost(isTopMost_);
    }
    auto newAlign = static_cast<TextAlignment>(alignmentInt_);
    if (alignment_ != newAlign) {
        SetAlignment(newAlign);
    }

    SyncRenderState();
}

void TextRendererComponent::Draw() {
    textObj_->Draw();
}

bool TextRendererComponent::Raycast(const Irufemi::Ray& ray, float& outDistance) const {
    if (!gameObject_) {
        return false;
    }
    auto transform = GetTransform();
    if (!transform) {
        return false;
    }

    // 簡易的にBoundingSphereで判定
    Irufemi::Sphere sphere;
    sphere.center = transform->GetWorldPosition();
    float maxScale = (std::max)({transform->GetWorldScale().x, transform->GetWorldScale().y});
    // 文字列の長さに応じたバウンディングスフィア半径を概算
    sphere.radius = maxScale * baseScale_ * (text_.length() * 0.5f);

    return Irufemi::Collision::IsCollision(ray, sphere, outDistance);
}

void TextRendererComponent::SetText(const std::wstring& text) {
    text_ = text;
    textU8_ = ConvertString(text_);
    if (textObj_) {
        textObj_->SetText(text_);
    }
}

void TextRendererComponent::SetFontId(const std::string& fontId) {
    fontId_ = fontId;
    if (textObj_) {
        textObj_->SetFontId(fontId_);
    }
}

void TextRendererComponent::SetBaseScale(float baseScale) {
    baseScale_ = baseScale;
    if (textObj_) {
        textObj_->SetBaseScale(baseScale_);
    }
}

void TextRendererComponent::SetColor(const Irufemi::Vector4& color) {
    color_ = color;
    if (textObj_) {
        textObj_->SetColor(color_);
    }
}

void TextRendererComponent::SetTopMost(bool isTopMost) {
    isTopMost_ = isTopMost;
    if (textObj_) {
        textObj_->SetTopMost(isTopMost_);
    }
}

void TextRendererComponent::SetAlignment(TextAlignment align) {
    alignment_ = align;
    alignmentInt_ = static_cast<int>(align);
    if (textObj_) {
        textObj_->SetAlignment(alignment_);
    }
}

void TextRendererComponent::SetVerticalAlignment(TextVerticalAlignment vAlign) {
    verticalAlignment_ = vAlign;
    verticalAlignmentInt_ = static_cast<int>(vAlign);
    if (textObj_) {
        textObj_->SetVerticalAlignment(verticalAlignment_);
    }
}

void TextRendererComponent::OnRegisterProperties() {
    textU8_ = ConvertString(text_);
    alignmentInt_ = static_cast<int>(alignment_);
    verticalAlignmentInt_ = static_cast<int>(verticalAlignment_);

    RegisterProperty("text", &textU8_);
    RegisterProperty("fontId", &fontId_);
    RegisterProperty("baseScale", &baseScale_);
    RegisterProperty("color", &color_);
    RegisterProperty("alignment", &alignmentInt_);
    RegisterProperty("verticalAlignment", &verticalAlignmentInt_);
    RegisterProperty("isTopMost", &isTopMost_);
}

void TextRendererComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);

    // デシリアライズ直後に UTF-8文字列およびアライメント数値を型安全に即時同期
    text_ = ConvertString(textU8_);
    alignment_ = static_cast<TextAlignment>(alignmentInt_);
    verticalAlignment_ = static_cast<TextVerticalAlignment>(verticalAlignmentInt_);

    if (textObj_) {
        textObj_->SetText(text_);
        textObj_->SetFontId(fontId_);
        textObj_->SetBaseScale(baseScale_);
        textObj_->SetColor(color_);
        textObj_->SetTopMost(isTopMost_);
        textObj_->SetAlignment(alignment_);
        textObj_->SetVerticalAlignment(verticalAlignment_);
    }
}
