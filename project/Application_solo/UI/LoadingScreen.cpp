#include "UI/LoadingScreen.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h"
#include "Renderer/Object/2D/Text/Text.h"
#include "Renderer/System/Core/Object2DResource.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/DrawManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/AttractDemoManager.h"
#include "Core/Math/Math.h"
#include <cmath>
#include <algorithm>

LoadingScreen::LoadingScreen() = default;
LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Finalize() {
    veilResource_.reset();
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

    float screenW = static_cast<float>(engine->GetGameResolutionWidth());
    float screenH = static_cast<float>(engine->GetGameResolutionHeight());

    camera_ = std::make_shared<Camera>();
    camera_->Initialize(engine->GetGameResolutionWidth(), engine->GetGameResolutionHeight());
    camera_->UpdateMatrix();

    // 1. 深遠な宇宙空間背景（漆黒ダークスペース）
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize("resources/whiteTexture.png");
    bgSprite_->SetColor({0.002f, 0.005f, 0.012f, 1.0f}); // 移動中の深宇宙
    bgSprite_->SetSize(screenW, screenH);
    bgSprite_->SetAnchor(0.0f, 0.0f);
    bgSprite_->SetPosition(0.0f, 0.0f);
    bgSprite_->SetTopMost(true);

    // 2. 重力水面膜（Gravitational Veil Surface）プロシージャルリソースの初期化
    // GPUピクセルシェーダー（customEffectType = 10）により1080p/4K全画面で直接描画
    veilResource_ = std::make_unique<Object2DResource>();
    veilResource_->SetTextureManager(engine->GetTextureManager());

    auto mat = veilResource_->GetMaterialData();
    if (mat) {
        mat->color = {1.0f, 1.0f, 1.0f, 1.0f};
        mat->enableLighting = false;
        mat->hasTexture = false;
        mat->lightingMode = 0;   // None
        mat->customEffectType = 10;
        mat->customEffectParam = 0.0f;
        mat->uvTransform = Irufemi::Math::MakeIdentity4x4();
    }

    auto& vList = veilResource_->GetVertexDataList();
    auto& iList = veilResource_->GetIndexDataList();
    vList.resize(4);
    // 左上
    vList[0].position = {0.0f, 0.0f, 0.0f, 1.0f};
    vList[0].texcoord = {0.0f, 0.0f};
    vList[0].color = {1.0f, 1.0f, 1.0f, 1.0f};
    // 右上
    vList[1].position = {screenW, 0.0f, 0.0f, 1.0f};
    vList[1].texcoord = {1.0f, 0.0f};
    vList[1].color = {1.0f, 1.0f, 1.0f, 1.0f};
    // 左下
    vList[2].position = {0.0f, screenH, 0.0f, 1.0f};
    vList[2].texcoord = {0.0f, 1.0f};
    vList[2].color = {1.0f, 1.0f, 1.0f, 1.0f};
    // 右下
    vList[3].position = {screenW, screenH, 0.0f, 1.0f};
    vList[3].texcoord = {1.0f, 1.0f};
    vList[3].color = {1.0f, 1.0f, 1.0f, 1.0f};

    iList = {0, 1, 2, 1, 3, 2};

    veilResource_->CreateResource();
    veilResource_->Map();

    // 3. テキスト & UI
    demoText_ = std::make_unique<Text>();
    demoText_->SetFontManagerInstance(engine->GetFontManager());
    demoText_->SetDrawManagerInstance(engine->GetDrawManager());
    demoText_->SetCameraManagerInstance(engine->GetCameraManager());
    demoText_->Initialize("toro_glitch");
    demoText_->SetAlignment(TextAlignment::Right);
    demoText_->SetBaseScale(22.0f);
    demoText_->SetTopMost(true);

    nowLoadingText_ = std::make_unique<Text>();
    nowLoadingText_->SetFontManagerInstance(engine->GetFontManager());
    nowLoadingText_->SetDrawManagerInstance(engine->GetDrawManager());
    nowLoadingText_->SetCameraManagerInstance(engine->GetCameraManager());
    nowLoadingText_->Initialize("toro_glitch");
    nowLoadingText_->SetAlignment(TextAlignment::Right);
    nowLoadingText_->SetBaseScale(20.0f);
    nowLoadingText_->SetText(L"NOW LOADING");
    nowLoadingText_->SetColor({0.30f, 0.92f, 1.0f, 0.95f});
    nowLoadingText_->SetTopMost(true);

    dots_.clear();
    for (int i = 0; i < 3; ++i) {
        auto dot = std::make_unique<Primitive2DObject>();
        dot->Initialize(Irufemi::Primitive2DType::Circle, "resources/whiteTexture.png");
        dot->SetColor({0.30f, 0.90f, 1.0f, 1.0f});
        dot->SetPivot({0.5f, 0.5f});
        float baseX = screenW - 65.0f;
        float baseY = screenH - 45.0f;
        dot->SetPosition(Irufemi::Vector3{baseX + i * 20.0f, baseY, 0.0f});
        dot->SetSize({8.0f, 8.0f});
        dot->SetTopMost(true);
        dots_.push_back(std::move(dot));
    }
}

