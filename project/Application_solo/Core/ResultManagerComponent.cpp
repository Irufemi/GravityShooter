#include "Core/ResultManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Scenes/Result/ResultScene.h"
#include "Engine/Irufemi.h"
#include "Audio/AudioManager.h"
#include "UI/UISound.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include <algorithm>
#include <cmath>

ResultManagerComponent::~ResultManagerComponent() {
    RemoveEntryPostProcess(); // ポストプロセスの確実な解除

    if (auto engine = GetEngine()) {
        engine->SetTimeScale(1.0f); // シーン破棄時に確実にタイムスケールを等倍に戻す
    }
}

void ResultManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Result Delay Time", &resultDelayTime_);
    RegisterProperty("Time Scale Reset Delay", &timeScaleResetDelayTime_);
    RegisterProperty("Time Scale Recovery Duration", &timeScaleRecoveryDuration_);
    RegisterProperty("Base Title Scale", &baseTitleScale_);
    RegisterProperty("Next Scene Name", &nextSceneName_);
}

void ResultManagerComponent::Initialize() {
    timer_ = 0.0f;
    canReturnToTitle_ = false;
    isExiting_ = false;
    hasResetTimeScale_ = false;
    hasPlayedSnapSE_ = false;
    isRecoveringTimeScale_ = false;
    timeScaleRecoveryTimer_ = 0.0f;
    startTimeScale_ = 0.0f;
    isPostProcessActive_ = false;

    // 1. 退廃世界の空気感を引き締めるダーク暗幕オーバーレイの配置
    if (gameObject_ && gameObject_->GetScene()) {
        auto bgObj = std::make_shared<GameObject>("Result_DarkBackdrop");
        gameObject_->GetScene()->AddGameObject(bgObj);
        if (auto t = bgObj->GetTransform()) {
            t->SetPosition({640.0f, 360.0f, 5.0f});
        }
        auto sprite = bgObj->AddComponent<SpriteRendererComponent>();
        if (sprite) {
            sprite->SetTexture("resources/whiteTexture.png");
            sprite->SetBaseSize({1280.0f, 720.0f});
            sprite->SetAnchor({0.5f, 0.5f});
            sprite->SetColor({0.02f, 0.03f, 0.05f, 0.0f}); // 初期アルファ0
        }
        bgObj->Initialize();
        darkBackdropObj_ = bgObj;
    }

    // 2. 自分自身のGameObjectに TextRendererComponent を追加（または既存のものを取得）
    auto text = gameObject_->GetComponent<TextRendererComponent>();
    if (!text) {
        text = gameObject_->AddComponent<TextRendererComponent>().get();
    }

    // ResultScene に渡された静的フラグを使って表示文字を決定
    text->SetFontId("toro_glitch");
    text->SetText(ResultScene::IsClear() ? L"STAGE CLEAR" : L"GAME OVER");
    text->SetBaseScale(baseTitleScale_ * 1.35f); // 出現時は拡大から開始
    text->SetTopMost(true);
    text->SetAlignment(TextAlignment::Center);
    // クリア時はネオンシアン・グリーン、失敗時は重力警告レッド
    text->SetColor(ResultScene::IsClear() ? Irufemi::Vector4{0.4f, 1.0f, 0.7f, 1.0f}
                                          : Irufemi::Vector4{1.0f, 0.25f, 0.25f, 1.0f});

    // 3. "Press SPACE to Return" 用のテキストオブジェクトを事前に生成し非アクティブ化
    if (gameObject_ && gameObject_->GetScene()) {
        auto uiObj = std::make_shared<GameObject>("ReturnText");
        gameObject_->GetScene()->AddGameObject(uiObj);
        if (auto t = uiObj->GetTransform()) {
            t->SetPosition({640.0f, 600.0f, 0.0f});
        }

        auto returnText = uiObj->AddComponent<TextRendererComponent>().get();
        returnText->SetFontId("toro_glitch");
        returnText->SetText(L"Press SPACE / A to Return");
        returnText->SetBaseScale(20.0f);
        returnText->SetTopMost(true);
        returnText->SetAlignment(TextAlignment::Center);
        returnText->SetColor({0.85f, 0.9f, 1.0f, 0.0f});

        uiObj->Initialize();
        uiObj->SetActive(false); // 初期状態は非表示
        pressSpaceObj_ = uiObj;
    }

    // 4. リザルト突入時の演出ポストプロセス（勝利の光芒 or 敗北の重力歪曲）を発火
    ApplyEntryPostProcess();
}

void ResultManagerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    // スローモーションの影響を受けないよう、RealDeltaTimeを使用する
    float dt = engine->GetRealDeltaTime();
    timer_ += dt;

    // 暗幕のフェードイン (0.0 -> 0.78 / EaseInQuad でグッと暗く沈み込ませる)
    float enterProgress = std::clamp(timer_ / 0.50f, 0.0f, 1.0f);
    UpdateEntryPostProcess(enterProgress);

    if (auto bgObj = darkBackdropObj_.lock()) {
        if (auto sprite = bgObj->GetComponent<SpriteRendererComponent>()) {
            float bgAlpha = 0.78f * (enterProgress * enterProgress);
            sprite->SetColor({0.02f, 0.03f, 0.05f, bgAlpha});
        }
    }

    // タイトル文字のスケールスナップ演出 (1.35 -> 1.0)
    if (auto text = gameObject_->GetComponent<TextRendererComponent>()) {
        float scaleProgress = std::clamp(timer_ / 0.40f, 0.0f, 1.0f);
        float scaleOffset = 0.35f * std::pow(1.0f - scaleProgress, 3.0f);
        text->SetBaseScale(baseTitleScale_ * (1.0f + scaleOffset));

        // スナップ完了の瞬間 (0.40秒) に se_hud_snap.mp3 (メカニカルラッチ音) を発火！
        if (!hasPlayedSnapSE_ && scaleProgress >= 0.98f) {
            hasPlayedSnapSE_ = true;
            if (auto am = engine->GetAudioManager()) {
                am->PlayByFile("resources/audio/SE/se_hud_snap.mp3", 0.90f, 1.0f, AudioCategory::SE);
            }
        }
    }

    // 一定時間経過後、スローモーションを解除して等倍速に戻す（イージング開始）
    if (!hasResetTimeScale_ && !isRecoveringTimeScale_ && timer_ >= timeScaleResetDelayTime_) {
        isRecoveringTimeScale_ = true;
        startTimeScale_ = engine->GetTimeScale();
        timeScaleRecoveryTimer_ = 0.0f;
    }

    // イージング中の処理
    if (isRecoveringTimeScale_) {
        timeScaleRecoveryTimer_ += dt;
        float duration = (std::max)(timeScaleRecoveryDuration_, 0.001f);
        float t = std::clamp(timeScaleRecoveryTimer_ / duration, 0.0f, 1.0f);

        // 滑らかに補間する (Smoothstep: 3t^2 - 2t^3)
        float easeT = t * t * (3.0f - 2.0f * t);

        float newScale = startTimeScale_ + (1.0f - startTimeScale_) * easeT;
        engine->SetTimeScale(newScale);

        if (t >= 1.0f) {
            isRecoveringTimeScale_ = false;
            hasResetTimeScale_ = true;
            engine->SetTimeScale(1.0f); // 確実に1.0に戻す
        }
    }

    // リターン待機解除
    if (!canReturnToTitle_ && timer_ >= resultDelayTime_) {
        canReturnToTitle_ = true;

        if (auto uiObj = pressSpaceObj_.lock()) {
            uiObj->SetActive(true);
        }
    }

    // ガイドテキストの呼吸点滅 (Pulse) & タイトル帰還
    if (canReturnToTitle_ && !isExiting_) {
        if (auto uiObj = pressSpaceObj_.lock()) {
            if (auto returnText = uiObj->GetComponent<TextRendererComponent>()) {
                float pulse = 0.5f + 0.5f * std::sin((timer_ - resultDelayTime_) * 4.5f);
                returnText->SetColor({0.85f, 0.9f, 1.0f, 0.35f + 0.65f * pulse});
            }
        }

        auto input = engine->GetInputManager();
        bool userTrigger = input && (input->IsKeyPressed(VK_SPACE) || input->IsButtonPressed(XINPUT_GAMEPAD_A) ||
                                     input->IsButtonPressed(XINPUT_GAMEPAD_START));

        if (userTrigger) {
            isExiting_ = true;
            UISound::PlayDecide(1.0f);
            engine->SetTimeScale(1.0f);

            // BGMの滑らかなフェードアウト (0.75秒で対数減衰し、Titleへ静かに遷移)
            if (auto am = engine->GetAudioManager()) {
                am->FadeCategoryVolume(AudioCategory::BGM, 0.0f, 0.75f);
            }

            // 重力歪曲（RadialBlur）でタイトルへ帰還 (世界観に合致した重力シアンブラック指定)
            engine->GetSceneManager()->TransitionTo(nextSceneName_, SceneTransition::Type::RadialBlur, 0.8f,
                                                    EaseType::EaseInCubic, Irufemi::Vector4{0.01f, 0.03f, 0.06f, 1.0f});
        }
    }
}

