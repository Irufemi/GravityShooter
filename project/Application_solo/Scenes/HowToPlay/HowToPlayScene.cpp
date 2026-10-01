#include "Scenes/HowToPlay/HowToPlayScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"

void HowToPlayScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);
    openCooldownTimer_ = 0.15f;
}

void HowToPlayScene::OnEnter() {
    BaseScene::OnEnter();
    openCooldownTimer_ = 0.15f;
}

void HowToPlayScene::Update() {
    BaseScene::Update();

    if (!engine_) {
        return;
    }

    auto inputManager = engine_->GetInputManager();
    if (!inputManager) {
        return;
    }

    float dt = engine_->GetDeltaTime();
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
        // タイトル画面へ戻る (スタックからポップ)
        if (auto sm = engine_->GetSceneManager()) {
            sm->PopScene();
        }
    }
}

void HowToPlayScene::Draw() {
    BaseScene::Draw();
}
