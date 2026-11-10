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
#include "Audio/SoundHelper.h"
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
    RegisterProperty("Font ID", &fontId_);
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
        Irufemi::Vector2 gameRes = {1280.0f, 720.0f};
        if (auto engine = GetEngine()) {
            gameRes = engine->GetGameResolution();
        }
        if (auto t = bgObj->GetTransform()) {
            t->SetPosition({gameRes.x * 0.5f, gameRes.y * 0.5f, 5.0f});
        }
        auto sprite = bgObj->AddComponent<SpriteRendererComponent>();
        if (sprite) {
            sprite->SetBaseSize(gameRes);
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
    text->SetFontId(fontId_);
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
            t->SetPosition({640.0f, 620.0f, 0.0f});
        }

        auto returnText = uiObj->AddComponent<TextRendererComponent>().get();
        returnText->SetFontId(fontId_);
        returnText->SetText(L"Press SPACE / A to Return");
        returnText->SetBaseScale(18.0f);
        returnText->SetTopMost(true);
        returnText->SetAlignment(TextAlignment::Center);
        returnText->SetColor({0.85f, 0.9f, 1.0f, 0.0f});

        uiObj->Initialize();
        uiObj->SetActive(false); // 初期状態は非表示
        pressSpaceObj_ = uiObj;
    }

    // 4. セッション統計＆ランク判定UIの構築
    SetupStatsUI();

    // 5. リザルト突入時の演出ポストプロセス（勝利の光芒 or 敗北の重力歪曲）を発火
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
            SoundHelper::Play(GameSE::HudSnap);
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

    // 統計・ランク演出の更新
    UpdateStatsRoll(dt);

    // ガイドテキストの呼吸点滅 (Pulse)
    if (canReturnToTitle_ && !isExiting_) {
        if (auto uiObj = pressSpaceObj_.lock()) {
            if (auto returnText = uiObj->GetComponent<TextRendererComponent>()) {
                float pulse = 0.5f + 0.5f * std::sin((timer_ - resultDelayTime_) * 4.5f);
                returnText->SetColor({0.85f, 0.9f, 1.0f, 0.35f + 0.65f * pulse});
            }
        }
    }

    // ユーザー入力（スキップ または タイトル帰還）
    auto input = engine->GetInputManager();
    bool userTrigger = input && (input->IsKeyPressed(VK_SPACE) || input->IsButtonPressed(XINPUT_GAMEPAD_A) ||
                                 input->IsButtonPressed(XINPUT_GAMEPAD_START));

    if (userTrigger) {
        if (phase_ != ResultPhase::Ready) {
            // 演出中の場合は即座にフル開示（スキップ）
            SkipToReady();
        } else if (!isExiting_) {
            // 演出完了後の場合はタイトルへ帰還
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

void ResultManagerComponent::SetupStatsUI() {
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (!scene) {
        return;
    }

    if (auto scoreMgr = ScoreManagerComponent::GetInstance()) {
        stats_ = scoreMgr->GetStats();
    }

    // ランク判定（クリア時：スコア基準、ゲームオーバー時：D）
    if (ResultScene::IsClear()) {
        if (stats_.totalScore >= 20000) {
            rankText_ = L"EVALUATION: RANK S";
            rankColor_ = {1.0f, 0.85f, 0.2f, 1.0f}; // ゴールド
        } else if (stats_.totalScore >= 12000) {
            rankText_ = L"EVALUATION: RANK A";
            rankColor_ = {0.3f, 0.95f, 1.0f, 1.0f}; // シアン
        } else if (stats_.totalScore >= 6000) {
            rankText_ = L"EVALUATION: RANK B";
            rankColor_ = {0.4f, 1.0f, 0.6f, 1.0f}; // エメラルドグリーン
        } else {
            rankText_ = L"EVALUATION: RANK C";
            rankColor_ = {0.8f, 0.85f, 0.9f, 1.0f}; // クールホワイト
        }
    } else {
        rankText_ = L"MISSION FAILED (RANK D)";
        rankColor_ = {1.0f, 0.3f, 0.3f, 1.0f}; // 警告レッド
    }

    // 統計データ5行の構築
    struct RowData {
        std::wstring label;
        std::wstring value;
    };
    std::vector<RowData> rows;

    wchar_t scoreBuf[32];
    swprintf_s(scoreBuf, L"%08d pts", stats_.totalScore);
    rows.push_back({L"TOTAL SCORE", scoreBuf});

    std::wstring multiStr = stats_.maxMultiKill >= 5 ? L"x5 (MAX)" : (L"x" + std::to_wstring((std::max)(1, stats_.maxMultiKill)));
    rows.push_back({L"MAX MULTI-LOCK", multiStr});

    rows.push_back({L"PURGED ENEMIES", std::to_wstring(stats_.enemiesKilled) + L" UNITS"});
    rows.push_back({L"BULLETS BLOCKED", std::to_wstring(stats_.bulletsBlocked) + L" SHOTS"});

    int mins = static_cast<int>(stats_.clearTimeSeconds) / 60;
    int secs = static_cast<int>(stats_.clearTimeSeconds) % 60;
    int millis = static_cast<int>((stats_.clearTimeSeconds - static_cast<int>(stats_.clearTimeSeconds)) * 10.0f);
    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"%02d:%02d.%1d", mins, secs, millis);
    rows.push_back({L"CLEAR TIME", timeBuf});

    statRows_.clear();
    const float startY = 220.0f;
    const float stepY = 38.0f;

    for (size_t i = 0; i < rows.size(); ++i) {
        float y = startY + static_cast<float>(i) * stepY;

        // ラベル列 (左揃え X: 380)
        auto labelObj = std::make_shared<GameObject>("StatLabel_" + std::to_string(i));
        scene->AddGameObject(labelObj);
        if (auto t = labelObj->GetTransform()) {
            t->SetPosition({380.0f, y, 0.0f});
        }
        auto labelText = labelObj->AddComponent<TextRendererComponent>();
        labelText->SetFontId(fontId_);
        labelText->SetText(rows[i].label);
        labelText->SetBaseScale(18.0f);
        labelText->SetTopMost(true);
        labelText->SetAlignment(TextAlignment::Left);
        labelText->SetColor({0.65f, 0.72f, 0.85f, 0.0f}); // 初期非表示
        labelObj->Initialize();

        // 値列 (右揃え X: 900)
        auto valObj = std::make_shared<GameObject>("StatVal_" + std::to_string(i));
        scene->AddGameObject(valObj);
        if (auto t = valObj->GetTransform()) {
            t->SetPosition({900.0f, y, 0.0f});
        }
        auto valText = valObj->AddComponent<TextRendererComponent>();
        valText->SetFontId(fontId_);
        valText->SetText(rows[i].value);
        valText->SetBaseScale(20.0f);
        valText->SetTopMost(true);
        valText->SetAlignment(TextAlignment::Right);
        valText->SetColor({0.35f, 0.92f, 1.0f, 0.0f}); // 初期非表示
        valObj->Initialize();

        statRows_.push_back({labelObj, valObj, rows[i].value});
    }

    // ランクスタンプUI (中央 Y: 485)
    auto rankObj = std::make_shared<GameObject>("RankStampText");
    scene->AddGameObject(rankObj);
    if (auto t = rankObj->GetTransform()) {
        t->SetPosition({640.0f, 485.0f, 0.0f});
    }
    auto rankText = rankObj->AddComponent<TextRendererComponent>();
    rankText->SetFontId(fontId_);
    rankText->SetText(rankText_);
    rankText->SetBaseScale(46.0f);
    rankText->SetTopMost(true);
    rankText->SetAlignment(TextAlignment::Center);
    rankText->SetColor({rankColor_.x, rankColor_.y, rankColor_.z, 0.0f}); // 初期非表示
    rankObj->Initialize();
    rankValueObj_ = rankObj;
}

void ResultManagerComponent::UpdateStatsRoll(float dt) {
    if (phase_ == ResultPhase::TitleSnap) {
        phaseTimer_ += dt;
        if (phaseTimer_ >= 0.45f) {
            phase_ = ResultPhase::RollStats;
            phaseTimer_ = 0.0f;
            if (!hasPlayedDataSyncSE_) {
                hasPlayedDataSyncSE_ = true;
                SoundHelper::Play(GameSE::DataSync);
            }
        }
    } else if (phase_ == ResultPhase::RollStats) {
        phaseTimer_ += dt;
        float rollDuration = 0.90f;
        float progress = std::clamp(phaseTimer_ / rollDuration, 0.0f, 1.0f);

        for (size_t i = 0; i < statRows_.size(); ++i) {
            float rowThreshold = static_cast<float>(i) / static_cast<float>(statRows_.size());
            float rowAlpha = std::clamp((progress - rowThreshold) / 0.20f, 0.0f, 1.0f);

            if (auto label = statRows_[i].labelObj.lock()) {
                if (auto text = label->GetComponent<TextRendererComponent>()) {
                    text->SetColor({0.65f, 0.72f, 0.85f, rowAlpha});
                }
            }
            if (auto val = statRows_[i].valueObj.lock()) {
                if (auto text = val->GetComponent<TextRendererComponent>()) {
                    text->SetColor({0.35f, 0.92f, 1.0f, rowAlpha});
                }
            }
        }

        if (phaseTimer_ >= 1.00f) {
            phase_ = ResultPhase::PreStamp;
            phaseTimer_ = 0.0f;
        }
    } else if (phase_ == ResultPhase::PreStamp) {
        phaseTimer_ += dt;
        if (phaseTimer_ >= 0.15f) { // 0.15秒のタメ
            phase_ = ResultPhase::StampRank;
            phaseTimer_ = 0.0f;
            TriggerRankStamp();
        }
    } else if (phase_ == ResultPhase::StampRank) {
        phaseTimer_ += dt;
        float stampProgress = std::clamp(phaseTimer_ / 0.25f, 0.0f, 1.0f);
        float scaleOffset = 1.0f * std::pow(1.0f - stampProgress, 3.0f); // 2.0 -> 1.0
        float alpha = std::clamp(phaseTimer_ / 0.08f, 0.0f, 1.0f);

        if (auto rankObj = rankValueObj_.lock()) {
            if (auto text = rankObj->GetComponent<TextRendererComponent>()) {
                text->SetBaseScale(46.0f * (1.0f + scaleOffset));
                text->SetColor({rankColor_.x, rankColor_.y, rankColor_.z, alpha});
            }
        }

        if (phaseTimer_ >= 0.40f) {
            phase_ = ResultPhase::Ready;
            canReturnToTitle_ = true;
            if (auto uiObj = pressSpaceObj_.lock()) {
                uiObj->SetActive(true);
            }
        }
    }
}

void ResultManagerComponent::TriggerRankStamp() {
    if (!hasPlayedRankSE_) {
        hasPlayedRankSE_ = true;
        SoundHelper::Play(GameSE::RankStamp);
    }
    if (auto rankObj = rankValueObj_.lock()) {
        if (auto text = rankObj->GetComponent<TextRendererComponent>()) {
            text->SetBaseScale(46.0f * 2.0f);
            text->SetColor({rankColor_.x, rankColor_.y, rankColor_.z, 1.0f});
        }
    }
}

void ResultManagerComponent::SkipToReady() {
    phase_ = ResultPhase::Ready;
    canReturnToTitle_ = true;

    // 全行をアルファ1.0で表示
    for (size_t i = 0; i < statRows_.size(); ++i) {
        if (auto label = statRows_[i].labelObj.lock()) {
            if (auto text = label->GetComponent<TextRendererComponent>()) {
                text->SetColor({0.65f, 0.72f, 0.85f, 1.0f});
            }
        }
        if (auto val = statRows_[i].valueObj.lock()) {
            if (auto text = val->GetComponent<TextRendererComponent>()) {
                text->SetColor({0.35f, 0.92f, 1.0f, 1.0f});
            }
        }
    }

    // ランクスタンプ表示
    if (!hasPlayedRankSE_) {
        hasPlayedRankSE_ = true;
        SoundHelper::Play(GameSE::RankStamp);
    }
    if (auto rankObj = rankValueObj_.lock()) {
        if (auto text = rankObj->GetComponent<TextRendererComponent>()) {
            text->SetBaseScale(46.0f);
            text->SetColor({rankColor_.x, rankColor_.y, rankColor_.z, 1.0f});
        }
    }

    if (auto uiObj = pressSpaceObj_.lock()) {
        uiObj->SetActive(true);
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
