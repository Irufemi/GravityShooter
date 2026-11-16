#include "TitleScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Irufemi.h"

// デストラクタ
TitleScene::~TitleScene() {}

// 初期化
void TitleScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void TitleScene::Update() {
    Irufemi::BaseScene::Update();

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("InGame", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void TitleScene::Draw() {
    Irufemi::BaseScene::Draw();
}
