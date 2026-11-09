#include "UI/ScoreHUDComponent.h"
#include "Combat/ScoreManagerComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>

ScoreHUDComponent::ScoreHUDComponent() = default;
ScoreHUDComponent::~ScoreHUDComponent() = default;

void ScoreHUDComponent::Initialize() {
    displayedScore_ = 0.0f;
    popAnimT_ = 0.0f;
    isElementsCreated_ = false;
}

void ScoreHUDComponent::Start() {
    EnsureUIElements();
}

ScoreHUDComponent* ScoreHUDComponent::GetOrCreate(BaseScene* scene) {
    if (!scene) {
        return nullptr;
    }

    if (auto obj = scene->FindGameObject("ScoreHUD")) {
        if (auto comp = obj->GetComponent<ScoreHUDComponent>()) {
            return comp;
        }
    }

    auto hudObj = std::make_shared<GameObject>("ScoreHUD");
    hudObj->SetIsSerializable(false);
    auto comp = hudObj->AddComponent<ScoreHUDComponent>();
    scene->AddGameObject(hudObj);
    comp->Initialize();
    return comp.get();
}

void ScoreHUDComponent::EnsureUIElements() {
    if (isElementsCreated_ || !gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }

    // --- 1. 画面右上固定：トータルスコア ＆ 撃破総数 ---
    scoreHeaderObj_ = std::make_shared<GameObject>("HUD_ScoreHeader");
    scoreHeaderObj_->SetIsSerializable(false);
    scene->AddGameObject(scoreHeaderObj_);
    if (auto t = scoreHeaderObj_->GetTransform()) {
        t->SetPosition({1240.0f, 22.0f, 0.0f});
    }
    scoreHeaderText_ = scoreHeaderObj_->AddComponent<TextRendererComponent>().get();
    scoreHeaderText_->SetFontId("toro_glitch");
    scoreHeaderText_->SetAlignment(TextAlignment::Right);
    scoreHeaderText_->SetBaseScale(12.0f);
    scoreHeaderText_->SetTopMost(true);
    scoreHeaderText_->SetColor({0.35f, 0.75f, 1.0f, 0.5f});
    scoreHeaderText_->SetText(L"// DATA: SCORE");
    scoreHeaderObj_->Initialize();

    scoreObj_ = std::make_shared<GameObject>("HUD_ScoreText");
    scoreObj_->SetIsSerializable(false);
    scene->AddGameObject(scoreObj_);
    if (auto t = scoreObj_->GetTransform()) {
        t->SetPosition({1240.0f, 44.0f, 0.0f});
    }
    scoreText_ = scoreObj_->AddComponent<TextRendererComponent>().get();
    scoreText_->SetFontId("toro_glitch");
    scoreText_->SetAlignment(TextAlignment::Right);
    scoreText_->SetBaseScale(22.0f);
    scoreText_->SetTopMost(true);
    scoreText_->SetColor({1.0f, 1.0f, 1.0f, 0.95f});
    scoreText_->SetText(L"00000000");
    scoreObj_->Initialize();

    killsObj_ = std::make_shared<GameObject>("HUD_KillsText");
    killsObj_->SetIsSerializable(false);
    scene->AddGameObject(killsObj_);
    if (auto t = killsObj_->GetTransform()) {
        t->SetPosition({1240.0f, 68.0f, 0.0f});
    }
    killsText_ = killsObj_->AddComponent<TextRendererComponent>().get();
    killsText_->SetFontId("toro_glitch");
    killsText_->SetAlignment(TextAlignment::Right);
    killsText_->SetBaseScale(14.0f);
    killsText_->SetTopMost(true);
    killsText_->SetColor({0.4f, 0.85f, 1.0f, 0.75f});
    killsText_->SetText(L"KILLS: 00");
    killsObj_->Initialize();

    // --- 2. 照準周辺ポップアップ（普段は完全非表示！） ---
    popObj_ = std::make_shared<GameObject>("HUD_HitPopText");
    popObj_->SetIsSerializable(false);
    scene->AddGameObject(popObj_);
    popText_ = popObj_->AddComponent<TextRendererComponent>().get();
    popText_->SetFontId("toro_glitch");
    popText_->SetAlignment(TextAlignment::Left);
    popText_->SetBaseScale(20.0f);
    popText_->SetTopMost(true);
    popText_->SetColor({1.0f, 0.9f, 0.3f, 1.0f});
    popText_->SetText(L"");
    popObj_->Initialize();

    popScoreObj_ = std::make_shared<GameObject>("HUD_HitPopScore");
    popScoreObj_->SetIsSerializable(false);
    scene->AddGameObject(popScoreObj_);
    popScoreText_ = popScoreObj_->AddComponent<TextRendererComponent>().get();
    popScoreText_->SetFontId("toro_glitch");
    popScoreText_->SetAlignment(TextAlignment::Left);
    popScoreText_->SetBaseScale(16.0f);
    popScoreText_->SetTopMost(true);
    popScoreText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    popScoreText_->SetText(L"");
    popScoreObj_->Initialize();

    isElementsCreated_ = true;
}

