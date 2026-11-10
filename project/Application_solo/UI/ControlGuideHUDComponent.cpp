#include "UI/ControlGuideHUDComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"

ControlGuideHUDComponent::ControlGuideHUDComponent() = default;
ControlGuideHUDComponent::~ControlGuideHUDComponent() = default;

void ControlGuideHUDComponent::Initialize() {
    EnsureUIElements();
}

void ControlGuideHUDComponent::Start() {
    EnsureUIElements();
}

ControlGuideHUDComponent* ControlGuideHUDComponent::GetOrCreate(BaseScene* scene) {
    if (!scene) {
        return nullptr;
    }
    if (auto obj = scene->FindGameObject("HUD_ControlGuideContainer")) {
        if (auto comp = obj->GetComponent<ControlGuideHUDComponent>()) {
            return comp;
        }
    }
    auto hudObj = std::make_shared<GameObject>("HUD_ControlGuideContainer");
    scene->AddGameObject(hudObj);
    if (auto t = hudObj->GetTransform()) {
        t->SetPosition({285.0f, 665.0f, 0.0f});
    }
    auto comp = hudObj->AddComponent<ControlGuideHUDComponent>();
    hudObj->Initialize();
    return comp.get();
}

void ControlGuideHUDComponent::EnsureUIElements() {
    if (isElementsCreated_) {
        return;
    }
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (!scene) {
        return;
    }

    containerObj_ = gameObject_->shared_from_this();

    const float panelW = 540.0f;
    const float panelH = 76.0f;
    const float halfW = panelW * 0.5f;
    const float halfH = panelH * 0.5f;

    // 1. 半透明ダークネイビー・バックプレート (Z: 2.0f)
    plateObj_ = std::make_shared<GameObject>("Guide_Backplate");
    scene->AddGameObject(plateObj_);
    containerObj_->AddChild(plateObj_);
    if (auto t = plateObj_->GetTransform()) {
        t->SetPosition({0.0f, 0.0f, 2.0f});
    }
    plateSprite_ = plateObj_->AddComponent<SpriteRendererComponent>().get();
    plateSprite_->SetBaseSize({panelW, panelH});
    plateSprite_->SetAnchor({0.5f, 0.5f});
    plateSprite_->SetColor({0.02f, 0.04f, 0.07f, 0.65f});
    plateObj_->Initialize();

    // 2. 上部ボーダーライン (厚さ 1.5px, Z: 1.5f)
    topBorderObj_ = std::make_shared<GameObject>("Guide_TopBorder");
    scene->AddGameObject(topBorderObj_);
    containerObj_->AddChild(topBorderObj_);
    if (auto t = topBorderObj_->GetTransform()) {
        t->SetPosition({0.0f, -halfH, 1.5f});
    }
    topBorderSprite_ = topBorderObj_->AddComponent<SpriteRendererComponent>().get();
    topBorderSprite_->SetBaseSize({panelW, 1.5f});
    topBorderSprite_->SetAnchor({0.5f, 0.5f});
    topBorderSprite_->SetColor({0.30f, 0.85f, 1.00f, 0.60f});
    topBorderObj_->Initialize();

    // 3. 左端アクセントバー (厚さ 3.0px, Z: 1.5f)
    leftBarObj_ = std::make_shared<GameObject>("Guide_LeftBar");
    scene->AddGameObject(leftBarObj_);
    containerObj_->AddChild(leftBarObj_);
    if (auto t = leftBarObj_->GetTransform()) {
        t->SetPosition({-halfW, 0.0f, 1.5f});
    }
    leftBarSprite_ = leftBarObj_->AddComponent<SpriteRendererComponent>().get();
    leftBarSprite_->SetBaseSize({3.0f, panelH});
    leftBarSprite_->SetAnchor({0.5f, 0.5f});
    leftBarSprite_->SetColor({0.30f, 0.85f, 1.00f, 0.80f});
    leftBarObj_->Initialize();

    // 4. ヘッダーテキスト (日本語＋英語)
    headerTextObj_ = std::make_shared<GameObject>("Guide_HeaderText");
    scene->AddGameObject(headerTextObj_);
    containerObj_->AddChild(headerTextObj_);
    if (auto t = headerTextObj_->GetTransform()) {
        t->SetPosition({-halfW + 18.0f, -24.0f, 0.0f});
    }
    headerText_ = headerTextObj_->AddComponent<TextRendererComponent>().get();
    headerText_->SetFontId(fontId_);
    headerText_->SetText(L"// FCS CONTROLS: 操作システム");
    headerText_->SetBaseScale(10.0f);
    headerText_->SetTopMost(true);
    headerText_->SetAlignment(TextAlignment::Left);
    headerText_->SetColor({0.40f, 0.85f, 1.00f, 0.75f});
    headerTextObj_->Initialize();

    // 5. 操作行 1: ガレキ引き寄せ ＆ ロックオン ＆ 一斉射撃
    row1TextObj_ = std::make_shared<GameObject>("Guide_Row1Text");
    scene->AddGameObject(row1TextObj_);
    containerObj_->AddChild(row1TextObj_);
    if (auto t = row1TextObj_->GetTransform()) {
        t->SetPosition({-halfW + 18.0f, -4.0f, 0.0f});
    }
    row1Text_ = row1TextObj_->AddComponent<TextRendererComponent>().get();
    row1Text_->SetFontId(fontId_);
    row1Text_->SetText(L"[E / LT] ガレキ引寄    [右クリック / RB] ロックオン    [左クリック / RT] 一斉射撃");
    row1Text_->SetBaseScale(11.0f);
    row1Text_->SetTopMost(true);
    row1Text_->SetAlignment(TextAlignment::Left);
    row1Text_->SetColor({0.90f, 0.95f, 1.00f, 0.95f});
    row1TextObj_->Initialize();

    // 6. 操作行 2: ロック解除 ＆ 機体移動 ＆ ポーズ
    row2TextObj_ = std::make_shared<GameObject>("Guide_Row2Text");
    scene->AddGameObject(row2TextObj_);
    containerObj_->AddChild(row2TextObj_);
    if (auto t = row2TextObj_->GetTransform()) {
        t->SetPosition({-halfW + 18.0f, 18.0f, 0.0f});
    }
    row2Text_ = row2TextObj_->AddComponent<TextRendererComponent>().get();
    row2Text_->SetFontId(fontId_);
    row2Text_->SetText(L"[R / B]  ロック解除    [WASD / Lスティック] 機体移動    [ESC / START] ポーズ");
    row2Text_->SetBaseScale(11.0f);
    row2Text_->SetTopMost(true);
    row2Text_->SetAlignment(TextAlignment::Left);
    row2Text_->SetColor({0.75f, 0.82f, 0.92f, 0.85f});
    row2TextObj_->Initialize();

    isElementsCreated_ = true;
}

void ControlGuideHUDComponent::Update() {
}

void ControlGuideHUDComponent::SetVisible(bool visible) {
    isVisible_ = visible;
    if (containerObj_) {
        containerObj_->SetActive(visible);
    }
}
