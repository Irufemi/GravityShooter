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
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "UI/UISound.h"
#include <algorithm>
#include <cmath>

namespace {
// 2次ベジェ曲線補間
inline Irufemi::Vector2 EvaluateQuadraticBezier(const Irufemi::Vector2& p0, const Irufemi::Vector2& p1,
                                                const Irufemi::Vector2& p2, float t) {
    float u = 1.0f - t;
    return {u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x, u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y};
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
    s_isKioskModeActive_ = false;
    s_isAttractModeActive_ = false;
}

void AttractDemoManager::OnFinalize() {
    StopAllDemo(false);
    CleanupDemoHud();
    s_isKioskModeActive_ = false;
    s_isAttractModeActive_ = false;
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

    // 静的フラグの同期更新（ゲームコンポーネントからの入力遮断判定用）
    s_isKioskModeActive_ = isKioskLoopMode_;
    s_isAttractModeActive_ = (isKioskLoopMode_ || isDemoPlaying_ || isDemoInGame_);

    auto sm = engine_->GetSceneManager();
    if (!sm) {
        return;
    }

    const std::string& currentScene = sm->GetCurrent();
    bool sceneChanged = (currentScene != previousScene_);

    // 2. シーン遷移（Enter）時の初期化処理
    if (sceneChanged) {
        demoHudObj_.reset();
        demoHudText_ = nullptr;

        if (currentScene == "InGame") {
            // Title から本編 InGame に突入した瞬間
            inGameTimer_ = 0.0f;
            hasTriggeredReturn_ = false;
            isDemoPlaying_ = false; // タイトル用デモフラグは終了

            // ゲームシーンへの入力リーク防止：仮想マウスを即座にサニタイズ（手動マウス復帰）
            if (auto input = engine_->GetInputManager()) {
                if (auto mouse = input->GetMouse()) {
                    mouse->SetVirtualPosition({0.0f, 0.0f}, false);
                }
            }
        } else if (currentScene == "Title") {
            // InGame から Title に帰還した瞬間
            isDemoInGame_ = false;
            hasTriggeredReturn_ = false;
            demoTimeline_ = 0.0f;
            idleTimer_ = 0.0f;

            if (auto input = engine_->GetInputManager()) {
                if (auto mouse = input->GetMouse()) {
                    mouse->SetVirtualPosition({0.0f, 0.0f}, false);
                }
            }
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

    // 4. デモ案内HUD（AUTO DEMO / DEMO LOOP）の動的描画・点滅更新
    UpdateDemoHud(deltaTime);
}

void AttractDemoManager::UpdateF8Input(float deltaTime) {
    if (f8CooldownTimer_ > 0.0f) {
        f8CooldownTimer_ -= deltaTime;
    }

    auto inputManager = engine_->GetInputManager();
    if (!inputManager) {
        return;
    }

    // InputManager の状態と Win32 GetAsyncKeyState の両面でF8キー押下を確実に検知
    SHORT asyncState = GetAsyncKeyState(VK_F8);
    bool isF8Down = (asyncState & 0x8000) != 0;
    if (inputManager->IsKeyPressed(VK_F8) || inputManager->IsKeyDown(VK_F8)) {
        isF8Down = true;
    }

    // 物理的なキー押下エッジ判定（前フレームOFF → 今フレームON）
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
        // キオスク固定モードでない場合は手動入力で即座にデモ解除（Wake-up on any input）
        if (!isKioskLoopMode_) {
            if (inputManager->HasAnyInput()) {
                StopTitleDemo();
                return;
            }
        }

        demoTimeline_ += deltaTime;

        Irufemi::Vector2 targetCursorPos = posSweepStart_;

        // 【Phase 1】 0.00s 〜 4.20s : S字重力スラローム＆星雲歪み・パルス実演
        if (demoTimeline_ < 1.40f) {
            // 区間1 (0.00s〜1.40s): 画面下部から右ガレキ密集エリアへ鋭く加速侵入（星雲引き波が大きく伸びる）
            float t = SmoothStep(demoTimeline_ / 1.40f);
            targetCursorPos.x = std::lerp(posSweepStart_.x, posSweepRight_.x, t);
            targetCursorPos.y = std::lerp(posSweepStart_.y, posSweepRight_.y, t);
        } else if (demoTimeline_ < 2.80f) {
            // 区間2 (1.40s〜2.80s): 右ガレキから自機頭上を経由し、左ガレキへS字クロス通過
            float t = (demoTimeline_ - 1.40f) / 1.40f;
            targetCursorPos = EvaluateQuadraticBezier(posSweepRight_, posSweepCenter_, posSweepLeft_, t);

            // 自機頭上クロス（t ≒ 0.5 付近）で重力光彩パルス ＆ 全ガレキ一斉衝撃波を発火！
            if (t >= 0.45f && !hasTriggeredDemoPulse_) {
                hasTriggeredDemoPulse_ = true;
                if (dirComp) {
                    if (auto nebulaComp = dirComp->GetNebulaComponent()) {
                        nebulaComp->TriggerPulse(1.0f);
                    }
                    dirComp->TriggerGravitationalShockwave(1.0f);
                }
            }
        } else if (demoTimeline_ < 4.20f) {
            // 区間3 (2.80s〜4.20s): 左ガレキからメニュー「HOW TO PLAY」へ滑らかに吸い寄せ合流
            float t = SmoothStep((demoTimeline_ - 2.80f) / 1.40f);
            targetCursorPos.x = std::lerp(posSweepLeft_.x, posHowToPlay_.x, t);
            targetCursorPos.y = std::lerp(posSweepLeft_.y, posHowToPlay_.y, t);
        }
        // 【Phase 2-1】 4.20s 〜 5.60s : 「HOW TO PLAY」上でピタッと静止ホバー（拡大演出をじっくり魅せる）
        else if (demoTimeline_ < 5.60f) {
            targetCursorPos = posHowToPlay_;
        }
        // 【Phase 2-2】 5.60s 〜 7.00s : 「OPTIONS」へ移動し、ピタッと静止ホバー（UI行き来の心地よさ）
        else if (demoTimeline_ < 7.00f) {
            float t = SmoothStep(std::clamp((demoTimeline_ - 5.60f) / 0.50f, 0.0f, 1.0f));
            targetCursorPos.x = std::lerp(posHowToPlay_.x, posOptions_.x, t);
            targetCursorPos.y = std::lerp(posHowToPlay_.y, posOptions_.y, t);
            // 6.10s 〜 7.00s の間は posOptions_ 上で完全静止
        }
        // 【Phase 2-3】 7.00s 〜 8.00s : 本命「GAME START」へスッと吸い寄せ
        else if (demoTimeline_ < 8.00f) {
            float t = SmoothStep(std::clamp((demoTimeline_ - 7.00f) / 0.60f, 0.0f, 1.0f));
            targetCursorPos.x = std::lerp(posOptions_.x, posStart_.x, t);
            targetCursorPos.y = std::lerp(posOptions_.y, posStart_.y, t);
        }
        // 【Phase 2-4】 8.00s 〜 8.60s : STARTボタン上で一瞬のタメ（呼吸・出撃への期待感）
        else if (demoTimeline_ < 8.60f) {
            targetCursorPos = posStart_;
        }
        // 【Phase 3】 8.60s 〜 : GAME START クリック決定 ＆ 出撃演出発火
        else {
            targetCursorPos = posStart_;

            if (!hasSubmitted_) {
                hasSubmitted_ = true;
                UISound::PlayDecide(); // 通常決定時と完全に同一の決定音を発音！

                if (currentScenePtr) {
                    if (auto menuMgr = currentScenePtr->FindGameObject("MenuManager")) {
                        if (auto menuCtrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
                            menuCtrl->TriggerScreenFlash(); // 決定瞬間の白光フラッシュ
                            menuCtrl->StartDismissAnimation();
                            menuCtrl->SetLaunching(true);
                        }
                        if (dirComp) {
                            isDemoInGame_ = true;                // InGame側での帰還を待機
                            dirComp->SetNextSceneName("InGame"); // 本編 InGame シーンへ直接出撃！
                            dirComp->StartLaunchSequence();
                        }
                    }
                }
            }
        }

        // 仮想カーソル座標を反映（出撃決定後は上書きを終了してサニタイズ）
        if (!hasSubmitted_) {
            inputManager->SetVirtualCursorPosition(targetCursorPos);
            if (auto mouse = inputManager->GetMouse()) {
                mouse->SetVirtualPosition(targetCursorPos, true);
            }
        } else {
            if (auto mouse = inputManager->GetMouse()) {
                mouse->SetVirtualPosition({0.0f, 0.0f}, false);
            }
        }
    }
    // 通常待機中の場合（無操作アイドル監視）
    else {
        if (inputManager->HasAnyInput()) {
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

    // スキップ判定（画面表示後 0.30秒以降）
    // ※ F8キオスク固定展示中は操作遮断のため手動スキップしない
    // ※ 通常放置デモから遷移した場合のみ、プレイヤーが操作した瞬間に即座にタイトルへ戻す
    bool userSkip = false;
    if (!isKioskLoopMode_ && inputManager && inGameTimer_ > 0.30f) {
        if (inputManager->HasAnyInput()) {
            userSkip = true;
        }
    }

    if (userSkip) {
        // 手動スキップされた場合はデモを完全解除して通常タイトルに戻す
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
    hasTriggeredDemoPulse_ = false;
    s_isAttractModeActive_ = true;

    // 各ボタンの最新スクリーン座標を取得・キャッシュ
    posStart_ = GetButtonCenter("Btn_Start", {640.0f, 420.0f});
    posHowToPlay_ = GetButtonCenter("Btn_HowToPlay", {640.0f, 490.0f});
    posOptions_ = GetButtonCenter("Btn_Options", {640.0f, 560.0f});

    // 画面手前にいるガレキの最新スクリーン座標を取得し、必中かすめポイントとして動的設定
    posSweepRight_ = {820.0f, 310.0f};
    if (auto sm = engine_->GetSceneManager()) {
        if (auto currentScene = dynamic_cast<BaseScene*>(sm->GetCurrentScene())) {
            TitleSceneDirectorComponent* dirComp = nullptr;
            if (auto menuMgr = currentScene->FindGameObject("MenuManager")) {
                dirComp = menuMgr->GetComponent<TitleSceneDirectorComponent>();
            }
            if (!dirComp) {
                for (auto& obj : currentScene->GetGameObjects()) {
                    if (obj && (dirComp = obj->GetComponent<TitleSceneDirectorComponent>())) {
                        break;
                    }
                }
            }
            if (dirComp) {
                Irufemi::Vector2 debrisPos;
                if (dirComp->GetClosestFrontDebrisScreenPos(debrisPos)) {
                    // ガレキの至近距離（反発半径の内側約40px）を確実に通過
                    posSweepRight_ = {debrisPos.x + 40.0f, debrisPos.y};
                }
            }
        }
    }

    // 初期仮想カーソル位置を設定
    if (auto input = engine_->GetInputManager()) {
        input->SetVirtualCursorPosition(posSweepStart_);
        if (auto mouse = input->GetMouse()) {
            mouse->SetVirtualPosition(posSweepStart_, true);
        }
    }
}

void AttractDemoManager::StopTitleDemo() {
    isDemoPlaying_ = false;
    idleTimer_ = 0.0f;
    demoTimeline_ = 0.0f;
    hasSubmitted_ = false;
    hasTriggeredDemoPulse_ = false;
    s_isAttractModeActive_ = (isKioskLoopMode_ || isDemoInGame_);
    if (demoHudObj_ && !s_isAttractModeActive_) {
        demoHudObj_->SetActive(false);
    }

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
    hasTriggeredDemoPulse_ = false;
    idleTimer_ = 0.0f;
    demoTimeline_ = 0.0f;
    inGameTimer_ = 0.0f;
    s_isKioskModeActive_ = false;
    s_isAttractModeActive_ = false;
    if (demoHudObj_) {
        demoHudObj_->SetActive(false);
    }

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

void AttractDemoManager::UpdateDemoHud(float deltaTime) {
    if (!engine_) {
        return;
    }
    auto sm = engine_->GetSceneManager();
    if (!sm) {
        return;
    }
    auto baseScene = dynamic_cast<BaseScene*>(sm->GetCurrentScene());
    if (!baseScene) {
        return;
    }

    bool isDemoActive = s_isAttractModeActive_;

    if (!isDemoActive) {
        if (demoHudObj_) {
            demoHudObj_->SetActive(false);
        }
        demoHudBlinkTimer_ = 0.0f;
        return;
    }

    // シーン遷移や未初期化時に現在のシーンへHUDを生成・バインド
    if (!demoHudObj_ || demoHudObj_->GetScene() != baseScene) {
        if (auto existing = baseScene->FindGameObject("DemoAttractHUD")) {
            demoHudObj_ = existing;
            demoHudText_ = existing->GetComponent<TextRendererComponent>();
        } else {
            auto hudObj = std::make_shared<GameObject>("DemoAttractHUD");
            baseScene->AddGameObject(hudObj);
            hudObj->SetIsSerializable(false); // シーン保存汚染を防止
            hudObj->SetHideInHierarchy(true);

            if (auto t = hudObj->GetTransform()) {
                // 画面右上（右端マージン 55px, 上部マージン 42px）
                t->SetPosition({1225.0f, 42.0f, 0.0f});
            }

            auto textComp = hudObj->AddComponent<TextRendererComponent>();
            if (textComp) {
                textComp->SetFontId("toro_glitch");
                textComp->SetText(L"AUTO DEMO   |   PRESS ANY KEY");
                textComp->SetAlignment(TextAlignment::Right);
                textComp->SetBaseScale(22.0f);
                textComp->SetTopMost(true);
                textComp->SetColor({0.30f, 0.90f, 1.0f, 0.0f});
                demoHudText_ = textComp.get();
            }
            hudObj->Initialize();
            demoHudObj_ = hudObj;
        }
    }

    if (!demoHudObj_ || !demoHudText_) {
        return;
    }

    demoHudObj_->SetActive(true);
    demoHudBlinkTimer_ += deltaTime * 2.8f;

    // サイン波による優雅なアルファ脈動（0.30〜0.85）
    float pulse = std::sin(demoHudBlinkTimer_) * 0.5f + 0.5f;
    float blinkAlpha = 0.30f + pulse * 0.55f;

    // F8ループ展示中なら誰でも直感的にわかる専用文言（黄金色）に切り替え
    if (isKioskLoopMode_) {
        if (demoHudText_->GetText() != L"DEMO LOOP   |   PRESS F8 TO EXIT") {
            demoHudText_->SetText(L"DEMO LOOP   |   PRESS F8 TO EXIT");
        }
        demoHudText_->SetColor({1.0f, 0.85f, 0.25f, blinkAlpha}); // 黄金色
    } else {
        if (demoHudText_->GetText() != L"AUTO DEMO   |   PRESS ANY KEY") {
            demoHudText_->SetText(L"AUTO DEMO   |   PRESS ANY KEY");
        }
        demoHudText_->SetColor({0.30f, 0.90f, 1.0f, blinkAlpha}); // サイバーシアン
    }
}

void AttractDemoManager::CleanupDemoHud() {
    if (demoHudObj_) {
        demoHudObj_->Destroy();
        demoHudObj_.reset();
        demoHudText_ = nullptr;
    }
}
