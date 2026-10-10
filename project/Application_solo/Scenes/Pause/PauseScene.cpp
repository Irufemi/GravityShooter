#include "Scenes/Pause/PauseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"
#include "Input/GameAction.h"
#include "Audio/Sound.h"
#include "Renderer/Font/FontManager.h"
#include "Framework/Utility/CVar.h"
#include "UI/UISound.h"
#include <algorithm>
#include <cmath>

namespace {
// メニュー項目のY座標
constexpr float kMenuStartY = 260.0f;
constexpr float kMenuItemSpacing = 68.0f;

// カラー定数
const Irufemi::Vector4 kColorSelected = {0.2f, 1.0f, 1.0f, 1.0f};     // 発光シアン
const Irufemi::Vector4 kColorUnselected = {0.7f, 0.75f, 0.8f, 0.8f};  // 淡いブルーグレー
const Irufemi::Vector4 kColorTitle = {0.1f, 0.95f, 1.0f, 1.0f};       // ネオンシアン
const Irufemi::Vector4 kColorDarkMask = {0.02f, 0.03f, 0.05f, 0.75f}; // 半透明ダークマスク
} // namespace

PauseScene::PauseScene() = default;

PauseScene::~PauseScene() {
    // 確実に元のBGM音量を復元
    Irufemi::CVarSystem::SetFloat("a.BGMVolume", originalBGMVolume_);
}

void PauseScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // テキストキャッシュを事前生成
    if (engine) {
        if (auto fm = engine->GetFontManager()) {
            fm->PrecacheText("toro_glitch", L"- PAUSE -");
            fm->PrecacheText("toro_glitch", L"RESUME");
            fm->PrecacheText("toro_glitch", L"RETRY");
            fm->PrecacheText("toro_glitch", L"OPTIONS");
            fm->PrecacheText("toro_glitch", L"TITLE");
            fm->PrecacheText("toro_glitch", L"QUIT");
        }
    }

    // 現在のBGM音量を退避し、ポーズ中は50%にダッキング
    originalBGMVolume_ = Irufemi::CVarSystem::GetFloat("a.BGMVolume");
    Irufemi::CVarSystem::SetFloat("a.BGMVolume", originalBGMVolume_ * 0.45f);

    CreateUIElements();

    // 突入時SE
    UISound::PlayDecide(0.8f);
    openCooldownTimer_ = 0.2f;
}

void PauseScene::OnEnter() {
    BaseScene::OnEnter();
    openCooldownTimer_ = 0.2f;

    if (engine_) {
        engine_->SetCursorLocked(false);
    }
}

void PauseScene::OnExit() {
    BaseScene::OnExit();
    // BGM音量を復元
    Irufemi::CVarSystem::SetFloat("a.BGMVolume", originalBGMVolume_);
}

void PauseScene::OnSuspend() {
    BaseScene::OnSuspend();
    isSuspended_ = true;
    SetUIVisible(false); // ポーズUIを非表示にしてOptionsSceneとの文字被りを完全防止
}

void PauseScene::OnResume() {
    BaseScene::OnResume();
    isSuspended_ = false;
    SetUIVisible(true);
    openCooldownTimer_ = 0.2f; // OptionsSceneを閉じた直後の誤入力防止

    // オプション画面で変更された可能性のある最新のBGM音量を取得し、ダッキングを再適用
    originalBGMVolume_ = Irufemi::CVarSystem::GetFloat("a.BGMVolume");
    Irufemi::CVarSystem::SetFloat("a.BGMVolume", originalBGMVolume_ * 0.45f);

    if (engine_) {
        engine_->SetCursorLocked(false);
    }
}

void PauseScene::SetUIVisible(bool visible) {
    if (darkOverlayObj_) {
        darkOverlayObj_->SetActive(visible);
    }
    if (titleObj_) {
        titleObj_->SetActive(visible);
    }
    for (auto& item : menuItems_) {
        if (item.gameObject) {
            item.gameObject->SetActive(visible);
        }
    }
}

