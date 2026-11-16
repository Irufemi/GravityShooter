#include "SelectScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// デストラクタ
SelectScene::~SelectScene() = default;

// 初期化
void SelectScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
}

// 更新
void SelectScene::Update() {
    Irufemi::BaseScene::Update();

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("Title", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void SelectScene::Draw() {
    Irufemi::BaseScene::Draw();
}
