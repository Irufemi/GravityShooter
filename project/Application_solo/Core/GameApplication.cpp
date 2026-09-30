#include "Core/GameApplication.h"

#include <memory>
#include <string>

#include "Engine/Irufemi.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/ComponentFactory.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "RailMechanics/RailRelativeFollowerComponent.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Environment/DebrisComponent.h"
#include "Player/TargetableComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Level/WaveManagerComponent.h"
#include "Player/GravityPlayerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Player/PlayerTargetingComponent.h"
#include "Combat/EnemySpawnerComponent.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Core/SceneTransitionButtonComponent.h"
#include "Effects/EffectManagerComponent.h"
#include "Environment/EnvironmentManagerComponent.h"
#include "UI/ReticleUIComponent.h"
#include "UI/LockonMarkerUIComponent.h"
#include "Combat/DroneManagerComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include "Core/GameLoopManagerComponent.h"
#include "Core/ResultManagerComponent.h"
#include "Combat/EnemyBeamComponent.h"
#include "Combat/EnemyBulletComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"

// エンジン機能
#include "RHI/DirectX12/ShaderManager.h"
#include "Renderer/Pipeline/PSOManager.h"

// UI
#include "UI/LoadingScreen.h"

// シーンのインクルード
#include "Scenes/title/TitleScene.h"
#include "Scenes/title/TitleMenuControllerComponent.h"
#include "Scenes/stageSelect/SelectScene.h"
#include "Scenes/inGame/GameScene.h"
#include "Scenes/Pause/PauseScene.h"
#include "Scenes/TL1/TL1Scene.h"
#include "Framework/Scene/OptionsScene.h"
#include "Scenes/HowToPlay/HowToPlayScene.h"

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
#include "Framework/Scene/DebugScene.h"
#endif
#include "Scenes/Result/ResultScene.h"

#ifdef EditorMode
#include "Core/EditorManager.h"
#include "Inspectors/ComponentEditorRegistry.h"
#include "Editor/WaveManagerComponentEditor.h"
#include "Editor/GravityPlayerComponentEditor.h"
#include "Editor/BossComponentEditor.h"
#endif