void PauseScene::CreateUIElements() {
    // 1. 半透明暗幕オーバーレイ (画面全域 1280x720)
    darkOverlayObj_ = std::make_shared<GameObject>("Pause_DarkOverlay");
    AddGameObject(darkOverlayObj_);
    if (auto t = darkOverlayObj_->GetTransform()) {
        t->SetPosition({640.0f, 360.0f, 5.0f});
    }
    auto sprite = darkOverlayObj_->AddComponent<SpriteRendererComponent>();
    if (sprite) {
        sprite->SetTexture("resources/whiteTexture.png");
        sprite->SetBaseSize({1280.0f, 720.0f});
        sprite->SetAnchor({0.5f, 0.5f});
        sprite->SetColor(kColorDarkMask);
    }
    darkOverlayObj_->Initialize();

    // 2. ヘッダータイトル "- PAUSE -"
    titleObj_ = std::make_shared<GameObject>("Pause_HeaderTitle");
    AddGameObject(titleObj_);
    if (auto t = titleObj_->GetTransform()) {
        t->SetPosition({640.0f, 150.0f, 0.0f});
    }
    auto titleText = titleObj_->AddComponent<TextRendererComponent>();
    if (titleText) {
        titleText->SetFontId("toro_glitch");
        titleText->SetText(L"- PAUSE -");
        titleText->SetBaseScale(56.0f);
        titleText->SetAlignment(TextAlignment::Center);
        titleText->SetColor(kColorTitle);
        titleText->SetTopMost(true);
    }
    titleObj_->Initialize();

    // 3. メニュー項目リスト（データドリブン設計: 表示テキスト・種別・実行アクションを一元定義）
    struct MenuEntry {
        std::wstring text;
        MenuItem type;
        std::function<void()> action;
    };

    const std::vector<MenuEntry> entries = {{L"RESUME", MenuItem::Resume,
                                             [this]() {
                                                 UISound::PlayCancel();
                                                 if (auto sm = engine_ ? engine_->GetSceneManager() : nullptr) {
                                                     sm->PopScene();
                                                 }
                                             }},
                                            {L"RETRY", MenuItem::Retry,
                                             [this]() {
                                                 UISound::PlayDecide();
                                                 if (auto sm = engine_ ? engine_->GetSceneManager() : nullptr) {
                                                     sm->LoadScene("InGame", SceneTransition::Type::Fade, 0.5f);
                                                 }
                                             }},
                                            {L"OPTIONS", MenuItem::Options,
                                             [this]() {
                                                 UISound::PlayDecide();
                                                 if (auto sm = engine_ ? engine_->GetSceneManager() : nullptr) {
                                                     sm->PushScene("OptionsScene");
                                                 }
                                             }},
                                            {L"TITLE", MenuItem::Title,
                                             [this]() {
                                                 UISound::PlayDecide();
                                                 if (auto sm = engine_ ? engine_->GetSceneManager() : nullptr) {
                                                     sm->TransitionTo("Title", SceneTransition::Type::Fade, 0.6f);
                                                 }
                                             }},
                                            {L"QUIT", MenuItem::Quit, []() {
                                                 UISound::PlayDecide();
                                                 PostQuitMessage(0);
                                             }}};

    menuItems_.clear();
    for (size_t i = 0; i < entries.size(); ++i) {
        float y = kMenuStartY + static_cast<float>(i) * kMenuItemSpacing;

        auto itemObj = std::make_shared<GameObject>("Pause_Item_" + std::to_string(i));
        AddGameObject(itemObj);
        if (auto t = itemObj->GetTransform()) {
            t->SetPosition({640.0f, y, 0.0f});
        }

        auto textComp = itemObj->AddComponent<TextRendererComponent>();
        if (textComp) {
            textComp->SetFontId("toro_glitch");
            textComp->SetText(entries[i].text);
            textComp->SetBaseScale(34.0f);
            textComp->SetAlignment(TextAlignment::Center);
            textComp->SetColor(kColorUnselected);
            textComp->SetTopMost(true);
        }
        itemObj->Initialize();

        ItemData data;
        data.text = entries[i].text;
        data.itemType = entries[i].type;
        data.action = entries[i].action;
        data.yPos = y;
        data.gameObject = itemObj;
        data.textComp = textComp.get();
        data.currentScale = 1.0f;
        menuItems_.push_back(std::move(data));
    }

    selectedIndex_ = 0;
    lastHoveredIndex_ = -1;
}

void PauseScene::Update() {
    if (isSuspended_) {
        return; // オプション画面表示中は更新しない
    }

    BaseScene::Update();

    if (!engine_) {
        return;
    }

    float dt = engine_->GetDeltaTime();
    if (openCooldownTimer_ > 0.0f) {
        openCooldownTimer_ -= dt;
        return; // 開いた直後のボタン入力残存ガード
    }

    UpdateInput(dt);
    UpdateSelectionVisuals(dt);
}

void PauseScene::Draw() {
    if (isSuspended_) {
        return; // オプション画面表示中は描画しない（文字被りを完全防止）
    }

    BaseScene::Draw();
}

