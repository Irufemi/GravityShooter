#include "GameOverScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// デストラクタ
GameOverScene::~GameOverScene() = default;

// 初期化
void GameOverScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
}

// 更新
void GameOverScene::Update() {
    Irufemi::BaseScene::Update();

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("Title", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void GameOverScene::Draw() {
    Irufemi::BaseScene::Draw();
}
