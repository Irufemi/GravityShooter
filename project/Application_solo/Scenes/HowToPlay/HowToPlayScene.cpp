#include "Scenes/HowToPlay/HowToPlayScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Renderer/Font/FontManager.h"
#include "UI/UISound.h"
#include <cmath>
#include <algorithm>

using namespace Irufemi;

void HowToPlayScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
    openCooldownTimer_ = 0.15f;
    transition_.StartOpen();
    uiAnimator_.Clear();

    // 操作説明テキストの事前キャッシュ（初回オープン時のヒッチ・コマ落ちを完全根絶）
    if (engine) {
        if (auto fm = engine->GetFontManager()) {
            const std::string fontId = "NotoSansJP-Bold";
            fm->PrecacheTextSync(fontId, L"--- 操作方法 (HOW TO PLAY) ---");
            fm->PrecacheTextSync(fontId, L"アクション キーボード / マウス ゲームパッド");
            fm->PrecacheTextSync(fontId, L"機体移動 照準 (レティクル) 移動 ガレキ引寄 敵・ガレキ ロックオン ガレキ投擲 "
                                         L"/ 攻撃 ロックオン解除 ポーズメニュー");
            fm->PrecacheTextSync(fontId, L"W / A / S / D マウス移動 E キー 右クリック 左クリック / Q R キー ESC");
            fm->PrecacheTextSync(fontId,
                                 L"左スティック 右スティック LT / LB RB / A ボタン RT (トリガー) B ボタン START");
            fm->PrecacheTextSync(fontId, L"★ シールド強奪: ボスのシールドをロックして引寄 (E / LT) で弾丸を鹵獲！ "
                                         L"一斉投擲: 投擲を行うと、ロックした全ての対象へ同時にガレキを発射！");
            fm->PrecacheTextSync(fontId, L"[ Bボタン  /  ESC  /  クリック で閉じる ]");
        }
    }
}

void HowToPlayScene::OnEnter() {
    Irufemi::BaseScene::OnEnter();
    openCooldownTimer_ = 0.15f;
    transition_.StartOpen();
    uiAnimator_.Clear();
}

void HowToPlayScene::CacheUIElements() {
    uiAnimator_.CacheElements(GetGameObjects(), "DarkBackdrop");
}

void HowToPlayScene::ApplyTransition(float progress) {
    if (!uiAnimator_.IsCached()) {
        CacheUIElements();
    }
    uiAnimator_.ApplyTransition(progress);
}

void HowToPlayScene::Update() {
    Irufemi::BaseScene::Update();

    if (!engine_) {
        return;
    }

    float dt = engine_->GetDeltaTime();

    if (!uiAnimator_.IsCached()) {
        CacheUIElements();
        ApplyTransition(0.0f); // 初回フレームで非表示から開始
    }

    // --- トランジション演出更新 (Opening: Fade&Scale In / Closing: Fade&Scale Out) ---
    if (transition_.Update(
            dt, [this](float progress) { ApplyTransition(progress); },
            [this]() {
                if (auto sm = engine_->GetSceneManager()) {
                    sm->PopScene();
                }
            })) {
        return; // オープン/クローズ途中は入力をガード
    }

    // --- 通常表示状態 (Open) 入力判定 ---
    auto inputManager = engine_->GetInputManager();
    if (!inputManager) {
        return;
    }

    if (openCooldownTimer_ > 0.0f) {
        openCooldownTimer_ -= dt;
        return; // 開いた直後のクリック・ボタン入力残存による即時クローズをガード
    }

    // 閉じる入力判定 (Bボタン / ESC / BackSpace / マウス左クリック)
    bool closeTrigger = inputManager->IsButtonPressed(XINPUT_GAMEPAD_B) || inputManager->IsKeyPressed(VK_ESCAPE) ||
                        inputManager->IsKeyPressed(VK_BACK);

    if (!closeTrigger && inputManager->GetMouse()) {
        closeTrigger =
            inputManager->GetMouse()->IsButtonPressed(Mouse::Button::Left) || inputManager->IsKeyPressed(VK_LBUTTON);
    }

    if (closeTrigger) {
        UISound::PlayCancel();
        transition_.StartClose();
    }
}

void HowToPlayScene::Draw() {
    Irufemi::BaseScene::Draw();
}