namespace {
// --- ゲーム固有の定数 ---
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;
const std::wstring kTitle = L"Application_solo";
const Irufemi::Vector4 kClearColor = {0.08f, 0.03f, 0.02f, 1.0f}; // 退廃的な荒野（ダーク・ラスト）
const char kInitialScene[]
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    = "InGame";
#else
    = "Title";
#endif

// --- シーン登録処理 ---
void RegisterScenes(SceneManager& sm) {
    sm.Register("Title", [] { return std::make_unique<TitleScene>(); });
    sm.Register("Select", [] { return std::make_unique<SelectScene>(); });
    sm.Register("InGame", [] { return std::make_unique<GameScene>(); });
    sm.Register("Pause", [] { return std::make_unique<PauseScene>(); });
    sm.Register("TL1", [] { return std::make_unique<TL1Scene>(); });
    sm.Register("Result", [] { return std::make_unique<ResultScene>(); });

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    sm.Register("Debug", [] { return std::make_unique<DebugScene>(); });
#endif
    sm.Register("OptionsScene", [] { return std::make_unique<OptionsScene>(); });
    sm.Register("HowToPlayScene", [] { return std::make_unique<HowToPlayScene>(); });
}

// --- シェーダー登録処理 ---
void RegisterShaders(IrufemiEngine& engine) {
    ShaderCompileOptions options;
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    options.isDebug = true;
#endif
    auto shaderManager = engine.GetDirectXCommon()->GetShaderManager();
    auto psoManager = engine.GetPSOManager();

    auto vs3d = shaderManager->GetOrCompile(L"Object3d.VS.hlsl", options);
    auto psEnergyCore = shaderManager->GetOrCompile(L"EnergyCore.PS.hlsl", options);
    psoManager->RegisterShader("EnergyCore", {{vs3d, psEnergyCore}});

    // EnergyBeam と LightningCrawl シェーダーの登録
    auto psEnergyBeam = shaderManager->GetOrCompile(L"EnergyBeam.PS.hlsl", options);
    psoManager->RegisterShader("EnergyBeam", {{vs3d, psEnergyBeam}});

    auto psLightningCrawl = shaderManager->GetOrCompile(L"LightningCrawl.PS.hlsl", options);
    psoManager->RegisterShader("LightningCrawl", {{vs3d, psLightningCrawl}});

    // LockonMarker用シェーダー (SpriteBatch.VS.hlsl を使う)
    auto vsSpriteBatch = shaderManager->GetOrCompile(L"SpriteBatch.VS.hlsl", options);
    auto psLuminanceAlpha = shaderManager->GetOrCompile(L"LuminanceAlpha2D.PS.hlsl", options);
    psoManager->RegisterShader("LuminanceAlpha2D", {{vsSpriteBatch, psLuminanceAlpha}});

    // GravitationalWave シェーダー (タイトル画面用 プロシージャル重力波＆高電圧プラズマ放電)
    auto vsFullscreen = shaderManager->GetOrCompile(L"Fullscreen.VS.hlsl", options);
    auto psGravitationalWave = shaderManager->GetOrCompile(L"GravitationalWave.PS.hlsl", options);
    psoManager->RegisterShader("GravitationalWave", {{vsFullscreen, psGravitationalWave}});
}

// --- コンポーネント登録処理 ---
void RegisterComponents() {
    ComponentFactory::Register("RailShooterPlayerComponent", "Game",
                               []() { return std::make_shared<RailShooterPlayerComponent>(); });
    ComponentFactory::Register("SplineFollowerComponent", "Game",
                               []() { return std::make_shared<SplineFollowerComponent>(); });
    ComponentFactory::Register("RailRelativeFollowerComponent", "Game",
                               []() { return std::make_shared<RailRelativeFollowerComponent>(); });
    ComponentFactory::Register("RailShooterEnemyComponent", "Game",
                               []() { return std::make_shared<RailShooterEnemyComponent>(); });
    ComponentFactory::Register("DebrisComponent", "Game", []() { return std::make_shared<DebrisComponent>(); });
    ComponentFactory::Register("TargetableComponent", "Game", []() { return std::make_shared<TargetableComponent>(); });
    ComponentFactory::Register("DebrisManagerComponent", "Game",
                               []() { return std::make_shared<DebrisManagerComponent>(); });
    ComponentFactory::Register("WaveManagerComponent", "Game",
                               []() { return std::make_shared<WaveManagerComponent>(); });
    ComponentFactory::Register("EnvironmentManagerComponent", "Game",
                               []() { return std::make_shared<EnvironmentManagerComponent>(); });
    ComponentFactory::Register("GravityPlayerComponent", "Game",
                               []() { return std::make_shared<GravityPlayerComponent>(); });
    ComponentFactory::Register("PlayerHealthComponent", "Game",
                               []() { return std::make_shared<PlayerHealthComponent>(); });
    ComponentFactory::Register("PlayerDamageVisualizerComponent", "Game",
                               []() { return std::make_shared<PlayerDamageVisualizerComponent>(); });
    ComponentFactory::Register("PlayerThrusterVisualizerComponent", "Game",
                               []() { return std::make_shared<PlayerThrusterVisualizerComponent>(); });
    ComponentFactory::Register("PlayerTargetingComponent", "Game",
                               []() { return std::make_shared<PlayerTargetingComponent>(); });
    ComponentFactory::Register("EnemySpawnerComponent", "Game",
                               []() { return std::make_shared<EnemySpawnerComponent>(); });
    ComponentFactory::Register("BossComponent", "Game", []() { return std::make_shared<BossComponent>(); });
    ComponentFactory::Register("BossDamageVisualizerComponent", "Game",
                               []() { return std::make_shared<BossDamageVisualizerComponent>(); });
    ComponentFactory::Register("SceneTransitionButtonComponent", "Game",
                               []() { return std::make_shared<SceneTransitionButtonComponent>(); });
    ComponentFactory::Register("EffectManagerComponent", "Game",
                               []() { return std::make_shared<EffectManagerComponent>(); });
    ComponentFactory::Register("ReticleUIComponent", "UI", []() { return std::make_shared<ReticleUIComponent>(); });
    ComponentFactory::Register("LockonMarkerUIComponent", "UI",
                               []() { return std::make_shared<LockonMarkerUIComponent>(); });
    ComponentFactory::Register("DroneManagerComponent", "Game",
                               []() { return std::make_shared<DroneManagerComponent>(); });
    ComponentFactory::Register("BossBulletManagerComponent", "Game",
                               []() { return std::make_shared<BossBulletManagerComponent>(); });
    ComponentFactory::Register("EnemyBeamComponent", "Game", []() { return std::make_shared<EnemyBeamComponent>(); });
    ComponentFactory::Register("EnemyBulletComponent", "Game",
                               []() { return std::make_shared<EnemyBulletComponent>(); });
    ComponentFactory::Register("EnemyBulletManagerComponent", "Game",
                               []() { return std::make_shared<EnemyBulletManagerComponent>(); });

    ComponentFactory::Register("GameLoopManagerComponent", "Game",
                               []() { return std::make_shared<GameLoopManagerComponent>(); });
    ComponentFactory::Register("ResultManagerComponent", "Game",
                               []() { return std::make_shared<ResultManagerComponent>(); });
    ComponentFactory::Register("TitleMenuControllerComponent", "Game",
                               []() { return std::make_shared<TitleMenuControllerComponent>(); });
}
} // namespace

GameApplication::GameApplication() = default;
GameApplication::~GameApplication() = default;

void GameApplication::Run() {
    // エンジンのインスタンスを生成
    auto engine = std::make_unique<IrufemiEngine>();

#ifdef EditorMode
    // エディタマネージャを拡張として事前登録（Initialize時に初期化される）
    auto editorManager = std::make_shared<EditorManager>();
    engine->AddExtension(editorManager);
#endif

    // エンジンの初期化
    engine->Initialize(kTitle, kClientWidth, kClientHeight, kClearColor);

#ifdef EditorMode
    // エンジン初期化後にエディタへ登録（OnInitialize内でレジストリが生成されるため）
    if (auto registry = editorManager->GetComponentEditorRegistry()) {
        registry->RegisterEditor<WaveManagerComponent, WaveManagerComponentEditor>();
        registry->RegisterEditor<GravityPlayerComponent, GravityPlayerComponentEditor>();
        registry->RegisterEditor<BossComponent, BossComponentEditor>();
    }
#endif

    // アプリ固有のシェーダー登録
    RegisterShaders(*engine);

    // 独自コンポーネントの登録
    RegisterComponents();
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
