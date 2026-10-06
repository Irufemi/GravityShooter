#include "Core/AttractDemoManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Platform/Input/InputManager.h"
#include "Platform/Input/Mouse.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/TitleMenuControllerComponent.h"
#include <algorithm>
#include <cmath>

namespace {
// 2次ベジェ曲線補間
inline Irufemi::Vector2 EvaluateQuadraticBezier(const Irufemi::Vector2& p0, const Irufemi::Vector2& p1,
                                                const Irufemi::Vector2& p2, float t) {
    float u = 1.0f - t;
    return {u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x,
            u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y};
}

// 3次エルミート補間（SmoothStep）
inline float SmoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

void AttractDemoManager::OnInitialize(IrufemiEngine* engine) {
    engine_ = engine;
    previousScene_ = "";
    isKioskLoopMode_ = false;
    isDemoPlaying_ = false;
    isDemoInGame_ = false;
    hasSubmitted_ = false;
    hasTriggeredReturn_ = false;
    wasF8Down_ = false;
    f8CooldownTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    demoTimeline_ = 0.0f;
    inGameTimer_ = 0.0f;
    hasInitializedMousePos_ = false;
}

void AttractDemoManager::OnFinalize() {
    StopAllDemo(false);
    engine_ = nullptr;
}

void AttractDemoManager::OnUpdate(float deltaTime) {
    if (!engine_) {
        return;
    }

    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    // 1. F8キーの監視（全シーン共通・最優先処理、多重発火防止クールダウン適用）
    UpdateF8Input(deltaTime);

    auto sm = engine_->GetSceneManager();
    if (!sm) {
        return;
    }

    const std::string& currentScene = sm->GetCurrent();
    bool sceneChanged = (currentScene != previousScene_);

    // 2. シーン遷移（Enter）時の初期化処理
    if (sceneChanged) {
        if (currentScene == "InGame") {
            // Title から本編 InGame に突入した瞬間
            inGameTimer_ = 0.0f;
            hasTriggeredReturn_ = false;
            isDemoPlaying_ = false; // タイトル用デモフラグは終了
        } else if (currentScene == "Title") {
            // InGame から Title に帰還した瞬間
            isDemoInGame_ = false;
            hasTriggeredReturn_ = false;
            demoTimeline_ = 0.0f;
            idleTimer_ = 0.0f;
        }
        previousScene_ = currentScene;
    }

    // 3. 現在のシーンに応じた個別処理
    if (currentScene == "Title") {
        UpdateTitleScene(deltaTime);
    } else if (currentScene == "InGame") {
        UpdateInGameScene(deltaTime);
    } else {
        // その他のシーン（OptionsやHowToPlay等）では無操作タイマーをリセット
        idleTimer_ = 0.0f;
    }
}

void AttractDemoManager::UpdateF8Input(float deltaTime) {
    if (f8CooldownTimer_ > 0.0f) {
        f8CooldownTimer_ -= deltaTime;
    }

    auto inputManager = engine_->GetInputManager();

    // 物理的に今キーが押されているかを判定 (0x8000 = 現在物理的に押下中フラグ)
    bool isF8Down = false;
    if (inputManager && inputManager->IsKeyDown(kToggleKey_)) {
        isF8Down = true;
    }
    if ((::GetAsyncKeyState(kToggleKey_) & 0x8000) != 0) {
        isF8Down = true;
    }

    // 立ち上がりエッジ検知（前フレームOFF かつ 今フレームONの瞬間のみ）
    bool f8Triggered = isF8Down && !wasF8Down_;
    wasF8Down_ = isF8Down;

    // クールダウン中、または押された瞬間でなければ無視（多重発火を完全に防止）
    if (!f8Triggered || f8CooldownTimer_ > 0.0f) {
        return;
    }

    // 多重発火ガード開始（300ms）
    f8CooldownTimer_ = kF8CooldownDuration_;

    // すでにデモ中（放置デモ・出撃中・InGame中含む）またはキオスクループ中の場合は【完全停止】
    if (isKioskLoopMode_ || isDemoPlaying_ || isDemoInGame_) {
        StopAllDemo(true);
    } else {
        // 通常待機中の場合は【キオスク固定デモ開始】
        isKioskLoopMode_ = true;
        auto sm = engine_->GetSceneManager();
        if (sm && sm->GetCurrent() == "Title") {
            StartTitleDemo();
        } else if (sm) {
            sm->TransitionTo("Title", SceneTransition::Type::Fade, 0.3f);
        }
    }
}

void AttractDemoManager::UpdateTitleScene(float deltaTime) {
    auto inputManager = engine_->GetInputManager();
    if (!inputManager) {
        return;
    }

    // キオスクループ中かつデモ未開始なら即始動
    if (isKioskLoopMode_ && !isDemoPlaying_) {
        StartTitleDemo();
    }

    auto sm = engine_->GetSceneManager();
    auto currentScenePtr = sm ? dynamic_cast<BaseScene*>(sm->GetCurrentScene()) : nullptr;

    // 出撃演出中かどうかをチェック
    TitleSceneDirectorComponent* dirComp = nullptr;
    if (currentScenePtr) {
        if (auto menuMgr = currentScenePtr->FindGameObject("MenuManager")) {
            dirComp = menuMgr->GetComponent<TitleSceneDirectorComponent>();
        }
        if (!dirComp) {
            for (auto& obj : currentScenePtr->GetGameObjects()) {
                if (obj && (dirComp = obj->GetComponent<TitleSceneDirectorComponent>())) {
                    break;
                }
            }
        }
    }

    if (dirComp && dirComp->IsLaunching()) {
        // 出撃シーケンス実行中は演出を見守るためタイムライン更新を待機
        return;
    }

    // デモ再生中の場合
    if (isDemoPlaying_) {
        // キオスク固定モードでない場合は手動入力で即座にデモ解除
        if (!isKioskLoopMode_) {
            Irufemi::Vector2 rawMousePos = inputManager->GetVirtualCursorPosition();
            if (auto mouse = inputManager->GetMouse()) {
                rawMousePos = mouse->GetPosition();
            }
            float mouseMoveDist = std::hypot(rawMousePos.x - lastRawMousePos_.x, rawMousePos.y - lastRawMousePos_.y);
            lastRawMousePos_ = rawMousePos;

            if (mouseMoveDist > 3.0f || inputManager->IsCursorActionPressed() || inputManager->IsKeyPressed(VK_SPACE) ||
                inputManager->IsKeyPressed(VK_RETURN) || inputManager->IsKeyPressed(VK_ESCAPE)) {
                StopTitleDemo();
                return;
            }
        }

        demoTimeline_ += deltaTime;

        Irufemi::Vector2 targetCursorPos = posSweepStart_;

        // 【Phase 1】 0.00s 〜 2.50s : 空間の歪み＆ガレキ反発スイープ実演
        if (demoTimeline_ < 2.50f) {
            float t = SmoothStep(demoTimeline_ / 2.50f);
            targetCursorPos = EvaluateQuadraticBezier(posSweepStart_, posSweepMid_, posSweepEnd_, t);
        }
        // 【Phase 2-1】 2.50s 〜 3.30s : 「HOW TO PLAY」へ移動（ボタン拡大ホバー）
        else if (demoTimeline_ < 3.30f) {
            float t = SmoothStep((demoTimeline_ - 2.50f) / 0.80f);
            targetCursorPos.x = std::lerp(posSweepEnd_.x, posHowToPlay_.x, t);
            targetCursorPos.y = std::lerp(posSweepEnd_.y, posHowToPlay_.y, t);
        }
        // 【Phase 2-2】 3.30s 〜 4.00s : 「OPTIONS」へ移動（ボタン拡大縮小の行き来）
        else if (demoTimeline_ < 4.00f) {
            float t = SmoothStep((demoTimeline_ - 3.30f) / 0.70f);
            targetCursorPos.x = std::lerp(posHowToPlay_.x, posOptions_.x, t);
            targetCursorPos.y = std::lerp(posHowToPlay_.y, posOptions_.y, t);
        }
        // 【Phase 2-3】 4.00s 〜 4.70s : 「GAME START」へスッと吸い寄せ
        else if (demoTimeline_ < 4.70f) {
            float t = SmoothStep((demoTimeline_ - 4.00f) / 0.70f);
            targetCursorPos.x = std::lerp(posOptions_.x, posStart_.x, t);
            targetCursorPos.y = std::lerp(posOptions_.y, posStart_.y, t);
        }
        // 【Phase 3】 4.70s 〜 : GAME START クリック決定 ＆ 出撃演出発火
        else {
            targetCursorPos = posStart_;

            if (!hasSubmitted_) {
                hasSubmitted_ = true;

                if (currentScenePtr) {
                    if (auto menuMgr = currentScenePtr->FindGameObject("MenuManager")) {
                        if (auto menuCtrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
                            menuCtrl->StartDismissAnimation();
                            menuCtrl->SetLaunching(true);
                        }
                        if (dirComp) {
                            isDemoInGame_ = true; // InGame側での帰還を待機
                            dirComp->SetNextSceneName("InGame"); // 本編 InGame シーンへ直接出撃！
                            dirComp->StartLaunchSequence();
                        }
                    }
                }
            }
        }

        // 仮想カーソル座標を反映（星雲シェーダー、ガレキ反発、ボタンホバーが完全連動）
        inputManager->SetVirtualCursorPosition(targetCursorPos);
        if (auto mouse = inputManager->GetMouse()) {
            mouse->SetVirtualPosition(targetCursorPos, true);
        }
    }
    // 通常待機中の場合（無操作アイドル監視）
    else {
        Irufemi::Vector2 rawMousePos = inputManager->GetVirtualCursorPosition();
        if (auto mouse = inputManager->GetMouse()) {
            rawMousePos = mouse->GetPosition();
        }

        if (!hasInitializedMousePos_) {
            lastRawMousePos_ = rawMousePos;
            hasInitializedMousePos_ = true;
        }

        float mouseMoveDist = std::hypot(rawMousePos.x - lastRawMousePos_.x, rawMousePos.y - lastRawMousePos_.y);
        lastRawMousePos_ = rawMousePos;

        bool hasUserInput = (mouseMoveDist > 2.0f) || inputManager->IsCursorActionPressed() ||
                             inputManager->IsKeyPressed(VK_SPACE) || inputManager->IsKeyPressed(VK_RETURN) ||
                             inputManager->IsKeyPressed('W') || inputManager->IsKeyPressed('S') ||
                             inputManager->IsKeyPressed('A') || inputManager->IsKeyPressed('D') ||
                             inputManager->IsKeyPressed(VK_UP) || inputManager->IsKeyPressed(VK_DOWN);

        if (hasUserInput) {
            idleTimer_ = 0.0f;
        } else {
            idleTimer_ += deltaTime;
            if (idleTimer_ >= kIdleThreshold_) {
                StartTitleDemo();
            }
        }
    }
}

void AttractDemoManager::UpdateInGameScene(float deltaTime) {
    if (hasTriggeredReturn_) {
        return;
    }

    // デモによる出撃中（またはF8キオスクループ中）でなければ通常プレイなので何もしない
    if (!isDemoInGame_ && !isKioskLoopMode_) {
        return;
    }

    auto sm = engine_->GetSceneManager();
    if (!sm) {
        return;
    }

    // フェードイン（ロード・暗転明け）が完了するまではタイマー進行・スキップを待機
    // （フェードイン中に TransitionTo を呼ぶと SceneManager の二重遷移ガードに弾かれて戻れなくなるのを防止）
    if (sm->IsLoading() || sm->IsTransitioning()) {
        return;
    }

    inGameTimer_ += deltaTime;

    auto inputManager = engine_->GetInputManager();

    // スキップ判定（画面表示後 0.30秒以降、クリックまたは決定キーによる即時復帰）
    bool userSkip = false;
    if (inputManager && inGameTimer_ > 0.30f) {
        if (inputManager->IsCursorActionPressed() || inputManager->IsKeyPressed(VK_SPACE) ||
            inputManager->IsKeyPressed(VK_RETURN) || inputManager->IsKeyPressed(VK_ESCAPE)) {
            userSkip = true;
        }
    }

    if (userSkip) {
        // 手動スキップされた場合はキオスクループも解除して通常タイトルに戻す
        StopAllDemo(true);
        return;
    }

    // タイムアウト判定（画面が明るくなってから 3.5秒実機映像を魅せて自動フェード帰還）
    if (inGameTimer_ >= kInGameDuration_) {
        hasTriggeredReturn_ = true;
        isDemoInGame_ = false;
        sm->TransitionTo("Title", SceneTransition::Type::Fade, 0.8f);
    }
}

void AttractDemoManager::StartTitleDemo() {
    isDemoPlaying_ = true;
    demoTimeline_ = 0.0f;
    hasSubmitted_ = false;

    // 各ボタンの最新スクリーン座標を取得・キャッシュ
    posStart_ = GetButtonCenter("Btn_Start", {640.0f, 420.0f});
    posHowToPlay_ = GetButtonCenter("Btn_HowToPlay", {640.0f, 490.0f});
    posOptions_ = GetButtonCenter("Btn_Options", {640.0f, 560.0f});

    // 初期仮想カーソル位置を設定
    if (auto input = engine_->GetInputManager()) {
        input->SetVirtualCursorPosition(posSweepStart_);
        if (auto mouse = input->GetMouse()) {
            mouse->SetVirtualPosition(posSweepStart_, true);
            lastRawMousePos_ = mouse->GetPosition();
        }
    }
}

void AttractDemoManager::StopTitleDemo() {
    isDemoPlaying_ = false;
    idleTimer_ = 0.0f;
    demoTimeline_ = 0.0f;
    hasSubmitted_ = false;

    // 仮想マウスの上書きを解除して手動マウスへ復帰
    if (auto input = engine_->GetInputManager()) {
        if (auto mouse = input->GetMouse()) {
            mouse->SetVirtualPosition({0.0f, 0.0f}, false);
        }
    }
}

void AttractDemoManager::StopAllDemo(bool returnToTitle) {
    isKioskLoopMode_ = false;
    isDemoPlaying_ = false;
    isDemoInGame_ = false;
    hasSubmitted_ = false;
    idleTimer_ = 0.0f;
    demoTimeline_ = 0.0f;
    inGameTimer_ = 0.0f;

    // 仮想マウス解除
    if (engine_) {
        if (auto input = engine_->GetInputManager()) {
            if (auto mouse = input->GetMouse()) {
                mouse->SetVirtualPosition({0.0f, 0.0f}, false);
            }
        }
    }

    if (!returnToTitle || !engine_) {
        return;
    }

    auto sm = engine_->GetSceneManager();
    if (!sm) {
        return;
    }

    const std::string& currentScene = sm->GetCurrent();
    if (currentScene != "Title") {
        hasTriggeredReturn_ = true;
        sm->TransitionTo("Title", SceneTransition::Type::Fade, 0.4f);
    } else {
        // タイトル画面で出撃中にデモ停止された場合、タイトルを再初期化して通常待機に戻す
        auto currentScenePtr = dynamic_cast<BaseScene*>(sm->GetCurrentScene());
        if (currentScenePtr) {
            TitleSceneDirectorComponent* dirComp = nullptr;
            if (auto menuMgr = currentScenePtr->FindGameObject("MenuManager")) {
                dirComp = menuMgr->GetComponent<TitleSceneDirectorComponent>();
            }
            if (dirComp && dirComp->IsLaunching()) {
                sm->TransitionTo("Title", SceneTransition::Type::Fade, 0.2f);
            }
        }
    }
}

Irufemi::Vector2 AttractDemoManager::GetButtonCenter(const std::string& btnName,
                                                     const Irufemi::Vector2& fallbackPos) const {
    if (!engine_) {
        return fallbackPos;
    }
    if (auto sm = engine_->GetSceneManager()) {
        if (auto scene = dynamic_cast<BaseScene*>(sm->GetCurrentScene())) {
            if (auto btnObj = scene->FindGameObject(btnName)) {
                if (auto t = btnObj->GetComponent<TransformComponent>()) {
                    const auto& pos = t->GetPosition();
                    return {pos.x, pos.y};
                }
            }
        }
    }
    return fallbackPos;
}
