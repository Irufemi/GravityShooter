#include "Scenes/title/TitleScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Irufemi.h"

#include "Platform/Input/InputManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Scenes/title/TitleMenuControllerComponent.h"

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

void TitleScene::OnSuspend() {
    BaseScene::OnSuspend();

    // 他のシーン（HowToPlayやOptions）が重なった時は、タイトル文字被りを防ぐためメニューUIを非表示化
    if (auto menuMgr = FindGameObject("MenuManager")) {
        if (auto ctrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
            ctrl->SetMenuVisible(false);
        }
    }
}

void TitleScene::OnResume() {
    BaseScene::OnResume();

    // スタックから復帰した時はメニューUIを再表示
    if (auto menuMgr = FindGameObject("MenuManager")) {
        if (auto ctrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
            ctrl->SetMenuVisible(true);
        }
    }
}
