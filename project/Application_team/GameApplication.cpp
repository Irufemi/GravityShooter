#include "GameApplication.h"

#include <memory>
#include <string>

#include "Engine/Irufemi.h"
#include "Framework/Scene/SceneManager.h"

// memoryでの未定義

#include "Framework/Component/ComponentFactory.h"

#include "components/SceneTransitionButtonComponent.h"

// UI
#include "UI/LoadingScreen.h"

// シーンのインクルード
#include "scene/title/TitleScene.h"
#include "scene/stageSelect/SelectScene.h"
#include "scene/inGame/GameScene.h"
#include "scene/Clear/ClearScene.h"
#include "scene/GameOver/GameOverScene.h"
#include "scene/Pause/PauseScene.h"
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
#include "Framework/Scene/DebugScene.h"
#endif

#ifdef EditorMode
#include "Core/EditorManager.h"
#endif

namespace {
// --- ゲーム固有の定数 ---
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;
const std::wstring kTitle = L"Application_team";
const Irufemi::Vector4 kClearColor = {0.1f, 0.25f, 0.5f, 1.0f};
const char kInitialScene[]
#if defined(_DEBUG) || defined(DEVELOPMENT)
    = "Debug";
#else
    = "Title";
#endif

// --- シーン登録処理 ---
void RegisterScenes(SceneManager& sm) {
    sm.Register("Title", [] { return std::make_unique<TitleScene>(); });
    sm.Register("Select", [] { return std::make_unique<SelectScene>(); });
    sm.Register("InGame", [] { return std::make_unique<GameScene>(); });
    sm.Register("Clear", [] { return std::make_unique<ClearScene>(); });
    sm.Register("GameOver", [] { return std::make_unique<GameOverScene>(); });
    sm.Register("Pause", [] { return std::make_unique<PauseScene>(); });
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    sm.Register("Debug", [] { return std::make_unique<DebugScene>(); });
#endif
}
} // namespace

GameApplication::GameApplication() = default;
GameApplication::~GameApplication() = default;

void GameApplication::Run() {
    // エンジンのインスタンスを生成
    auto engine = std::make_unique<IrufemiEngine>();

#ifdef EditorMode
    // エディタマネージャを拡張として事前登録（Initialize時に初期化される）
    engine->AddExtension(std::make_shared<EditorManager>());
#endif

    // エンジンの初期化
    engine->Initialize(kTitle, kClientWidth, kClientHeight, kClearColor);

    // 独自コンポーネントの登録

    ComponentFactory::Register("SceneTransitionButtonComponent", "Game",
                               []() { return std::make_shared<SceneTransitionButtonComponent>(); });
    // UIの登録
    auto loadingScreen = std::make_shared<LoadingScreen>();
    loadingScreen->Initialize(engine.get());
    engine->SetLoadingScreen(loadingScreen);

    // シーンの登録
    engine->SetSceneRegistrar(RegisterScenes);

    // 初期シーンの設定
    engine->SetInitialSceneName(kInitialScene);

    // ゲームループの実行
    engine->Execute();
}
