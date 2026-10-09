#include "Scenes/title/TitleMenuControllerComponent.h"
#include "Scenes/title/State/TitleMenuStateIdle.h"
#include "Scenes/title/State/TitleMenuStateOpeningModal.h"
#include "Scenes/title/State/TitleMenuStateSuspended.h"
#include "Scenes/title/State/TitleMenuStateLaunching.h"

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
#include "UI/UISound.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Input/GameAction.h"
#include "Core/AttractDemoManager.h"
#include "Renderer/PostProcess/PostProcessManager.h"

#ifdef EditorMode
#include "Core/EditorManager.h"
#endif

#include <cmath>
#include <algorithm>

void TitleMenuControllerComponent::Initialize() {
    currentIndex_ = 0;
    pressedButtonIndex_ = -1;
    ChangeState(std::make_unique<TitleMenuStateIdle>());
    stateTimer_ = 0.0f;
    pendingModalSceneName_.clear();
    dismissTimer_ = 0.0f;
    stickCooldownTimer_ = 0.0f;

    virtualCursorObj_ = nullptr;
    virtualCursorRenderer_ = nullptr;

    currentScales_ = {1.15f, 1.0f, 1.0f, 1.0f};
    targetScales_ = {1.15f, 0.95f, 0.95f, 0.95f};

    flashTimer_ = 0.0f;
    titleBreatheTimer_ = 0.0f;

    // シーン開始時にポストプロセスのFadeモードが残留しないよう安全にクリア
    if (auto engine = GetEngine()) {
        if (auto pp = engine->GetPostProcessManager()) {
            pp->GetFadeParams().intensity = 0.0f;
            pp->RemoveActiveMode(PostProcessMode::Fade);
        }
    }
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

    // 全画面インパクト白光フラッシュ更新
    UpdateScreenFlash(deltaTime);

    // タイトルロゴの呼吸脈動・シアン発光パルス更新
    UpdateTitleTextVisual(deltaTime);

    // ステートマシン更新 (State Pattern: ポリモーフィズム呼び出し)
    if (currentState_) {
        currentState_->Update(this, deltaTime);
    }
}

TitleMenuControllerComponent::MenuState TitleMenuControllerComponent::GetState() const {
    if (currentState_) {
        return currentState_->GetStateType();
    }
    return MenuState::Idle;
}

void TitleMenuControllerComponent::ChangeState(std::unique_ptr<ITitleMenuState> newState) {
    if (currentState_) {
        currentState_->Exit(this);
    }
    currentState_ = std::move(newState);
    stateTimer_ = 0.0f;
    if (currentState_) {
        currentState_->Enter(this);
    }
}

void TitleMenuControllerComponent::ChangeState(MenuState newState) {
    using StateFactory = std::unique_ptr<ITitleMenuState>(*)();
    static const std::unordered_map<MenuState, StateFactory> kStateFactories = {
        { MenuState::Idle,         []() -> std::unique_ptr<ITitleMenuState> { return std::make_unique<TitleMenuStateIdle>(); } },
        { MenuState::OpeningModal, []() -> std::unique_ptr<ITitleMenuState> { return std::make_unique<TitleMenuStateOpeningModal>(); } },
        { MenuState::Suspended,    []() -> std::unique_ptr<ITitleMenuState> { return std::make_unique<TitleMenuStateSuspended>(); } },
        { MenuState::Launching,    []() -> std::unique_ptr<ITitleMenuState> { return std::make_unique<TitleMenuStateLaunching>(); } },
    };

    if (auto it = kStateFactories.find(newState); it != kStateFactories.end()) {
        ChangeState(it->second());
    }
}

bool TitleMenuControllerComponent::IsHowToPlayOpen() const {
    return GetState() == MenuState::Suspended && pendingModalSceneName_ == "HowToPlayScene";
}

bool TitleMenuControllerComponent::IsLaunching() const {
    return GetState() == MenuState::Launching;
}

void TitleMenuControllerComponent::SetLaunching(bool launching) {
    if (launching) {
        ChangeState(MenuState::Launching);
    } else if (GetState() == MenuState::Launching) {
        ChangeState(MenuState::Idle);
    }
}

bool TitleMenuControllerComponent::IsDismissing() const {
    return GetState() == MenuState::Launching;
}