void ResultManagerComponent::ApplyEntryPostProcess() {
    if (isPostProcessActive_) {
        return;
    }
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto ppm = engine->GetPostProcessManager();
    if (!ppm) {
        return;
    }

    if (ResultScene::IsClear()) {
        // ステージクリア: 勝利の超光速ブレイク・放射ブラー (PreUIレイヤー)
        ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PreUI);
        auto& rb = ppm->GetRadialBlurParams();
        rb.center = {0.5f, 0.5f};
        rb.blurWidth = 0.040f;
        rb.numSamples = 16;

        // KOインパクト・ホワイトフラッシュ（PreUIレイヤーで背景を眩く包む）
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
        ppm->GetFadeParams().color = Irufemi::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
        ppm->GetFadeParams().intensity = 1.0f;
        hasWhiteFlash_ = true;
    } else {
        // ゲームオーバー: 重力異常グリッチ・色収差 (PreUIレイヤー)
        ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PreUI);
        auto& ca = ppm->GetChromaticAberrationParams();
        ca.intensity = 0.035f;

        // スーパーノヴァ爆発の全画面白光を引き継ぎ、暗幕へと滑らかに収束させる
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
        ppm->GetFadeParams().color = Irufemi::Vector4{1.0f, 1.0f, 1.0f, 1.0f};
        ppm->GetFadeParams().intensity = 1.0f;
        hasWhiteFlash_ = true;
    }

    isPostProcessActive_ = true;
}

void ResultManagerComponent::UpdateEntryPostProcess(float progress) {
    if (!isPostProcessActive_) {
        return;
    }
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto ppm = engine->GetPostProcessManager();
    if (!ppm) {
        return;
    }

    float factor = (1.0f - progress) * (1.0f - progress); // 2次減衰

    if (ResultScene::IsClear()) {
        auto& rb = ppm->GetRadialBlurParams();
        rb.blurWidth = 0.040f * factor;
    } else {
        auto& ca = ppm->GetChromaticAberrationParams();
        ca.intensity = 0.035f * factor;
    }

    // クリア・ゲームオーバー問わず、前シーンからの白光を3次減衰カーブでスッと収束
    if (hasWhiteFlash_) {
        ppm->GetFadeParams().intensity = std::pow(1.0f - progress, 3.0f);
    }

    if (progress >= 1.0f) {
        RemoveEntryPostProcess();
    }
}

void ResultManagerComponent::RemoveEntryPostProcess() {
    if (!isPostProcessActive_) {
        return;
    }
    if (auto engine = GetEngine()) {
        if (auto ppm = engine->GetPostProcessManager()) {
            ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
            if (hasWhiteFlash_) {
                ppm->RemoveActiveMode(PostProcessMode::Fade);
                hasWhiteFlash_ = false;
            }
            ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);
        }
    }
    isPostProcessActive_ = false;
}
