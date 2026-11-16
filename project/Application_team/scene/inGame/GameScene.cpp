#include "GameScene.h"
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
    Irufemi::BaseScene::Initialize(engine);

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void GameScene::Update() {
    Irufemi::BaseScene::Update(); // これにより GameObject 群の Update が呼ばれる

    // BackSpaceキーで遷移テスト
    if (IsKeyPressed(VK_F8)) {
        engine_->GetSceneManager()->TransitionTo("Pause", Irufemi::SceneTransition::Type::Slide, 1.0f);
    }
}

void GameScene::Draw() {
    Irufemi::BaseScene::Draw(); // これにより GameObject 群の Draw が呼ばれる
}