void TitleMenuControllerComponent::ResetTargetScalesForCurrentIndex() {
    SetMenuVisible(true);
    for (size_t i = 0; i < targetScales_.size(); ++i) {
        targetScales_[i] = (static_cast<int>(i) == currentIndex_) ? 1.15f : 0.95f;
    }
}

void TitleMenuControllerComponent::UpdateStickCooldown(float dt) {
    if (stickCooldownTimer_ > 0.0f) {
        stickCooldownTimer_ -= dt;
    }
}

void TitleMenuControllerComponent::PushPendingModalScene() {
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            sm->PushScene(pendingModalSceneName_);
        }
    }
}

void TitleMenuControllerComponent::HideVirtualCursor() {
    if (virtualCursorRenderer_) {
        virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    }
}

void TitleMenuControllerComponent::TriggerLaunchSequence() {
    if (auto scene = GetScene()) {
        if (auto menuMgr = scene->FindGameObject("MenuManager")) {
            if (auto director = menuMgr->GetComponent<TitleSceneDirectorComponent>()) {
                director->StartLaunchSequence();
                return;
            }
        }
    }
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            sm->TransitionTo("InGame", SceneTransition::Type::Fade, 0.9f);
        }
    }
}

void TitleMenuControllerComponent::UpdateOpeningModalAnimation(float deltaTime) {
    stateTimer_ += deltaTime;
    float progress = std::clamp(stateTimer_ / kModalAnimDuration_, 0.0f, 1.0f);

    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // 1. 各ボタンのアニメーション
    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        auto obj = scene->FindGameObject(buttonNames_[i]);
        if (!obj) {
            continue;
        }

        auto transform = obj->GetTransform();
        auto text = obj->GetComponent<TextRendererComponent>();
        auto sprite = obj->GetComponent<SpriteRendererComponent>();

        if (static_cast<int>(i) == currentIndex_) {
            // 決定ボタン: 0.00〜0.055sで沈み込み(1.15->0.88)、0.055〜0.22sで弾性ポップ(0.88->1.15)
            float s;
            if (progress < 0.25f) {
                s = std::lerp(1.15f, 0.88f, progress / 0.25f);
            } else {
                float popT = (progress - 0.25f) / 0.75f;
                float ease = std::sin(popT * 3.14159265f * 0.5f);
                s = std::lerp(0.88f, 1.15f, ease);
            }

            if (transform && i < initialScales_.size()) {
                auto base = initialScales_[i];
                transform->SetScale({base.x * s, base.y * s, base.z});
            }

            // ネオンシアン強発光を維持
            if (text) {
                text->SetColor({0.1f, 0.95f, 1.0f, 1.0f});
            }
            if (sprite) {
                sprite->SetColor({0.2f, 0.95f, 1.0f, 1.0f});
            }
        } else {
            // 非選択ボタン: アルファをスッと滑らかにフェードアウト (0.65 -> 0.0)
            float fadeAlpha = 0.65f * (1.0f - progress);
            if (text) {
                text->SetColor({0.7f, 0.75f, 0.8f, fadeAlpha});
            }
            if (sprite) {
                sprite->SetColor({0.6f, 0.65f, 0.7f, fadeAlpha});
            }
        }
    }

    // 2. アニメーション完了時に Suspended 状態へ遷移（ExitStateでPushSceneが発火！）
    if (progress >= 1.0f) {
        ChangeState(MenuState::Suspended);
    }
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

    // デモ再生中（放置デモまたはF8キオスク固定展示）は手動キーボード・十字キー入力を遮断
    if (!AttractDemoManager::IsAttractModeActive()) {
        int moveDelta = 0;

        // キーボード / 十字キー
        bool isPadDpad = inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_UP) ||
                         inputManager->IsButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN);

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
            currentIndex_ = (currentIndex_ + moveDelta + static_cast<int>(buttonNames_.size())) %
                            static_cast<int>(buttonNames_.size());

            if (oldIndex != currentIndex_) {
                UISound::PlayCursor();

                // 選択されたボタンの位置へ仮想カーソルをスナップ同期（星雲トレイルおよびUI整合性の両立）
                if (auto scene = GetScene()) {
                    if (auto btn = scene->FindGameObject(buttonNames_[currentIndex_])) {
                        if (auto t = btn->GetTransform()) {
                            const auto& btnPos = t->GetPosition();
                            inputManager->SetVirtualCursorPosition({btnPos.x, btnPos.y});
                        }
                    }
                }

                // ゲームパッド十字キー操作時はリングカーソルを明瞭に表示
                if (isPadDpad) {
                    inputManager->SetIsUsingGamepadCursor(true);
                }

                // 目標スケールの更新
                for (size_t i = 0; i < targetScales_.size(); ++i) {
                    targetScales_[i] = (static_cast<int>(i) == currentIndex_) ? 1.15f : 0.95f;
                }
            }
            return;
        }
    }

    // --- 統合仮想カーソル（マウス / ゲームパッド左スティック）によるホバー検出 ---
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();
    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        if (IsCursorOverButton(static_cast<int>(i), cursorPos)) {
            if (currentIndex_ != static_cast<int>(i)) {
                currentIndex_ = static_cast<int>(i);
                UISound::PlayCursor();
                for (size_t k = 0; k < targetScales_.size(); ++k) {
                    targetScales_[k] = (static_cast<int>(k) == currentIndex_) ? 1.15f : 0.95f;
                }
            }
            break;
        }
    }
}

