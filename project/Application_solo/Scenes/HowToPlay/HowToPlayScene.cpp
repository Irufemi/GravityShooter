#include "Scenes/HowToPlay/HowToPlayScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"

void HowToPlayScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);
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

    // 閉じる入力判定 (Bボタン / ESC / BackSpace / マウス左クリック)
    bool closeTrigger = inputManager->IsButtonPressed(XINPUT_GAMEPAD_B) || inputManager->IsKeyPressed(VK_ESCAPE) ||
                        inputManager->IsKeyPressed(VK_BACK);

    if (!closeTrigger && inputManager->GetMouse()) {
        closeTrigger =
            inputManager->GetMouse()->IsButtonPressed(Mouse::Button::Left) || inputManager->IsKeyPressed(VK_LBUTTON);
    }

    if (closeTrigger) {
        // キャンセル音再生
        if (auto audioManager = engine_->GetAudioManager()) {
            auto sound = audioManager->GetOrLoadSoundByFile("resources/audio/se_menu_cancel.wav", "se_menu_cancel");
            if (sound) {
                audioManager->Play(sound, false, 0.7f);
            }
        }

        // タイトル画面へ戻る (スタックからポップ)
        if (auto sm = engine_->GetSceneManager()) {
            sm->PopScene();
        }
    }
}

void HowToPlayScene::Draw() {
    BaseScene::Draw();
}
