#include "Scenes/title/TitleScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Irufemi.h"

#include "Platform/Input/InputManager.h"

// デストラクタ
TitleScene::~TitleScene() {}

// 初期化
void TitleScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void TitleScene::Update() {
    BaseScene::Update();
}

void TitleScene::Draw() {
    BaseScene::Draw();
}
