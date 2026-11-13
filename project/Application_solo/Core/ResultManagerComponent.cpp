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
            t->SetPosition({640.0f, 635.0f, 0.0f});
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

    // =========================================================================
    // 大手AAA基準: 親パネルコンテナの生成（中心: 640.0f, 355.0f）
    // =========================================================================
    auto panelContainer = std::make_shared<GameObject>("Result_PanelContainer");
    scene->AddGameObject(panelContainer);
    if (auto t = panelContainer->GetTransform()) {
        t->SetPosition({640.0f, 375.0f, 0.0f});
    }
    panelContainer->Initialize();
    panelContainerObj_ = panelContainer;

    panelDecorSprites_.clear();
    panelDecorTexts_.clear();

    const float panelW = 620.0f;
    const float panelH = 370.0f;
    const float halfW = panelW * 0.5f;
    const float halfH = panelH * 0.5f;

    // 1. 半透明ダークネイビー・バックプレート (Z: 2.0f)
    {
        auto plateObj = std::make_shared<GameObject>("Panel_Backplate");
        scene->AddGameObject(plateObj);
        panelContainer->AddChild(plateObj);
        if (auto t = plateObj->GetTransform()) {
            t->SetPosition({0.0f, 0.0f, 2.0f});
        }
        auto sprite = plateObj->AddComponent<SpriteRendererComponent>();
        sprite->SetBaseSize({panelW, panelH});
        sprite->SetAnchor({0.5f, 0.5f});
        sprite->SetColor({0.02f, 0.04f, 0.07f, 0.0f}); // 初期非表示
        plateObj->Initialize();
        panelDecorSprites_.push_back({sprite, 0.78f});
    }

    // 2. 上下ネオンシアン・アクセントボーダー (厚さ 2px, Z: 1.5f)
    auto createLine = [&](const std::string& name, float localY, float width, float height,
                          const Irufemi::Vector4& color, float baseAlpha) {
        auto lineObj = std::make_shared<GameObject>(name);
        scene->AddGameObject(lineObj);
        panelContainer->AddChild(lineObj);
        if (auto t = lineObj->GetTransform()) {
            t->SetPosition({0.0f, localY, 1.5f});
        }
        auto sprite = lineObj->AddComponent<SpriteRendererComponent>();
        sprite->SetBaseSize({width, height});
        sprite->SetAnchor({0.5f, 0.5f});
        sprite->SetColor({color.x, color.y, color.z, 0.0f});
        lineObj->Initialize();
        panelDecorSprites_.push_back({sprite, baseAlpha});
    };

    createLine("Panel_TopBorder", -halfH, panelW, 2.0f, {0.30f, 0.90f, 1.00f, 1.0f}, 0.75f);
    createLine("Panel_BottomBorder", halfH, panelW, 2.0f, {0.30f, 0.90f, 1.00f, 1.0f}, 0.75f);
    createLine("Panel_HeaderDivider", -150.0f, panelW - 40.0f, 1.0f, {0.20f, 0.55f, 0.75f, 1.0f}, 0.50f);
    createLine("Panel_FooterDivider", 65.0f, panelW - 40.0f, 1.0f, {0.20f, 0.55f, 0.75f, 1.0f}, 0.50f);

    // 3. 四隅のL字コーナーブラケット (Corner Brackets, 各20px x 2px)
    auto createBracket = [&](const std::string& name, float posX, float posY, float w, float h) {
        auto bObj = std::make_shared<GameObject>(name);
        scene->AddGameObject(bObj);
        panelContainer->AddChild(bObj);
        if (auto t = bObj->GetTransform()) {
            t->SetPosition({posX, posY, 1.5f});
        }
        auto sprite = bObj->AddComponent<SpriteRendererComponent>();
        sprite->SetBaseSize({w, h});
        sprite->SetAnchor({0.5f, 0.5f});
        sprite->SetColor({0.40f, 0.95f, 1.00f, 0.0f});
        bObj->Initialize();
        panelDecorSprites_.push_back({sprite, 0.90f});
    };

    const float brkLen = 22.0f;
    const float brkThk = 2.0f;
    const float pad = 6.0f;
    // Top-Left
    createBracket("Bracket_TL_H", -halfW - pad + brkLen * 0.5f, -halfH - pad, brkLen, brkThk);
    createBracket("Bracket_TL_V", -halfW - pad, -halfH - pad + brkLen * 0.5f, brkThk, brkLen);
    // Top-Right
    createBracket("Bracket_TR_H", halfW + pad - brkLen * 0.5f, -halfH - pad, brkLen, brkThk);
    createBracket("Bracket_TR_V", halfW + pad, -halfH - pad + brkLen * 0.5f, brkThk, brkLen);
    // Bottom-Left
    createBracket("Bracket_BL_H", -halfW - pad + brkLen * 0.5f, halfH + pad, brkLen, brkThk);
    createBracket("Bracket_BL_V", -halfW - pad, halfH + pad - brkLen * 0.5f, brkThk, brkLen);
    // Bottom-Right
    createBracket("Bracket_BR_H", halfW + pad - brkLen * 0.5f, halfH + pad, brkLen, brkThk);
    createBracket("Bracket_BR_V", halfW + pad, halfH + pad - brkLen * 0.5f, brkThk, brkLen);

    // 4. ヘッダー装飾テキスト (SF端末記号論)
    auto createHeaderTxt = [&](const std::string& name, const std::wstring& str, float posX, float posY,
                               TextAlignment align, const Irufemi::Vector4& col, float baseAlpha) {
        auto obj = std::make_shared<GameObject>(name);
        scene->AddGameObject(obj);
        panelContainer->AddChild(obj);
        if (auto t = obj->GetTransform()) {
            t->SetPosition({posX, posY, 0.0f});
        }
        auto text = obj->AddComponent<TextRendererComponent>();
        text->SetFontId(fontId_);
        text->SetText(str);
        text->SetBaseScale(13.0f);
        text->SetTopMost(true);
        text->SetAlignment(align);
        text->SetColor({col.x, col.y, col.z, 0.0f});
        obj->Initialize();
        panelDecorTexts_.push_back({text, baseAlpha});
    };

    createHeaderTxt("Hdr_Status", L"[SYS: ONLINE]", -halfW + 25.0f, -165.0f, TextAlignment::Left,
                    {0.35f, 1.0f, 0.65f, 1.0f}, 0.85f);
    createHeaderTxt("Hdr_Title", L"// MISSION PERFORMANCE ARCHIVE", -halfW + 145.0f, -165.0f, TextAlignment::Left,
                    {0.45f, 0.82f, 0.98f, 1.0f}, 0.85f);
    createHeaderTxt("Hdr_Version", L"[VER 2.0.4]", halfW - 25.0f, -165.0f, TextAlignment::Right,
                    {0.55f, 0.62f, 0.72f, 1.0f}, 0.65f);

    // 5. 統計データ5行の構築（インデックス [01] 付き）
    struct RowData {
        std::wstring label;
        std::wstring value;
    };
    std::vector<RowData> rows;

    wchar_t scoreBuf[32];
    swprintf_s(scoreBuf, L"%08d pts", stats_.totalScore);
    rows.push_back({L"[01] TOTAL SCORE", scoreBuf});

    std::wstring multiStr =
        stats_.maxMultiKill >= 5 ? L"x5 (MAX)" : (L"x" + std::to_wstring((std::max)(1, stats_.maxMultiKill)));
    rows.push_back({L"[02] MAX MULTI-LOCK", multiStr});

    rows.push_back({L"[03] PURGED ENEMIES", std::to_wstring(stats_.enemiesKilled) + L" UNITS"});
    rows.push_back({L"[04] BULLETS BLOCKED", std::to_wstring(stats_.bulletsBlocked) + L" SHOTS"});

    int mins = static_cast<int>(stats_.clearTimeSeconds) / 60;
    int secs = static_cast<int>(stats_.clearTimeSeconds) % 60;
    int millis = static_cast<int>((stats_.clearTimeSeconds - static_cast<int>(stats_.clearTimeSeconds)) * 10.0f);
    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"%02d:%02d.%1d", mins, secs, millis);
    rows.push_back({L"[05] CLEAR TIME", timeBuf});

    statRows_.clear();
    const float startLocalY = -120.0f;
    const float stepLocalY = 35.0f;

    for (size_t i = 0; i < rows.size(); ++i) {
        float y = startLocalY + static_cast<float>(i) * stepLocalY;

        // ラベル列 (左揃え ローカル X: -halfW + 35)
        auto labelObj = std::make_shared<GameObject>("StatLabel_" + std::to_string(i));
        scene->AddGameObject(labelObj);
        panelContainer->AddChild(labelObj);
        if (auto t = labelObj->GetTransform()) {
            t->SetPosition({-halfW + 35.0f, y, 0.0f});
        }
        auto labelText = labelObj->AddComponent<TextRendererComponent>();
        labelText->SetFontId(fontId_);
        labelText->SetText(rows[i].label);
        labelText->SetBaseScale(17.0f);
        labelText->SetTopMost(true);
        labelText->SetAlignment(TextAlignment::Left);
        labelText->SetColor({0.65f, 0.72f, 0.85f, 0.0f}); // 初期非表示
        labelObj->Initialize();

        // 値列 (右揃え ローカル X: halfW - 35)
        auto valObj = std::make_shared<GameObject>("StatVal_" + std::to_string(i));
        scene->AddGameObject(valObj);
        panelContainer->AddChild(valObj);
        if (auto t = valObj->GetTransform()) {
            t->SetPosition({halfW - 35.0f, y, 0.0f});
        }
        auto valText = valObj->AddComponent<TextRendererComponent>();
        valText->SetFontId(fontId_);
        valText->SetText(rows[i].value);
        valText->SetBaseScale(19.0f);
        valText->SetTopMost(true);
        valText->SetAlignment(TextAlignment::Right);
        valText->SetColor({0.35f, 0.92f, 1.0f, 0.0f}); // 初期非表示
        valObj->Initialize();

        statRows_.push_back({labelObj, valObj, rows[i].value});
    }

    // 6. ランクスタンプUI (ローカル中央 Y: 120.0f)
    auto rankObj = std::make_shared<GameObject>("RankStampText");
    scene->AddGameObject(rankObj);
    panelContainer->AddChild(rankObj);
    if (auto t = rankObj->GetTransform()) {
        t->SetPosition({0.0f, 120.0f, 0.0f});
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
    // パネルフレーム＆装飾のフェードイン同期 (0.0s〜0.40s)
    if (phase_ == ResultPhase::TitleSnap) {
        phaseTimer_ += dt;
        float decorProgress = std::clamp(phaseTimer_ / 0.38f, 0.0f, 1.0f);

        // スプライト装飾のアルファ更新
        for (auto& item : panelDecorSprites_) {
            if (auto sp = item.sprite.lock()) {
                auto c = sp->GetColor();
                c.w = item.baseAlpha * decorProgress;
                sp->SetColor(c);
            }
        }
        // テキスト装飾のアルファ更新
        for (auto& item : panelDecorTexts_) {
            if (auto txt = item.text.lock()) {
                auto c = txt->GetColor();
                c.w = item.baseAlpha * decorProgress;
                txt->SetColor(c);
            }
        }

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

    // パネル装飾のアルファを即座に最大化
    for (auto& item : panelDecorSprites_) {
        if (auto sp = item.sprite.lock()) {
            auto c = sp->GetColor();
            c.w = item.baseAlpha;
            sp->SetColor(c);
        }
    }
    for (auto& item : panelDecorTexts_) {
        if (auto txt = item.text.lock()) {
            auto c = txt->GetColor();
            c.w = item.baseAlpha;
            txt->SetColor(c);
        }
    }

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
