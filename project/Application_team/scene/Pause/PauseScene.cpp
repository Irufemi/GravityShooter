#include "PauseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// デストラクタ
PauseScene::~PauseScene() = default;

// 初期化
void PauseScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
}

// 更新
void PauseScene::Update() {
    Irufemi::BaseScene::Update();

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("Clear", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void PauseScene::Draw() {
    Irufemi::BaseScene::Draw();
}
