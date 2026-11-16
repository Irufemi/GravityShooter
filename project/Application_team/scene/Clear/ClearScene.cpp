#include "ClearScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// デストラクタ
ClearScene::~ClearScene() = default;

// 初期化
void ClearScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
}

// 更新
void ClearScene::Update() {
    Irufemi::BaseScene::Update();

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("Select", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void ClearScene::Draw() {
    Irufemi::BaseScene::Draw();
}