void ScoreHUDComponent::Update() {
    EnsureUIElements();

    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    auto scoreMgr = ScoreManagerComponent::GetInstance();
    if (!scoreMgr) {
        return;
    }

    // --- A. 右上スコアの更新 (Rolling Digits: パラパラ回転補間) ---
    float targetScore = static_cast<float>(scoreMgr->GetCurrentScore());
    displayedScore_ = std::lerp(displayedScore_, targetScore, std::clamp(dt * 12.0f, 0.0f, 1.0f));
    int scoreToShow = static_cast<int>(std::round(displayedScore_));

    std::ostringstream ss;
    ss << std::setw(8) << std::setfill('0') << scoreToShow;
    std::string scoreStr = ss.str();
    std::wstring wScoreStr(scoreStr.begin(), scoreStr.end());
    if (scoreText_) {
        scoreText_->SetText(wScoreStr);
    }

    if (killsText_) {
        int kills = scoreMgr->GetTotalKills();
        std::ostringstream kss;
        kss << "KILLS: " << std::setw(2) << std::setfill('0') << kills;
        std::string kStr = kss.str();
        std::wstring wkStr(kStr.begin(), kStr.end());
        killsText_->SetText(wkStr);
    }

    // --- B. 撃破時ポップアップ演出（レティクル位置の取得と動的ポップ） ---
    const auto& popup = scoreMgr->GetPopupData();

    if (popup.remainingDisplayTime > 0.0f) {
        // 仮想カーソル（照準）の現在座標を取得
        Irufemi::Vector2 cursorPos = {640.0f, 360.0f};
        if (auto inputManager = engine->GetInputManager()) {
            cursorPos = inputManager->GetVirtualCursorPosition();
        }

        // 残時間に応じたフェードと微小上昇アニメーション
        float ratio = std::clamp(popup.remainingDisplayTime / 0.85f, 0.0f, 1.0f);
        float alpha = std::clamp(ratio * 1.5f, 0.0f, 1.0f);
        float yFloat = (1.0f - ratio) * 16.0f; // 上へフワッと16px上昇

        Irufemi::Vector3 basePos = {cursorPos.x + 36.0f, cursorPos.y + 10.0f - yFloat, 0.0f};
        if (popObj_ && popObj_->GetTransform()) {
            popObj_->GetTransform()->SetPosition(basePos);
        }
        if (popScoreObj_ && popScoreObj_->GetTransform()) {
            popScoreObj_->GetTransform()->SetPosition({basePos.x, basePos.y + 22.0f, 0.0f});
        }

        int count = popup.killCount;
        int pts = popup.scoreGained;

        if (popText_) {
            if (count >= 5) {
                popText_->SetText(L"MULTI KILL x5!! (MAX)");
                popText_->SetBaseScale(24.0f);
                popText_->SetColor({1.0f, 0.25f, 0.55f, alpha}); // ネオンピンク
            } else if (count >= 3) {
                popText_->SetText(L"MULTI KILL x" + std::to_wstring(count) + L"!");
                popText_->SetBaseScale(22.0f);
                popText_->SetColor({1.0f, 0.85f, 0.2f, alpha}); // ゴールド
            } else if (count == 2) {
                popText_->SetText(L"MULTI KILL x2!");
                popText_->SetBaseScale(20.0f);
                popText_->SetColor({0.3f, 1.0f, 0.6f, alpha}); // ネオングリーン
            } else {
                popText_->SetText(L"HIT!");
                popText_->SetBaseScale(17.0f);
                popText_->SetColor({0.3f, 0.85f, 1.0f, alpha}); // シアン
            }
        }

        if (popScoreText_) {
            std::wstring scorePts = L"+" + std::to_wstring(pts) + L" pts";
            popScoreText_->SetText(scorePts);
            popScoreText_->SetBaseScale((count >= 3) ? 17.0f : 15.0f);
            popScoreText_->SetColor({1.0f, 1.0f, 1.0f, alpha});
        }
    } else {
        // 表示時間終了時は照準まわりを完全非表示（普段は100%クリアな視界）
        if (popText_) {
            popText_->SetText(L"");
        }
        if (popScoreText_) {
            popScoreText_->SetText(L"");
        }
    }
}
