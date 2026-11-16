#include "Scenes/HowToPlay/HowToPlayScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "UI/UISound.h"
#include <cmath>
#include <algorithm>

using namespace Irufemi;

void HowToPlayScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
    openCooldownTimer_ = 0.15f;
    transition_.StartOpen();
    uiAnimator_.Clear();
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
