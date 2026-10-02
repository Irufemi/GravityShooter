#include "Scenes/inGame/GameScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// ECSコンポーネントのインクルード
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Scene/SceneSerializer.h"

// デストラクタ
GameScene::~GameScene() = default;

// 初期化
void GameScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void GameScene::Update() {
    BaseScene::Update(); // これにより GameObject 群の Update が呼ばれる
}

void GameScene::Draw() {
    BaseScene::Draw(); // これにより GameObject 群の Draw が呼ばれる
}

void GameScene::OnSuspend() {
    BaseScene::OnSuspend();
    SetHUDActive(false); // ポーズ中・オーバーレイ表示中は自機レティクル等のHUDを確実に非表示化
}

void GameScene::OnResume() {
    BaseScene::OnResume();
    SetHUDActive(true); // ゲーム復帰時にHUDを再表示
}

void GameScene::SetHUDActive(bool active) {
    if (auto reticle = FindGameObject("Reticle")) {
        reticle->SetActive(active);
    }
    if (auto lockon = FindGameObject("LockonMarkerUI")) {
        lockon->SetActive(active);
    }
}