void PauseScene::UpdateInput(float deltaTime) {
    auto input = engine_->GetInputManager();
    if (!input) {
        return;
    }

    // 1. 即時ポーズ解除判定（ESCキー / Bボタン / STARTボタン）
    if (input->IsCancelPressed() || input->StartPressed()) {
        ExecuteAction(MenuItem::Resume);
        return;
    }

    // 仮想カーソルの更新
    input->UpdateVirtualCursor(deltaTime);
    const auto& cursorPos = input->GetVirtualCursorPosition();

    // 2. マウス/仮想カーソルによるホバー判定（動的Textバウンディングボックス＆スケール連動）
    int mouseHoveredIndex = -1;
    for (size_t i = 0; i < menuItems_.size(); ++i) {
        if (IsCursorOverItem(static_cast<int>(i), cursorPos)) {
            mouseHoveredIndex = static_cast<int>(i);
            break;
        }
    }

    if (mouseHoveredIndex >= 0 && mouseHoveredIndex != selectedIndex_) {
        selectedIndex_ = mouseHoveredIndex;
        UISound::PlayCursor();
    }

    // 3. キーボード・ゲームパッドの上下移動判定
    bool moveUp = input->IsKeyPressed(VK_UP) || input->IsKeyPressed('W') || input->DPadUp();
    bool moveDown = input->IsKeyPressed(VK_DOWN) || input->IsKeyPressed('S') || input->DPadDown();

    // スティック入力（デッドゾーン付き）
    float stickY = input->GetLeftStickY();
    if (stickY > 0.6f && lastHoveredIndex_ != -2) {
        moveUp = true;
        lastHoveredIndex_ = -2;
    } else if (stickY < -0.6f && lastHoveredIndex_ != -3) {
        moveDown = true;
        lastHoveredIndex_ = -3;
    } else if (std::abs(stickY) < 0.3f) {
        lastHoveredIndex_ = -1;
    }

    if (moveUp) {
        selectedIndex_ =
            (selectedIndex_ - 1 + static_cast<int>(menuItems_.size())) % static_cast<int>(menuItems_.size());
        UISound::PlayCursor();
    } else if (moveDown) {
        selectedIndex_ = (selectedIndex_ + 1) % static_cast<int>(menuItems_.size());
        UISound::PlayCursor();
    }

    // 4. 決定判定（論理アクション UI_Submit または カーソルクリック）
    bool isDecide = InputHelper::IsActionPressed(input, GameAction::UI_Submit);

    // カーソル押下開始の追跡
    if (input->IsCursorActionPressed()) {
        pressedItemIndex_ = -1;
        if (mouseHoveredIndex >= 0) {
            pressedItemIndex_ = mouseHoveredIndex;
            selectedIndex_ = mouseHoveredIndex;
            isDecide = true; // 即時レスポンス
        }
    }

    // 解放瞬間の確定（Drag-outキャンセル対応）
    if (input->IsCursorActionReleased()) {
        if (pressedItemIndex_ >= 0 && IsCursorOverItem(pressedItemIndex_, cursorPos)) {
            selectedIndex_ = pressedItemIndex_;
            isDecide = true;
        }
        pressedItemIndex_ = -1;
    }

    if (isDecide) {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(menuItems_.size())) {
            ExecuteAction(selectedIndex_);
        }
    }
}

void PauseScene::UpdateSelectionVisuals(float deltaTime) {
    for (size_t i = 0; i < menuItems_.size(); ++i) {
        auto& item = menuItems_[i];
        if (!item.textComp) {
            continue;
        }

        bool isSelected = (static_cast<int>(i) == selectedIndex_);
        float targetScale = isSelected ? 1.15f : 1.0f;
        // スムーズなイージング補間
        item.currentScale += (targetScale - item.currentScale) * std::clamp(deltaTime * 15.0f, 0.0f, 1.0f);

        item.textComp->SetBaseScale(34.0f * item.currentScale);
        item.textComp->SetColor(isSelected ? kColorSelected : kColorUnselected);
    }
}

void PauseScene::ExecuteAction(int index) {
    if (index >= 0 && index < static_cast<int>(menuItems_.size())) {
        if (menuItems_[index].action) {
            menuItems_[index].action();
        }
    }
}

void PauseScene::ExecuteAction(MenuItem item) {
    for (const auto& menuItem : menuItems_) {
        if (menuItem.itemType == item) {
            if (menuItem.action) {
                menuItem.action();
            }
            break;
        }
    }
}

bool PauseScene::IsCursorOverItem(int index, const Irufemi::Vector2& cursorPos) const {
    if (index < 0 || index >= static_cast<int>(menuItems_.size())) {
        return false;
    }

    const auto& item = menuItems_[index];
    if (!item.gameObject || !item.textComp) {
        return false;
    }

    auto transform = item.gameObject->GetTransform();
    if (!transform) {
        return false;
    }

    const auto& pos = transform->GetPosition();
    const auto& scale = transform->GetScale();
    const auto& minB = item.textComp->GetLocalBoundsMin();
    const auto& maxB = item.textComp->GetLocalBoundsMax();

    // テキスト幾何バウンディングボックスが有効な場合
    if (minB.x != maxB.x && minB.y != maxB.y) {
        const float kPadX = 22.0f;
        const float kPadY = 12.0f;

        float left = pos.x + (minB.x - kPadX) * scale.x;
        float right = pos.x + (maxB.x + kPadX) * scale.x;
        float top = pos.y + (minB.y - kPadY) * scale.y;
        float bottom = pos.y + (maxB.y + kPadY) * scale.y;

        return (cursorPos.x >= left && cursorPos.x <= right && cursorPos.y >= top && cursorPos.y <= bottom);
    }

    // フォールバック
    float halfW = 140.0f * scale.x;
    float halfH = 26.0f * scale.y;
    return (std::abs(cursorPos.x - pos.x) <= halfW && std::abs(cursorPos.y - pos.y) <= halfH);
}
