#include "Scenes/Result/ResultScene.h"
#include "Core/ResultManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/Irufemi.h"

// ライフサイクル管理と状態クリーンアップ
ResultScene::~ResultScene() {
    s_isClearPending_ = false;
    s_currentSessionClear_ = false;
    isClearResult_ = false;
}

void ResultScene::OnExit() {
    BaseScene::OnExit();
    s_isClearPending_ = false;
    s_currentSessionClear_ = false;
}

void ResultScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // 静的保留フラグからインスタンス状態へ引き渡し、静的フラグはワンタイム消費して初期化（状態残留防止）
    isClearResult_ = s_isClearPending_;
    s_currentSessionClear_ = isClearResult_;
    s_isClearPending_ = false;

    // 本来は "Result.json" などから DataDriven にロードすべきですが、
    // 今回は空のシーンに対して、UI・進行を管理する ResultManagerComponent を持った
    // GameObject を一つだけ生成して配置します。
    auto managerObj = std::make_shared<GameObject>("ResultManager");
    AddGameObject(managerObj);

    auto t = managerObj->GetTransform();
    if (t) {
        t->SetPosition({640.0f, 340.0f, 0.0f});
    }

    // ResultManagerComponent にすべてのロジックを委譲する
    managerObj->AddComponent<ResultManagerComponent>();

    managerObj->Initialize();
}
