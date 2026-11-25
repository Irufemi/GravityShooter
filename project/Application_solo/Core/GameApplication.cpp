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
#include "Level/StageIntroDirectorComponent.h"
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
#include "UI/ScoreHUDComponent.h"
#include "UI/PlayerStatusHUDComponent.h"
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
#include "Core/Utility/JsonUtility.h"
#include "Core/Utility/StringUtility.h"

// UI
#include "UI/LoadingScreen.h"
#include "UI/UISound.h"
#include "Audio/SoundHelper.h"

// シーンのインクルード
#include "Scenes/title/TitleScene.h"
#include "Scenes/title/TitleMenuControllerComponent.h"
#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Environment/NebulaSeaFloorComponent.h"
#include "Scenes/stageSelect/SelectScene.h"
#include "Scenes/inGame/GameScene.h"
#include "Scenes/inGame/ParticleShowcaseDirectorComponent.h"
#include "Scenes/Pause/PauseScene.h"
#include "Scenes/TL1/TL1Scene.h"
#include "Scenes/Options/OptionsScene.h"
#include "Scenes/HowToPlay/HowToPlayScene.h"
#include "Core/AttractDemoManager.h"

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

using namespace Irufemi;

namespace {
// --- ゲーム固有の定数 ---
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;
const std::wstring kTitle = L"Application_solo";
const Irufemi::Vector4 kClearColor = {0.08f, 0.03f, 0.02f, 1.0f}; // 退廃的な荒野（ダーク・ラスト）
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
constexpr std::string_view kInitialScene = GameScenes::InGame;
#else
constexpr std::string_view kInitialScene = GameScenes::Title;
#endif

// --- デモモード文字列のパース ---
AttractDemoManager::DemoMode ParseDemoMode(const std::string& modeStr) {
    if (modeStr == "ParticleShowcase") {
        return AttractDemoManager::DemoMode::ParticleShowcase;
    } else if (modeStr == "GameOverShowcase") {
        return AttractDemoManager::DemoMode::GameOverShowcase;
    } else if (modeStr == "TransitionShowcase") {
        return AttractDemoManager::DemoMode::TransitionShowcase;
    } else if (modeStr == "StandardAttract") {
        return AttractDemoManager::DemoMode::StandardAttract;
    }
    return AttractDemoManager::DemoMode::ClearShowcase;
}

// --- シーン登録処理 ---
void RegisterScenes(Irufemi::SceneManager& sm) {
    sm.Register(GameScenes::Title, [] { return std::make_unique<TitleScene>(); });
    sm.Register(GameScenes::Select, [] { return std::make_unique<SelectScene>(); });
    sm.Register(GameScenes::InGame, [] { return std::make_unique<GameScene>(); });
    sm.Register(GameScenes::ParticleShowcase, [] { return std::make_unique<GameScene>(); });
    sm.Register(GameScenes::Pause, [] { return std::make_unique<PauseScene>(); });
    sm.Register(GameScenes::TL1, [] { return std::make_unique<TL1Scene>(); });
    sm.Register(GameScenes::Result, [] { return std::make_unique<ResultScene>(); });

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    sm.Register(GameScenes::Debug, [] { return std::make_unique<Irufemi::DebugScene>(); });
#endif
    sm.Register(GameScenes::Options, [] { return std::make_unique<OptionsScene>(); });
    sm.Register(GameScenes::HowToPlay, [] { return std::make_unique<HowToPlayScene>(); });
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

    // 3D不透明パス用の MRT 共通設定 (Object3D と同一の 5面 G-Buffer レイアウト)
    Irufemi::PSOManager::PipelineStateDesc mrtDesc{};
    mrtDesc.numRenderTargets = 5;
    mrtDesc.rtvFormat1 = DXGI_FORMAT_R8G8B8A8_UNORM;     // Mask
    mrtDesc.rtvFormat2 = DXGI_FORMAT_R16G16B16A16_FLOAT; // Normal & Depth
    mrtDesc.rtvFormat3 = DXGI_FORMAT_R8G8B8A8_UNORM;     // Material
    mrtDesc.rtvFormat4 = DXGI_FORMAT_R16G16_FLOAT;       // Velocity

    // ボス撃破演出用: 3D空間ノイズ電流オーバーレイピクセルシェーダー (MRT 5面対応)
    auto psBossLightning = shaderManager->GetOrCompile(L"BossLightning.PS.hlsl", options);
    auto bossLightningDesc = mrtDesc;
    bossLightningDesc.shaders = {vs3d, psBossLightning};
    psoManager->RegisterShader("BossLightning", bossLightningDesc);

    // 3D空間量子化アーク放電シェーダー
    auto psElectricArc = shaderManager->GetOrCompile(L"ElectricArc.PS.hlsl", options);
    psoManager->RegisterShader("ElectricArc", {{vs3d, psElectricArc}});

    // LockonMarker用シェーダー (SpriteBatch.VS.hlsl を使う)
    auto vsSpriteBatch = shaderManager->GetOrCompile(L"SpriteBatch.VS.hlsl", options);
    auto psLuminanceAlpha = shaderManager->GetOrCompile(L"LuminanceAlpha2D.PS.hlsl", options);
    psoManager->RegisterShader("LuminanceAlpha2D", {{vsSpriteBatch, psLuminanceAlpha}});

    // CosmicNebula シェーダー (タイトル画面用 神秘的な深宇宙星雲背景)
    auto vsCosmicNebula = shaderManager->GetOrCompile(L"CosmicNebula.VS.hlsl", options);
    auto psCosmicNebula = shaderManager->GetOrCompile(L"CosmicNebula.PS.hlsl", options);
    psoManager->RegisterShader("CosmicNebula", {{vsCosmicNebula, psCosmicNebula}});

    // NebulaSeaFloor シェーダー (ゲームシーン用 3D星雲ガス海)
    auto vsNebulaSea = shaderManager->GetOrCompile(L"NebulaSeaFloor.VS.hlsl", options);
    auto psNebulaSea = shaderManager->GetOrCompile(L"NebulaSeaFloor.PS.hlsl", options);
    psoManager->RegisterShader("NebulaSeaFloor", {{vsNebulaSea, psNebulaSea}});
}

// --- コンポーネント登録処理 ---
void RegisterComponents() {
    Irufemi::ComponentFactory::Register("RailShooterPlayerComponent", "Game",
                                        []() { return std::make_shared<RailShooterPlayerComponent>(); });
    Irufemi::ComponentFactory::Register("SplineFollowerComponent", "Game",
                                        []() { return std::make_shared<SplineFollowerComponent>(); });
    Irufemi::ComponentFactory::Register("RailRelativeFollowerComponent", "Game",
                                        []() { return std::make_shared<RailRelativeFollowerComponent>(); });
    Irufemi::ComponentFactory::Register("RailShooterEnemyComponent", "Game",
                                        []() { return std::make_shared<RailShooterEnemyComponent>(); });
    Irufemi::ComponentFactory::Register("DebrisComponent", "Game",
                                        []() { return std::make_shared<DebrisComponent>(); });
    Irufemi::ComponentFactory::Register("TargetableComponent", "Game",
                                        []() { return std::make_shared<TargetableComponent>(); });
    Irufemi::ComponentFactory::Register("DebrisManagerComponent", "Game",
                                        []() { return std::make_shared<DebrisManagerComponent>(); });
    Irufemi::ComponentFactory::Register("WaveManagerComponent", "Game",
                                        []() { return std::make_shared<WaveManagerComponent>(); });
    Irufemi::ComponentFactory::Register("StageIntroDirectorComponent", "Game",
                                        []() { return std::make_shared<StageIntroDirectorComponent>(); });
    Irufemi::ComponentFactory::Register("EnvironmentManagerComponent", "Game",
                                        []() { return std::make_shared<EnvironmentManagerComponent>(); });
    Irufemi::ComponentFactory::Register("GravityPlayerComponent", "Game",
                                        []() { return std::make_shared<GravityPlayerComponent>(); });
    Irufemi::ComponentFactory::Register("PlayerHealthComponent", "Game",
                                        []() { return std::make_shared<PlayerHealthComponent>(); });
    Irufemi::ComponentFactory::Register("PlayerDamageVisualizerComponent", "Game",
                                        []() { return std::make_shared<PlayerDamageVisualizerComponent>(); });
    Irufemi::ComponentFactory::Register("PlayerThrusterVisualizerComponent", "Game",
                                        []() { return std::make_shared<PlayerThrusterVisualizerComponent>(); });
    Irufemi::ComponentFactory::Register("PlayerTargetingComponent", "Game",
                                        []() { return std::make_shared<PlayerTargetingComponent>(); });
    Irufemi::ComponentFactory::Register("EnemySpawnerComponent", "Game",
                                        []() { return std::make_shared<EnemySpawnerComponent>(); });
    Irufemi::ComponentFactory::Register("BossComponent", "Game", []() { return std::make_shared<BossComponent>(); });
    Irufemi::ComponentFactory::Register("BossDamageVisualizerComponent", "Game",
                                        []() { return std::make_shared<BossDamageVisualizerComponent>(); });
    Irufemi::ComponentFactory::Register("SceneTransitionButtonComponent", "Game",
                                        []() { return std::make_shared<SceneTransitionButtonComponent>(); });
    Irufemi::ComponentFactory::Register("EffectManagerComponent", "Game",
                                        []() { return std::make_shared<EffectManagerComponent>(); });
    Irufemi::ComponentFactory::Register("ReticleUIComponent", "UI",
                                        []() { return std::make_shared<ReticleUIComponent>(); });
    Irufemi::ComponentFactory::Register("LockonMarkerUIComponent", "UI",
                                        []() { return std::make_shared<LockonMarkerUIComponent>(); });
    Irufemi::ComponentFactory::Register("ScoreHUDComponent", "UI",
                                        []() { return std::make_shared<ScoreHUDComponent>(); });
    Irufemi::ComponentFactory::Register("PlayerStatusHUDComponent", "UI",
                                        []() { return std::make_shared<PlayerStatusHUDComponent>(); });
    Irufemi::ComponentFactory::Register("DroneManagerComponent", "Game",
                                        []() { return std::make_shared<DroneManagerComponent>(); });
    Irufemi::ComponentFactory::Register("BossBulletManagerComponent", "Game",
                                        []() { return std::make_shared<BossBulletManagerComponent>(); });
    Irufemi::ComponentFactory::Register("EnemyBeamComponent", "Game",
                                        []() { return std::make_shared<EnemyBeamComponent>(); });
    Irufemi::ComponentFactory::Register("EnemyBulletComponent", "Game",
                                        []() { return std::make_shared<EnemyBulletComponent>(); });
    Irufemi::ComponentFactory::Register("EnemyBulletManagerComponent", "Game",
                                        []() { return std::make_shared<EnemyBulletManagerComponent>(); });

    Irufemi::ComponentFactory::Register("GameLoopManagerComponent", "Game",
                                        []() { return std::make_shared<GameLoopManagerComponent>(); });
    Irufemi::ComponentFactory::Register("ResultManagerComponent", "Game",
                                        []() { return std::make_shared<ResultManagerComponent>(); });
    Irufemi::ComponentFactory::Register("TitleMenuControllerComponent", "Game",
                                        []() { return std::make_shared<TitleMenuControllerComponent>(); });
    Irufemi::ComponentFactory::Register("TitleCosmicNebulaComponent", "Game",
                                        []() { return std::make_shared<TitleCosmicNebulaComponent>(); });
    Irufemi::ComponentFactory::Register("TitleSceneDirectorComponent", "Game",
                                        []() { return std::make_shared<TitleSceneDirectorComponent>(); });
    Irufemi::ComponentFactory::Register("ParticleShowcaseDirectorComponent", "Game",
                                        []() { return std::make_shared<ParticleShowcaseDirectorComponent>(); });
    Irufemi::ComponentFactory::Register("NebulaSeaFloorComponent", "Environment",
                                        []() { return std::make_shared<NebulaSeaFloorComponent>(); });
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

    // デフォルト構成設定（フォールバック用）
    int32_t clientWidth = kClientWidth;
    int32_t clientHeight = kClientHeight;
    std::wstring title = kTitle;
    Irufemi::Vector4 clearColor = kClearColor;
    std::string initialScene = std::string(kInitialScene);

    AttractDemoConfig demoConfig;
    demoConfig.enabledOnStart = false; // デフォルトはOFF
    demoConfig.defaultMode = AttractDemoManager::DemoMode::ClearShowcase;
    demoConfig.enableDevHotkeys = true;

    // 外部設定ファイル（resources/config/app_config.json）の読み込み
    nlohmann::json appConfigJson;
    std::string configPath = "resources/config/app_config.json";
    if (!JsonUtility::LoadFromFile(configPath, appConfigJson)) {
        configPath = "project/Application_solo/resources/config/app_config.json";
        JsonUtility::LoadFromFile(configPath, appConfigJson);
    }

    if (!appConfigJson.is_null() && !appConfigJson.empty()) {
        if (appConfigJson.contains("window")) {
            const auto& wnd = appConfigJson["window"];
            if (wnd.contains("title") && wnd["title"].is_string()) {
                title = ConvertString(wnd["title"].get<std::string>());
            }
            if (wnd.contains("width") && wnd["width"].is_number_integer()) {
                clientWidth = wnd["width"].get<int32_t>();
            }
            if (wnd.contains("height") && wnd["height"].is_number_integer()) {
                clientHeight = wnd["height"].get<int32_t>();
            }
            if (wnd.contains("clearColor")) {
                clearColor = JsonUtility::ToVector4(wnd["clearColor"], clearColor);
            }
        }

        if (appConfigJson.contains("scene")) {
            const auto& scn = appConfigJson["scene"];
            if (scn.contains("initialScene") && scn["initialScene"].is_string()) {
                initialScene = scn["initialScene"].get<std::string>();
            }
        }

        if (appConfigJson.contains("demo")) {
            const auto& demo = appConfigJson["demo"];
            if (demo.contains("enabledOnStart") && demo["enabledOnStart"].is_boolean()) {
                demoConfig.enabledOnStart = demo["enabledOnStart"].get<bool>();
            }
            if (demo.contains("defaultMode") && demo["defaultMode"].is_string()) {
                demoConfig.defaultMode = ParseDemoMode(demo["defaultMode"].get<std::string>());
            }
            if (demo.contains("enableDevHotkeys") && demo["enableDevHotkeys"].is_boolean()) {
                demoConfig.enableDevHotkeys = demo["enableDevHotkeys"].get<bool>();
            }
        }
    }

#ifdef EditorMode
    // エディタモード実行時は、編集・検証作業を阻害しないよう起動時自動デモを強制無効化
    demoConfig.enabledOnStart = false;
#endif

    // コマンドライン引数（-demo または --kiosk）が指定されている場合はデモ起動へ上書き
    if (const wchar_t* cmdLine = GetCommandLineW()) {
        if (wcsstr(cmdLine, L"-demo") || wcsstr(cmdLine, L"--demo") || wcsstr(cmdLine, L"-kiosk") ||
            wcsstr(cmdLine, L"--kiosk")) {
            demoConfig.enabledOnStart = true;
        }
    }

    // アトラクトデモ＆キオスク固定展示マネージャを拡張として事前登録
    engine->AddExtension(std::make_shared<AttractDemoManager>(demoConfig));

    // エンジンの初期化
    engine->Initialize(title, clientWidth, clientHeight, clearColor);

#ifdef EditorMode
    // エンジン初期化後にエディタへ登録（OnInitialize内でレジストリが生成されるため）
    if (auto registry = editorManager->GetComponentEditorRegistry()) {
        registry->RegisterEditor<WaveManagerComponent, WaveManagerComponentEditor>();
        registry->RegisterEditor<GravityPlayerComponent, GravityPlayerComponentEditor>();
        registry->RegisterEditor<BossComponent, BossComponentEditor>();
    }
#endif

    // 入力アクション設定（データドリブンバインディング）の読み込み
    if (auto inputManager = engine->GetInputManager()) {
        if (!inputManager->LoadBindingsFromJson("resources/config/input_actions.json")) {
            inputManager->LoadBindingsFromJson("project/Application_solo/resources/config/input_actions.json");
        }
    }

    // アプリ固有のシェーダー登録
    RegisterShaders(*engine);

    // アプリ固有シェーダーの代表的ステートバリエーションを非同期プリウォーム (PSO Precaching)
    if (auto psoManager = engine->GetPSOManager()) {
        psoManager->PreWarmRegisteredShaders(engine->GetThreadPool());
    }

    // 独自コンポーネントの登録
    RegisterComponents();
    // UIの登録
    auto loadingScreen = std::make_shared<LoadingScreen>();
    loadingScreen->Initialize(engine.get());
    engine->SetLoadingScreen(loadingScreen);

    // UIサウンドシステムの初期化・アセット事前ロード
    UISound::Initialize(engine->GetAudioManager());
    UISound::Preload();

    // ゲームサウンドシステム（データドリブン定義）の初期化・事前ロード
    SoundHelper::Initialize(engine->GetAudioManager());
    SoundHelper::PreloadAll();

    // シーンの登録
    engine->SetSceneRegistrar(RegisterScenes);

    // 初期シーンの設定
    engine->SetInitialSceneName(initialScene);

    // ゲームループの実行
    engine->Execute();
}
