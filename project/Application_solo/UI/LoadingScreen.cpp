#include "UI/LoadingScreen.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h"
#include "Renderer/Object/2D/Text/Text.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Core/AttractDemoManager.h"
#include <cmath>

LoadingScreen::LoadingScreen() = default;
LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Finalize() {
    dots_.clear();
    nowLoadingText_.reset();
    bgSprite_.reset();
    demoText_.reset();
    camera_.reset();
}

void LoadingScreen::Initialize(IrufemiEngine* engine) {
    if (!engine) {
        return;
    }

    camera_ = std::make_unique<Camera>();
    camera_->Initialize(engine->GetGameResolutionWidth(), engine->GetGameResolutionHeight());
    camera_->UpdateMatrix();

    // デモ案内HUD用テキスト初期化
    demoText_ = std::make_unique<Text>();
    demoText_->SetFontManagerInstance(engine->GetFontManager());
    demoText_->SetDrawManagerInstance(engine->GetDrawManager());
    demoText_->SetCameraManagerInstance(engine->GetCameraManager());
    demoText_->Initialize("toro_glitch");
    demoText_->SetAlignment(TextAlignment::Right);
    demoText_->SetBaseScale(22.0f);
    demoText_->SetTopMost(true);

    nowLoadingText_ = std::make_unique<Sprite>();
    // 生成した「Now Loading」画像をセット
    nowLoadingText_->Initialize("resources/texture/load/now_loading.png");

    // 画像は黒背景に文字が含まれる
    // 描画時に加算合成(Add)を使うことで、黒を透過させる
    nowLoadingText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

    float screenW = static_cast<float>(engine->GetGameResolutionWidth());
    float screenH = static_cast<float>(engine->GetGameResolutionHeight());

    // 背景を真っ黒に塗りつぶすスプライト
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize("resources/whiteTexture.png");
    bgSprite_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
    bgSprite_->SetSize(screenW, screenH);
    bgSprite_->SetAnchor(0.0f, 0.0f); // 左上
    bgSprite_->SetPosition(0.0f, 0.0f);
    bgSprite_->SetTopMost(true);

    // スプライトのサイズと位置を右下に合わせる
    // 画像は正方形(1:1)なので、縮尺がおかしくならないよう同サイズにする
    nowLoadingText_->SetSize(256.0f, 256.0f);
    nowLoadingText_->SetAnchor(1.0f, 0.5f);                         // 右端・縦中央アンカー
    nowLoadingText_->SetPosition(screenW - 80.0f, screenH - 45.0f); // ドットの高さと合わせる

    // "..." のドットを3つ作る
    for (int i = 0; i < 3; ++i) {
        auto dot = std::make_unique<Primitive2DObject>();
        dot->Initialize(Irufemi::Primitive2DType::Circle, "resources/whiteTexture.png");
        dot->SetColor({1.0f, 1.0f, 1.0f, 1.0f}); // 文字のNeon Cyanに合わせた色にする
        dot->SetPivot({0.5f, 0.5f});

        // 中心座標をセット: nowLoadingText_ のさらに右側に等間隔で配置
        float baseX = screenW - 65.0f;
        float baseY = screenH - 45.0f;
        dot->SetPosition(Irufemi::Vector3{baseX + i * 20.0f, baseY, 0.0f});
        dot->SetSize({8.0f, 8.0f}); // radius 4.0f -> size 8.0f

        // 最前面UIとして登録
        dot->SetTopMost(true);

        dots_.push_back(std::move(dot));
    }

    // 最前面UIとして登録
    nowLoadingText_->SetTopMost(true);
}

void LoadingScreen::Update(float deltaTime) {
    animationTimer_ += deltaTime;
    // 0.5秒ごとにドットが1個増え、0 -> 1 -> 2 -> 3 -> 0 のループになる
    const float kDotInterval = 0.5f;
    if (animationTimer_ > kDotInterval) {
        animationTimer_ -= kDotInterval;
        dotCount_ = (dotCount_ + 1) % 4;
    }

    camera_->Update();
    if (bgSprite_) {
        bgSprite_->Update();
    }
    if (nowLoadingText_) {
        nowLoadingText_->Update();
    }
    for (auto& dot : dots_) {
        dot->Update();
    }

    // デモHUDアニメーション更新
    if (AttractDemoManager::IsAttractModeActive() && demoText_) {
        demoBlinkTimer_ += deltaTime * 2.8f;
        float pulse = std::sin(demoBlinkTimer_) * 0.5f + 0.5f;
        float blinkAlpha = 0.30f + pulse * 0.55f;

        if (AttractDemoManager::IsKioskModeActive()) {
            if (demoText_->GetText() != L"DEMO LOOP   |   PRESS F8 TO EXIT") {
                demoText_->SetText(L"DEMO LOOP   |   PRESS F8 TO EXIT");
            }
            demoText_->SetColor({1.0f, 0.85f, 0.25f, blinkAlpha}); // 黄金色
        } else {
            if (demoText_->GetText() != L"AUTO DEMO   |   PRESS ANY KEY") {
                demoText_->SetText(L"AUTO DEMO   |   PRESS ANY KEY");
            }
            demoText_->SetColor({0.30f, 0.90f, 1.0f, blinkAlpha}); // サイバーシアン
        }
        demoText_->Update();
    }
}

void LoadingScreen::Draw(IrufemiEngine* engine) {
    if (!engine) {
        return;
    }

    // ウィンドウのサイズに対応するため、描画時に画面サイズに合わせて位置やサイズを動的に更新する
    float screenW = static_cast<float>(engine->GetGameResolutionWidth());
    float screenH = static_cast<float>(engine->GetGameResolutionHeight());
    float uiScale = screenH / 720.0f;

    if (bgSprite_) {
        // 背景は必ず加算ではなく通常のブレンド（不透明）で上書き描画する
        engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
        bgSprite_->SetSize(screenW, screenH);
        bgSprite_->Draw();
    }

    // 文字とドットは加算合成（黒背景を透過）
    engine->SetBlend(Irufemi::BlendMode::kBlendModeAdd);
    if (nowLoadingText_) {
        nowLoadingText_->SetUIScale(uiScale);
        nowLoadingText_->SetPosition(screenW - 80.0f * uiScale, screenH - 45.0f * uiScale);
        nowLoadingText_->Draw();
    }

    float baseX = screenW - 65.0f * uiScale;
    float baseY = screenH - 45.0f * uiScale;
    for (int i = 0; i < dots_.size(); ++i) {
        dots_[i]->SetSize({8.0f * uiScale, 8.0f * uiScale});
        dots_[i]->SetPosition(Irufemi::Vector3{baseX + i * 20.0f * uiScale, baseY, 0.0f});
    }

    for (int i = 0; i < dotCount_; ++i) {
        if (i < dots_.size()) {
            dots_[i]->Draw();
        }
    }

    // 描画後、安全のために元の通常ブレンドに戻す
    engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);

    // アトラクトデモ稼働中ならローディング画面の最前面右上にシステムHUDを描画
    if (AttractDemoManager::IsAttractModeActive() && demoText_) {
        demoText_->SetPosition(screenW - 55.0f * uiScale, 42.0f * uiScale);
        demoText_->Draw();
    }
}
