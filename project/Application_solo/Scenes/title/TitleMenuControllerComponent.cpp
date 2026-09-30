#include "Scenes/title/TitleMenuControllerComponent.h"

#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Scene/SceneTransition.h"
#include "Irufemi.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"

#ifdef EditorMode
#include "Core/EditorManager.h"
#endif

#include <cmath>
#include <algorithm>

void TitleMenuControllerComponent::Initialize() {
    currentIndex_ = 0;
    isHowToPlayOpen_ = false;
    isLaunching_ = false;
    stickCooldownTimer_ = 0.0f;

    currentScales_ = {1.15f, 1.0f, 1.0f, 1.0f};
    targetScales_ = {1.15f, 0.95f, 0.95f, 0.95f};
}

void TitleMenuControllerComponent::OnRegisterProperties() {
    // インスペクタ用プロパティ（必要に応じて追加）
}

void TitleMenuControllerComponent::Update() {
    if (isLaunching_) {
        // 出撃シーケンス中は追加入力を受け付けない
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float deltaTime = engine->GetDeltaTime();

    // スティッククールダウン減衰
    if (stickCooldownTimer_ > 0.0f) {
        stickCooldownTimer_ -= deltaTime;
    }

    // ナビゲーション入力処理
    HandleNavigationInput();

    // 決定入力処理
    HandleSelectionInput();

    // ボタンの視覚フィードバック（拡縮・発光アニメーション）
    UpdateButtonVisuals(deltaTime);
}

void TitleMenuControllerComponent::HandleNavigationInput() {
    auto engine = GetEngine();
    if (!engine) return;

    auto inputManager = engine->GetInputManager();
    if (!inputManager) return;

    int moveDelta = 0;

    // キーボード / 十字キー
    if (inputManager->IsKeyPressed(VK_UP) || inputManager->IsKeyPressed('W') ||
        inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_UP)) {
        moveDelta = -1;
    } else if (inputManager->IsKeyPressed(VK_DOWN) || inputManager->IsKeyPressed('S') ||
               inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN)) {
        moveDelta = 1;
    }

    // 左スティック入力 (デッドゾーン考慮)
    if (moveDelta == 0 && stickCooldownTimer_ <= 0.0f) {
        float stickY = inputManager->GetLeftStickY();
        if (stickY > 0.55f) {
            moveDelta = -1;
            stickCooldownTimer_ = kStickCooldown_;
        } else if (stickY < -0.55f) {
            moveDelta = 1;
            stickCooldownTimer_ = kStickCooldown_;
        }
    }

    // フォーカス移動の実行
    if (moveDelta != 0) {
        int oldIndex = currentIndex_;
        currentIndex_ = (currentIndex_ + moveDelta + static_cast<int>(buttonNames_.size())) % static_cast<int>(buttonNames_.size());

        if (oldIndex != currentIndex_) {
            PlaySE("resources/audio/se_menu_cursor.wav", "se_menu_cursor", 0.6f);

            // 目標スケールの更新
            for (size_t i = 0; i < targetScales_.size(); ++i) {
                targetScales_[i] = (static_cast<int>(i) == currentIndex_) ? 1.15f : 0.95f;
            }
        }
        return;
    }

    // --- マウスカーソルによるホバー検出 ---
    if (auto mouse = inputManager->GetMouse()) {
        const auto& mousePos = mouse->GetPosition();
        if (auto scene = GetScene()) {
            for (size_t i = 0; i < buttonNames_.size(); ++i) {
                if (auto btnObj = scene->FindGameObject(buttonNames_[i])) {
                    if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                        const auto& pos = transform->GetPosition();
                        // ボタンの当たり判定領域 (横幅 ±180px, 縦幅 ±25px)
                        if (std::abs(mousePos.x - pos.x) <= 180.0f && std::abs(mousePos.y - pos.y) <= 25.0f) {
                            if (currentIndex_ != static_cast<int>(i)) {
                                currentIndex_ = static_cast<int>(i);
                                PlaySE("resources/audio/se_menu_cursor.wav", "se_menu_cursor", 0.6f);
                                for (size_t k = 0; k < targetScales_.size(); ++k) {
                                    targetScales_[k] = (static_cast<int>(k) == currentIndex_) ? 1.15f : 0.95f;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

void TitleMenuControllerComponent::HandleSelectionInput() {
    auto engine = GetEngine();
    if (!engine) return;

    auto inputManager = engine->GetInputManager();
    if (!inputManager) return;

    // 決定キー (Aボタン / Space / Enter)
    bool isSelected = inputManager->IsButtonPressed(XINPUT_GAMEPAD_A) ||
                      inputManager->IsKeyPressed(VK_SPACE) ||
                      inputManager->IsKeyPressed(VK_RETURN);

    // マウス左クリックによる決定判定（選択中のボタン領域内でのクリック）
    if (!isSelected && inputManager->GetMouse()) {
        if (inputManager->GetMouse()->IsButtonPressed(Mouse::Button::Left) || inputManager->IsKeyPressed(VK_LBUTTON)) {
            if (auto scene = GetScene()) {
                if (auto btnObj = scene->FindGameObject(buttonNames_[currentIndex_])) {
                    if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                        const auto& mousePos = inputManager->GetMouse()->GetPosition();
                        const auto& pos = transform->GetPosition();
                        if (std::abs(mousePos.x - pos.x) <= 180.0f && std::abs(mousePos.y - pos.y) <= 25.0f) {
                            isSelected = true;
                        }
                    }
                }
            }
        }
    }

    if (isSelected) {
        ExecuteSelection();
    }
}

void TitleMenuControllerComponent::ExecuteSelection() {
    auto engine = GetEngine();
    if (!engine) return;

    PlaySE("resources/audio/se_menu_decide.wav", "se_menu_decide", 0.9f);

    switch (currentIndex_) {
        case 0: // GAME START
        {
            isLaunching_ = true;
            // 決定直後の出撃フェード遷移 (Step 4 で出撃シーケンスと統合)
            if (auto sm = engine->GetSceneManager()) {
                sm->TransitionTo("InGame", SceneTransition::Type::Fade, 0.9f);
            }
            break;
        }
        case 1: // HOW TO PLAY
        {
            if (auto sm = engine->GetSceneManager()) {
                sm->PushScene("HowToPlayScene");
            }
            break;
        }
        case 2: // OPTIONS
        {
            if (auto sm = engine->GetSceneManager()) {
                sm->PushScene("OptionsScene");
            }
            break;
        }
        case 3: // QUIT
        {
#ifdef EditorMode
            if (auto editor = EditorManager::GetInstance()) {
                editor->ExitPlayMode();
                break;
            }
#endif
            PostQuitMessage(0);
            break;
        }
        default:
            break;
    }
}

void TitleMenuControllerComponent::UpdateButtonVisuals(float deltaTime) {
    auto scene = GetScene();
    if (!scene) return;

    // 初回実行時にエディタ設定の初期スケールを自動キャッシュ（遅延取得）
    if (initialScales_.size() < buttonNames_.size()) {
        initialScales_.clear();
        for (const auto& name : buttonNames_) {
            if (auto btnObj = scene->FindGameObject(name)) {
                if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                    initialScales_.push_back(transform->GetScale());
                    continue;
                }
            }
            initialScales_.push_back({1.0f, 1.0f, 1.0f});
        }
    }

    const float kLerpSpeed = 14.0f;

    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        // スケール補間 (Spring/Lerp)
        currentScales_[i] = std::lerp(currentScales_[i], targetScales_[i], std::clamp(deltaTime * kLerpSpeed, 0.0f, 1.0f));

        auto btnObj = scene->FindGameObject(buttonNames_[i]);
        if (!btnObj) continue;

        auto transform = btnObj->GetComponent<TransformComponent>();
        if (transform && i < initialScales_.size()) {
            // エディタ設定の本来のスケール × 選択状態の乗率 (1.15倍 or 0.95倍)
            Irufemi::Vector3 baseScale = initialScales_[i];
            float s = currentScales_[i];
            transform->SetScale({baseScale.x * s, baseScale.y * s, baseScale.z});
        }

        // テキストコンポーネントがある場合の色補正
        auto text = btnObj->GetComponent<TextRendererComponent>();
        if (text) {
            if (static_cast<int>(i) == currentIndex_) {
                // 選択中: ネオンシアン強発光
                text->SetColor({0.1f, 0.95f, 1.0f, 1.0f});
            } else {
                // 非選択: 控えめな半透明ホワイト
                text->SetColor({0.7f, 0.75f, 0.8f, 0.65f});
            }
        }

        // スプライトコンポーネントがある場合の色補正
        auto sprite = btnObj->GetComponent<SpriteRendererComponent>();
        if (sprite) {
            if (static_cast<int>(i) == currentIndex_) {
                sprite->SetColor({0.2f, 0.95f, 1.0f, 1.0f});
            } else {
                sprite->SetColor({0.6f, 0.65f, 0.7f, 0.7f});
            }
        }
    }
}

void TitleMenuControllerComponent::PlaySE(const std::string& relativePath, const std::string& soundName, float volume) {
    auto engine = GetEngine();
    if (!engine) return;

    auto audioManager = engine->GetAudioManager();
    if (!audioManager) return;

    auto soundData = audioManager->GetOrLoadSoundByFile(relativePath, soundName);
    if (soundData) {
        audioManager->Play(soundData, false, volume);
    }
}