void LoadingScreen::Update(float deltaTime) {
    animationTimer_ += deltaTime;

    camera_->Update();
    if (bgSprite_) {
        bgSprite_->Update();
    }

    if (nowLoadingText_) {
        float pulse = std::sin(animationTimer_ * 3.0f) * 0.12f + 0.88f;
        nowLoadingText_->SetColor({0.30f * pulse, 0.92f * pulse, 1.0f, 0.92f});
        nowLoadingText_->Update();
    }

    // ドットの波打つアルファウェーブ
    for (int i = 0; i < dots_.size(); ++i) {
        float wave = std::sin(animationTimer_ * 4.2f - static_cast<float>(i) * 0.85f) * 0.5f + 0.5f;
        float dAlpha = 0.20f + wave * 0.80f;
        dots_[i]->SetColor({0.30f, 0.92f, 1.0f, dAlpha});
        dots_[i]->Update();
    }

    // アトラクトデモHUD
    if (AttractDemoManager::IsAttractModeActive() && demoText_) {
        demoBlinkTimer_ += deltaTime * 2.8f;
        float pulse = std::sin(demoBlinkTimer_) * 0.5f + 0.5f;
        float blinkAlpha = 0.30f + pulse * 0.55f;

        if (AttractDemoManager::IsKioskModeActive()) {
            if (demoText_->GetText() != L"DEMO LOOP   |   PRESS F8 TO EXIT") {
                demoText_->SetText(L"DEMO LOOP   |   PRESS F8 TO EXIT");
            }
            demoText_->SetColor({1.0f, 0.85f, 0.25f, blinkAlpha});
        } else {
            if (demoText_->GetText() != L"AUTO DEMO   |   PRESS ANY KEY") {
                demoText_->SetText(L"AUTO DEMO   |   PRESS ANY KEY");
            }
            demoText_->SetColor({0.30f, 0.90f, 1.0f, blinkAlpha});
        }
        demoText_->Update();
    }
}

void LoadingScreen::Draw(IrufemiEngine* engine) {
    if (!engine) {
        return;
    }

    // 0. シーン遷移によるCameraManager空洞化の補填（2D描画の安全保障）
    if (auto camMgr = engine->GetCameraManager()) {
        if (!camMgr->GetActiveCamera()) {
            camMgr->AddCamera("LoadingCamera", camera_);
            camMgr->SetActiveCamera("LoadingCamera");
        }
    }

    float screenW = static_cast<float>(engine->GetGameResolutionWidth());
    float screenH = static_cast<float>(engine->GetGameResolutionHeight());
    float uiScale = screenH / 720.0f;

    // 1. 深遠な宇宙空間背景（漆黒ダークスペース）
    engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
    if (bgSprite_) {
        bgSprite_->SetColor({0.002f, 0.005f, 0.012f, 1.0f});
        bgSprite_->SetSize(screenW, screenH);
        bgSprite_->Draw();
    }

    // 2. 重力水面膜（Gravitational Veil Surface）の波打ち更新と直接描画
    // 2. 重力水面膜（Gravitational Veil Surface）プロシージャル描画
    // GPUピクセルシェーダー（customEffectType = 10）により全画面1080p/4Kで滑らかに流動
    if (veilResource_) {
        auto* mat = veilResource_->GetMaterialData();
        if (mat) {
            mat->customEffectType = 10;
            mat->customEffectParam = animationTimer_;
        }

        // 画面解像度追従
        auto& vList = veilResource_->GetVertexDataList();
        if (vList.size() == 4) {
            vList[1].position.x = screenW;
            vList[2].position.y = screenH;
            vList[3].position.x = screenW;
            vList[3].position.y = screenH;
        }

        engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
        veilResource_->UpdateTransform(*camera_);
        veilResource_->SyncBeforeDraw();
        engine->GetDrawManager()->SubmitTopMostSprite(veilResource_.get());
    }

    // 3. 右下 NOW LOADING テキスト ＆ ウェーブドット
    engine->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
    float dotBaseX = screenW - 75.0f * uiScale;
    float dotBaseY = screenH - 45.0f * uiScale;

    if (nowLoadingText_) {
        nowLoadingText_->SetPosition(dotBaseX - 15.0f * uiScale, dotBaseY - 10.0f * uiScale);
        nowLoadingText_->Draw();
    }

    for (int i = 0; i < dots_.size(); ++i) {
        dots_[i]->SetSize({7.0f * uiScale, 7.0f * uiScale});
        dots_[i]->SetPosition(Irufemi::Vector3{dotBaseX + i * 18.0f * uiScale, dotBaseY, 0.0f});
        dots_[i]->Draw();
    }

    // 4. アトラクトデモHUD（右上）
    if (AttractDemoManager::IsAttractModeActive() && demoText_) {
        demoText_->SetPosition(screenW - 55.0f * uiScale, 42.0f * uiScale);
        demoText_->Draw();
    }
}