void TitleMenuControllerComponent::HandleSelectionInput() {
    // デモ再生中（放置デモまたはF8キオスク固定展示）は手動決定入力を遮断
    if (AttractDemoManager::IsAttractModeActive()) {
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto inputManager = engine->GetInputManager();
    if (!inputManager) {
        return;
    }

    // 1. キーボード / ゲームパッドによるフォーカス項目の決定（Space, Enter, またはPad_A）
    bool isSubmit = InputHelper::IsActionPressed(inputManager, GameAction::UI_Submit);

    // 2. マウス / 仮想カーソルによるボタン内クリック決定
    bool isCursorSubmit = false;
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();

    // 押下瞬間の検知（ボタン上で押下されたか追跡）
    if (inputManager->IsCursorActionPressed()) {
        pressedButtonIndex_ = -1;
        for (size_t i = 0; i < buttonNames_.size(); ++i) {
            if (IsCursorOverButton(static_cast<int>(i), cursorPos)) {
                pressedButtonIndex_ = static_cast<int>(i);
                currentIndex_ = static_cast<int>(i);
                isCursorSubmit = true; // 押下即時レスポンスを確保
                break;
            }
        }
    }

    // 解放瞬間の検知（ドラッグ外れでなく同一ボタン上でのクリック完了）
    if (inputManager->IsCursorActionReleased()) {
        if (pressedButtonIndex_ >= 0) {
            if (IsCursorOverButton(pressedButtonIndex_, cursorPos)) {
                currentIndex_ = pressedButtonIndex_;
                isCursorSubmit = true;
            }
            pressedButtonIndex_ = -1;
        }
    }

    if (isSubmit || isCursorSubmit) {
        ExecuteSelection();
    }
}

void TitleMenuControllerComponent::ExecuteSelection() {
    using MenuAction = void (*)(TitleMenuControllerComponent*);
    static const std::array<MenuAction, 4> kMenuActions = {
        [](TitleMenuControllerComponent* self) {
            self->ChangeState(MenuState::Launching);
        },
        [](TitleMenuControllerComponent* self) {
            self->pendingModalSceneName_ = "HowToPlayScene";
            self->ChangeState(MenuState::OpeningModal);
        },
        [](TitleMenuControllerComponent* self) {
            self->pendingModalSceneName_ = "OptionsScene";
            self->ChangeState(MenuState::OpeningModal);
        },
        [](TitleMenuControllerComponent* self) {
            UISound::PlayDecide();
#ifdef EditorMode
            if (auto editor = EditorManager::GetInstance()) {
                editor->RequestExitPlayMode();
                return;
            }
#endif
            PostQuitMessage(0);
        }
    };

    if (currentIndex_ >= 0 && static_cast<size_t>(currentIndex_) < kMenuActions.size()) {
        kMenuActions[static_cast<size_t>(currentIndex_)](this);
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

void TitleMenuControllerComponent::StartDismissAnimation() {
    dismissTimer_ = 0.0f;

    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // 各ボタンの初期座標をキャッシュ
    dismissStartPositions_.clear();
    for (const auto& name : buttonNames_) {
        if (auto obj = scene->FindGameObject(name)) {
            if (auto t = obj->GetTransform()) {
                dismissStartPositions_.push_back(t->GetPosition());
                continue;
            }
        }
        dismissStartPositions_.push_back({640.0f, 360.0f, 0.0f});
    }

    // タイトルロゴの初期座標をキャッシュ
    if (auto titleObj = scene->FindGameObject("TitleText")) {
        if (auto t = titleObj->GetTransform()) {
            titleTextStartPos_ = t->GetPosition();
        }
    }

    // 仮想カーソルを即座に非表示
    if (virtualCursorRenderer_) {
        virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    }
}

void TitleMenuControllerComponent::UpdateDismissAnimation(float deltaTime) {
    dismissTimer_ += deltaTime;
    float t = std::clamp(dismissTimer_ / kDismissDuration_, 0.0f, 1.0f);
    float alpha = 1.0f - t;

    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // 1. 各ボタンのディゾルブ（GAME STARTは白発光、他は外側へ拡散透過）
    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        auto obj = scene->FindGameObject(buttonNames_[i]);
        if (!obj) {
            continue;
        }

        auto transform = obj->GetTransform();
        auto text = obj->GetComponent<TextRendererComponent>();

        if (static_cast<int>(i) == currentIndex_) {
            // 決定された GAME START: 白く純光フラッシュしながらスケール拡大
            if (text) {
                text->SetColor({std::lerp(0.1f, 1.0f, t), 1.0f, 1.0f, alpha});
            }
            if (transform && i < initialScales_.size()) {
                float popScale;
                if (t < 0.20f) {
                    // [0.00s 〜 0.06s]: 物理的な押し込み（クリック沈み込み）
                    popScale = std::lerp(1.15f, 0.90f, t / 0.20f);
                } else {
                    // [0.06s 〜 0.30s]: 弾けるような拡大拡散
                    popScale = std::lerp(0.90f, 1.35f, (t - 0.20f) / 0.80f);
                }
                auto base = initialScales_[i];
                transform->SetScale({base.x * popScale, base.y * popScale, base.z});
            }
        } else {
            // 非選択項目: 上下へ拡散しながら急速に透明化
            if (text) {
                text->SetColor({0.7f, 0.75f, 0.8f, 0.65f * alpha});
            }
            if (transform && i < dismissStartPositions_.size()) {
                float spreadY = (static_cast<int>(i) < currentIndex_) ? -40.0f * t : 40.0f * t;
                const auto& startPos = dismissStartPositions_[i];
                transform->SetPosition({startPos.x, startPos.y + spreadY, startPos.z});
            }
        }
    }

    // 2. タイトルロゴのディゾルブ（上方へフワッと透過消退）
    if (auto titleObj = scene->FindGameObject("TitleText")) {
        if (auto text = titleObj->GetComponent<TextRendererComponent>()) {
            text->SetColor({0.2f, 0.92f, 1.0f, alpha});
        }
        if (auto transform = titleObj->GetTransform()) {
            transform->SetPosition({titleTextStartPos_.x, titleTextStartPos_.y - 35.0f * t, titleTextStartPos_.z});
        }
    }

    // アニメーション完了時に非表示化
    if (dismissTimer_ >= kDismissDuration_) {
        SetMenuVisible(false);
    }
}

void TitleMenuControllerComponent::SetMenuVisible(bool visible) {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // タイトルロゴの表示/非表示＆リセット
    if (auto titleObj = scene->FindGameObject("TitleText")) {
        titleObj->SetActive(visible);
        if (visible) {
            if (auto text = titleObj->GetComponent<TextRendererComponent>()) {
                text->SetColor({0.2f, 0.92f, 1.0f, 1.0f});
            }
            if (auto t = titleObj->GetTransform()) {
                t->SetPosition({640.0f, 150.0f, 0.0f});
            }
        }
    }

    // 各ボタン項目の表示/非表示＆リセット
    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        if (auto btnObj = scene->FindGameObject(buttonNames_[i])) {
            btnObj->SetActive(visible);
            if (visible && i < dismissStartPositions_.size()) {
                if (auto t = btnObj->GetTransform()) {
                    t->SetPosition(dismissStartPositions_[i]);
                }
            }
        }
    }

    // 仮想カーソルの表示/非表示
    if (virtualCursorObj_) {
        virtualCursorObj_->SetActive(visible);
    }
}

void TitleMenuControllerComponent::RestoreFocusOnResume() {
    pressedButtonIndex_ = -1;
    pendingModalSceneName_.clear();
    dismissTimer_ = 0.0f;

    // Idle状態へ復帰（メニューUIの再表示とフォーカススケール設定を実行）
    ChangeState(MenuState::Idle);

    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto inputManager = engine->GetInputManager();
    if (!inputManager) {
        return;
    }

    // 選択中ボタンの座標へ仮想カーソルをピタッと同期復帰
    if (auto scene = GetScene()) {
        if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(buttonNames_.size())) {
            if (auto btn = scene->FindGameObject(buttonNames_[currentIndex_])) {
                if (auto t = btn->GetTransform()) {
                    const auto& btnPos = t->GetPosition();
                    inputManager->SetVirtualCursorPosition({btnPos.x, btnPos.y});
                }
            }
        }
    }
}

