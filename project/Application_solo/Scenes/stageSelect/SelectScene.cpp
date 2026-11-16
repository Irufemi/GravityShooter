#include "Scenes/stageSelect/SelectScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

SelectScene::~SelectScene() = default;

// 初期化
void SelectScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);
}

// 更新
void SelectScene::Update() {
    Irufemi::BaseScene::Update();
}

void SelectScene::Draw() {
    Irufemi::BaseScene::Draw();
}
