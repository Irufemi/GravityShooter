#include "Scenes/title/TitleMenuControllerComponent.h"

#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/Scene/SceneTransition.h"
#include "Irufemi.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Input/GameAction.h"

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

    virtualCursorObj_ = nullptr;
    virtualCursorRenderer_ = nullptr;

    currentScales_ = {1.15f, 1.0f, 1.0f, 1.0f};
    targetScales_ = {1.15f, 0.95f, 0.95f, 0.95f};
}

void TitleMenuControllerComponent::OnRegisterProperties() {
    // インスペクタ用プロパティ（必要に応じて追加）
}

void TitleMenuControllerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float deltaTime = engine->GetDeltaTime();

    // 仮想カーソルの更新（出撃中や操作不能時は非表示処理を含む）
    UpdateVirtualCursor(deltaTime);

    if (isLaunching_) {
        // 出撃シーケンス中は追加入力を受け付けない
        return;
    }

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
    if (!engine) {
        return;
    }

    auto inputManager = engine->GetInputManager();
    if (!inputManager) {
        return;
    }

    int moveDelta = 0;

    // キーボード / 十字キー
    if (inputManager->IsKeyPressed(VK_UP) || inputManager->IsKeyPressed('W') ||
        inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_UP)) {
        moveDelta = -1;
    } else if (inputManager->IsKeyPressed(VK_DOWN) || inputManager->IsKeyPressed('S') ||
               inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN)) {
        moveDelta = 1;
    }

    // フォーカス移動の実行（キーボード / 十字キー）
    if (moveDelta != 0) {
        int oldIndex = currentIndex_;
        currentIndex_ =
            (currentIndex_ + moveDelta + static_cast<int>(buttonNames_.size())) % static_cast<int>(buttonNames_.size());

        if (oldIndex != currentIndex_) {
            PlaySE("resources/audio/se_menu_cursor.wav", "se_menu_cursor", 0.6f);

            // 目標スケールの更新
            for (size_t i = 0; i < targetScales_.size(); ++i) {
                targetScales_[i] = (static_cast<int>(i) == currentIndex_) ? 1.15f : 0.95f;
            }
        }
        return;
    }

    // --- 統合仮想カーソル（マウス / ゲームパッド左スティック）によるホバー検出 ---
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();
    if (auto scene = GetScene()) {
        for (size_t i = 0; i < buttonNames_.size(); ++i) {
            if (auto btnObj = scene->FindGameObject(buttonNames_[i])) {
                if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                    const auto& pos = transform->GetPosition();
                    // ボタンの当たり判定領域 (横幅 ±180px, 縦幅 ±25px)
                    if (std::abs(cursorPos.x - pos.x) <= 180.0f && std::abs(cursorPos.y - pos.y) <= 25.0f) {
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

void TitleMenuControllerComponent::HandleSelectionInput() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto inputManager = engine->GetInputManager();
    if (!inputManager) {
        return;
    }

    // 決定キー (論理アクション UI_Submit または 仮想カーソルアクション)
    bool isSelected =
        InputHelper::IsActionPressed(inputManager, GameAction::UI_Submit) || inputManager->IsCursorActionPressed();

    // マウス左クリックやRT等でカーソルアクションを押した場合は、カーソルがボタン領域内にあるかチェック
    if (inputManager->IsCursorActionPressed()) {
        const auto& cursorPos = inputManager->GetVirtualCursorPosition();
        if (auto scene = GetScene()) {
            bool clickedAny = false;
            for (size_t i = 0; i < buttonNames_.size(); ++i) {
                if (auto btnObj = scene->FindGameObject(buttonNames_[i])) {
                    if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                        const auto& pos = transform->GetPosition();
                        if (std::abs(cursorPos.x - pos.x) <= 180.0f && std::abs(cursorPos.y - pos.y) <= 25.0f) {
                            currentIndex_ = static_cast<int>(i);
                            clickedAny = true;
                            break;
                        }
                    }
                }
            }
            if (!clickedAny) {
                isSelected = false; // ボタン領域外のクリック時は決定しない
            }
        }
    }

    if (isSelected) {
        ExecuteSelection();
    }
}

void TitleMenuControllerComponent::ExecuteSelection() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    PlaySE("resources/audio/se_menu_decide.wav", "se_menu_decide", 0.9f);

    switch (currentIndex_) {
    case 0: // GAME START
    {
        isLaunching_ = true;
        SetMenuVisible(false); // 出撃時はメニューUIを退避

        // TitleSceneDirectorComponent による出撃シーケンス（重力波パルス・自機加速・ドリーイン）を実行
        if (auto scene = GetScene()) {
            if (auto menuMgr = scene->FindGameObject("MenuManager")) {
                if (auto director = menuMgr->GetComponent<TitleSceneDirectorComponent>()) {
                    director->StartLaunchSequence();
                    break;
                }
            }
        }

        // フォールバック遷移
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
            editor->RequestExitPlayMode();
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
    if (!scene) {
        return;
    }

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
        currentScales_[i] =
            std::lerp(currentScales_[i], targetScales_[i], std::clamp(deltaTime * kLerpSpeed, 0.0f, 1.0f));

        auto btnObj = scene->FindGameObject(buttonNames_[i]);
        if (!btnObj) {
            continue;
        }

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
    if (!engine) {
        return;
    }

    auto audioManager = engine->GetAudioManager();
    if (!audioManager) {
        return;
    }

    auto soundData = audioManager->GetOrLoadSoundByFile(relativePath, soundName);
    if (soundData) {
        audioManager->Play(soundData, false, volume);
    }
}

void TitleMenuControllerComponent::SetMenuVisible(bool visible) {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // タイトルロゴの表示/非表示
    if (auto titleObj = scene->FindGameObject("TitleText")) {
        titleObj->SetActive(visible);
    }

    // 各ボタン項目の表示/非表示
    for (const auto& name : buttonNames_) {
        if (auto btnObj = scene->FindGameObject(name)) {
            btnObj->SetActive(visible);
        }
    }

    // 仮想カーソルの表示/非表示
    if (virtualCursorObj_) {
        virtualCursorObj_->SetActive(visible);
    }
}

void TitleMenuControllerComponent::UpdateVirtualCursor(float deltaTime) {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto inputManager = engine->GetInputManager();
    if (!inputManager) {
        return;
    }

    // 仮想カーソルGameObjectの取得（キャッシュ）
    if (!virtualCursorObj_) {
        if (auto scene = GetScene()) {
            virtualCursorObj_ = scene->FindGameObject("VirtualCursor");
            if (virtualCursorObj_) {
                virtualCursorRenderer_ = virtualCursorObj_->GetComponent<Primitive2DRendererComponent>();
            }
        }
    }

    // 出撃シーケンス中または遊び方モーダル表示中はカーソルを隠す
    if (isLaunching_ || isHowToPlayOpen_) {
        if (virtualCursorRenderer_) {
            virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        }
        return;
    }

    // 1. カーソルがボタン上にホバーしているか判定（マグネット摩擦用）
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();
    bool isHoveringAnyButton = false;
    if (auto scene = GetScene()) {
        for (const auto& name : buttonNames_) {
            if (auto btnObj = scene->FindGameObject(name)) {
                if (auto transform = btnObj->GetComponent<TransformComponent>()) {
                    const auto& pos = transform->GetPosition();
                    if (std::abs(cursorPos.x - pos.x) <= 180.0f && std::abs(cursorPos.y - pos.y) <= 25.0f) {
                        isHoveringAnyButton = true;
                        break;
                    }
                }
            }
        }
    }

    // 2. InputManager に仮想カーソル更新を委譲（ホバー摩擦適用）
    float speedMult = isHoveringAnyButton ? kStickyFriction_ : 1.0f;
    inputManager->UpdateVirtualCursor(deltaTime, speedMult);

    // 3. 仮想カーソルオブジェクトの座標・表示状態を同期
    const auto& newPos = inputManager->GetVirtualCursorPosition();
    if (virtualCursorObj_) {
        if (auto trans = virtualCursorObj_->GetTransform()) {
            trans->SetPosition({newPos.x, newPos.y, 0.0f});
            float targetScale = isHoveringAnyButton ? 1.25f : 1.0f;
            trans->SetScale({targetScale, targetScale, 1.0f});
        }

        if (virtualCursorRenderer_) {
            // ゲームパッド操作中のみリングカーソルを表示し、物理マウス操作時はマウスカーソルに委ねる（非表示）
            if (inputManager->IsUsingGamepadCursor()) {
                virtualCursorRenderer_->SetColor(isHoveringAnyButton ? Irufemi::Vector4{0.2f, 1.0f, 0.95f, 1.0f}
                                                                     : Irufemi::Vector4{0.1f, 0.95f, 1.0f, 0.85f});
            } else {
                virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
    }
}