void TitleMenuControllerComponent::TriggerScreenFlash() {
    flashTimer_ = kFlashDuration_;
    if (auto engine = GetEngine()) {
        if (auto pp = engine->GetPostProcessManager()) {
            pp->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PostUI);
            auto& fade = pp->GetFadeParams();
            fade.color = {1.0f, 1.0f, 1.0f, 1.0f};
            fade.intensity = 0.75f;
        }
    }
}

void TitleMenuControllerComponent::UpdateScreenFlash(float deltaTime) {
    if (flashTimer_ <= 0.0f) {
        return;
    }

    flashTimer_ -= deltaTime;
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto pp = engine->GetPostProcessManager();
    if (!pp) {
        return;
    }

    if (flashTimer_ <= 0.0f) {
        // フラッシュ終了: モードを解除し強度を完全ゼロ化
        pp->GetFadeParams().intensity = 0.0f;
        pp->RemoveActiveMode(PostProcessMode::Fade);
    } else {
        float t = std::clamp(flashTimer_ / kFlashDuration_, 0.0f, 1.0f);
        float alpha = t * t * 0.75f; // 2次減衰で鋭い光のキレ味を表現
        pp->GetFadeParams().intensity = alpha;
    }
}

void TitleMenuControllerComponent::UpdateTitleTextVisual(float deltaTime) {
    if (GetState() != MenuState::Idle) {
        return; // 出撃・演出・待機中はそちらのフェード制御に委ねる
    }

    titleBreatheTimer_ += deltaTime;

    auto scene = GetScene();
    if (!scene) {
        return;
    }

    if (auto titleObj = scene->FindGameObject("TitleText")) {
        // 1. 呼吸スケール (周期約2.4秒、1.00〜1.025倍の穏やかな浮遊呼吸)
        float breathe = 1.0f + std::sin(titleBreatheTimer_ * 2.4f) * 0.015f;
        if (auto t = titleObj->GetTransform()) {
            t->SetScale({breathe, breathe, 1.0f});
        }

        // 2. 定期的なシアン発光パルス (周期3.5秒、約0.25秒間の輝度ブースト)
        float pulsePhase = std::fmod(titleBreatheTimer_, 3.5f);
        float glow = 0.0f;
        if (pulsePhase < 0.25f) {
            glow = std::sin((pulsePhase / 0.25f) * 3.14159265f) * 0.35f;
        }

        if (auto text = titleObj->GetComponent<TextRendererComponent>()) {
            text->SetColor({std::clamp(0.20f + glow * 0.40f, 0.0f, 1.0f), std::clamp(0.92f + glow * 0.08f, 0.0f, 1.0f),
                            1.0f, 1.0f});
        }
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

    // 通常待機(Idle)時以外はカーソルを隠す
    if (GetState() != MenuState::Idle) {
        if (virtualCursorRenderer_) {
            virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        }
        return;
    }

    // 1. カーソルがボタン上にホバーしているか判定（マグネット摩擦用）
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();
    bool isHoveringAnyButton = false;
    for (size_t i = 0; i < buttonNames_.size(); ++i) {
        if (IsCursorOverButton(static_cast<int>(i), cursorPos)) {
            isHoveringAnyButton = true;
            break;
        }
    }

    // 2. InputManager に仮想カーソル更新を委譲（※ デモ中以外のみ実行）
    if (!AttractDemoManager::IsAttractModeActive()) {
        float speedMult = isHoveringAnyButton ? kStickyFriction_ : 1.0f;
        inputManager->UpdateVirtualCursor(deltaTime, speedMult);
    }

    // 3. 仮想カーソルオブジェクトの座標・表示状態を同期
    const auto& newPos = inputManager->GetVirtualCursorPosition();
    if (virtualCursorObj_) {
        bool isAttractDemo = AttractDemoManager::IsAttractModeActive();
        bool showCursor = inputManager->IsUsingGamepadCursor() || isAttractDemo;

        if (auto trans = virtualCursorObj_->GetTransform()) {
            trans->SetPosition({newPos.x, newPos.y, 0.0f});

            float baseTargetScale = isHoveringAnyButton ? 1.30f : 1.0f;
            if (isAttractDemo && isHoveringAnyButton) {
                // デモ中のホバー時は微細な呼吸パルスで「フォーカス中」を強調
                baseTargetScale += std::sin(titleBreatheTimer_ * 6.0f) * 0.08f;
            }

            const auto& curScale = trans->GetScale();
            float smoothScale = std::lerp(curScale.x, baseTargetScale, std::clamp(deltaTime * 14.0f, 0.0f, 1.0f));
            trans->SetScale({smoothScale, smoothScale, 1.0f});
        }

        if (virtualCursorRenderer_) {
            if (showCursor) {
                // ゲームパッド操作中またはデモ実演中はリングカーソルを明瞭に表示
                if (isHoveringAnyButton) {
                    virtualCursorRenderer_->SetColor({0.30f, 1.0f, 0.98f, 1.0f});
                } else {
                    virtualCursorRenderer_->SetColor({0.10f, 0.92f, 1.0f, 0.85f});
                }
            } else {
                // 通常のマウス操作時はOSカーソルに委ねるため非表示
                virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
    }
}

bool TitleMenuControllerComponent::IsCursorOverButton(int index, const Irufemi::Vector2& cursorPos) const {
    if (index < 0 || index >= static_cast<int>(buttonNames_.size())) {
        return false;
    }

    auto scene = GetScene();
    if (!scene) {
        return false;
    }

    auto btnObj = scene->FindGameObject(buttonNames_[index]);
    if (!btnObj) {
        return false;
    }

    auto transform = btnObj->GetComponent<TransformComponent>();
    if (!transform) {
        return false;
    }

    const auto& pos = transform->GetPosition();
    const auto& scale = transform->GetScale();

    // 1. TextRendererComponent によるテキストメッシュ幾何バウンディングボックスの算出
    auto textComp = btnObj->GetComponent<TextRendererComponent>();
    if (textComp) {
        const auto& minB = textComp->GetLocalBoundsMin();
        const auto& maxB = textComp->GetLocalBoundsMax();

        // テキストバウンディングボックスが正常に算出されている場合
        if (minB.x != maxB.x && minB.y != maxB.y) {
            // 操作感を快適にするためのパディング（UX Hitbox Padding）
            // 拡大時（1.15倍）にも自然に吸い付き、クリックが外れにくくなるよう余裕を付与
            const float kPaddingX = 24.0f;
            const float kPaddingY = 12.0f;

            float left = pos.x + (minB.x - kPaddingX) * scale.x;
            float right = pos.x + (maxB.x + kPaddingX) * scale.x;
            float top = pos.y + (minB.y - kPaddingY) * scale.y;
            float bottom = pos.y + (maxB.y + kPaddingY) * scale.y;

            return (cursorPos.x >= left && cursorPos.x <= right && cursorPos.y >= top && cursorPos.y <= bottom);
        }
    }

    // 2. フォールバック（文字サイズ未計算またはスプライトのみの場合）
    // ボタンの文字長に応じた基準半幅 × 現在のスケール
    constexpr std::array<float, 4> kBaseHalfWidths = {
        120.0f, // 0: START
        210.0f, // 1: HOW TO PLAY
        160.0f, // 2: OPTIONS
        100.0f  // 3: QUIT
    };
    float baseHalfW = 160.0f;
    if (index >= 0 && static_cast<size_t>(index) < kBaseHalfWidths.size()) {
        baseHalfW = kBaseHalfWidths[static_cast<size_t>(index)];
    }

    float halfW = baseHalfW * scale.x;
    float halfH = 28.0f * scale.y;

    return (std::abs(cursorPos.x - pos.x) <= halfW && std::abs(cursorPos.y - pos.y) <= halfH);
}
