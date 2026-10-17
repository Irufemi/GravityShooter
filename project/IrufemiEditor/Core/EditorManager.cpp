#include "Core/EditorManager.h"

#ifdef EditorMode
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/MathFunction.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Physics/CollisionManager.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <filesystem>
#include <iostream>

// 分離したエディタパネル群
#include "Core/IEditorPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/EngineSettingsPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ProjectBrowserPanel.h"
#include "Panels/SceneViewPanel.h"

// Editor Core
#include "Commands/EditorActionManager.h"
#include "Commands/EditorShortcutManager.h"
#include "Core/EditorTheme.h"
#include "Inspectors/ComponentEditorRegistry.h"

// FontAwesome 用のヘッダーを含める
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"

static EditorManager* sInstance = nullptr;

EditorManager::EditorManager() {
    sInstance = this;
}

EditorManager::~EditorManager() {
    if (sInstance == this) {
        sInstance = nullptr;
    }
}

EditorManager* EditorManager::GetInstance() {
    return sInstance;
}

void EditorManager::OnInitialize(IrufemiEngine* engine) {
    engine_ = engine;
    engine_->SetPlayMode(false); // 初期はEditモード

    actionManager_ = std::make_unique<EditorActionManager>(this);
    shortcutManager_ = std::make_unique<EditorShortcutManager>(this, actionManager_.get());

    componentEditorRegistry_ = std::make_unique<ComponentEditorRegistry>();
    componentEditorRegistry_->RegisterAllEditors();

    // テーマの適用
    EditorTheme::ApplyDarkTheme();

    // 各パネルの生成と初期化
    panels_.push_back(std::make_unique<SceneViewPanel>());
    panels_.push_back(std::make_unique<HierarchyPanel>());
    panels_.push_back(std::make_unique<InspectorPanel>());
    panels_.push_back(std::make_unique<EngineSettingsPanel>());
    panels_.push_back(std::make_unique<ProjectBrowserPanel>());
    panels_.push_back(std::make_unique<ConsolePanel>());

    for (auto& panel : panels_) {
        panel->Initialize(this);
    }
}

std::shared_ptr<GameObject> EditorManager::GetSelectedObject() const {
    return engine_ ? engine_->GetSelectedObject() : nullptr;
}

void EditorManager::SetSelectedObject(std::shared_ptr<GameObject> obj) {
    if (engine_) {
        engine_->SetSelectedObject(obj);
    }
}

void EditorManager::ClearSelectedObject() {
    if (engine_) {
        engine_->SetSelectedObject(nullptr);
    }
}

void EditorManager::OnUpdate(float deltaTime) {
    // ゲームプレイ中のコンポーネント更新がすべて完了した安全なタイミングでプレイモードを終了
    if (isExitPlayModeRequested_) {
        isExitPlayModeRequested_ = false;
        ExitPlayMode();
        return;
    }

    if (isStepRequested_) {
        // 次のフレームで再び停止
        if (engine_) {
            engine_->SetTimeScale(0.0f);
        }
        isStepRequested_ = false;
    }

    if (shortcutManager_) {
        shortcutManager_->Update();
    }
}

void EditorManager::EnterPlayMode() {
    if (IsPrefabMode()) {
        Log::OutPutLog(std::cout, "PlayMode is disabled while editing a prefab. Please exit Prefab Mode first.");
        return;
    }

    if (!engine_ || !engine_->GetSceneManager()) {
        return;
    }
    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    if (!scene) {
        return;
    }

    std::string currentSceneName = engine_->GetSceneManager()->GetCurrent();

    // Play押下時に現在のシーンを実ファイルにも保存する
    if (!currentSceneName.empty()) {
        SceneSerializer::Save(scene, currentSceneName);
    }

    // 現在のシーン状態をバックアップ
    SceneSerializer::Save(scene, "temp/.temp_playmode");
    playModeStartSceneName_ = currentSceneName; // 開始時のシーンを記憶

    // Play開始時にシーンをクリーンな状態にリロードする
    ClearSelectedObject(); // 選択状態をクリア

    // GPUがすべての描画コマンドを完了するのを待機してからオブジェクトを破棄
    if (auto dxCommon = engine_->GetDirectXCommon()) {
        dxCommon->WaitForGPU();
    }
    if (auto baseScene = dynamic_cast<BaseScene*>(scene)) {
        baseScene->ClearGameObjects();
    }
    if (auto cm = engine_->GetCollisionManager()) {
        cm->Clear();
    }
    if (auto debugRenderer = engine_->GetDebugPrimitiveRenderer()) {
        debugRenderer->ClearInstances();
    }

    // 保存したばかりのバックアップから復元して、完全に初期化し直す
    SceneSerializer::Load(scene, "temp/.temp_playmode");

    playState_ = PlayState::Playing;
    engine_->SetPlayMode(true);
    engine_->SetTimeScale(1.0f); // 再生時は等倍
}

void EditorManager::ExitPlayMode() {
    if (!engine_ || !engine_->GetSceneManager()) {
        return;
    }

    std::string currentSceneName = engine_->GetSceneManager()->GetCurrent();

    // プレイモード中にシーンが変わっていた場合は元のシーンに戻す
    if (!playModeStartSceneName_.empty() && currentSceneName != playModeStartSceneName_) {
        engine_->GetSceneManager()->TransitionTo(playModeStartSceneName_, SceneTransition::Type::Fade, 0.0f);
        playState_ = PlayState::Stopped;
        engine_->SetPlayMode(false);
        engine_->SetTimeScale(1.0f);
        return;
    }

    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    if (!scene) {
        return;
    }

    // プレイモード中の選択状態をクリア
    ClearSelectedObject();

    // GPUがすべての描画コマンドを完了するのを待機してからオブジェクトを破棄する
    // （実行中のフレームで使われているリソースが削除されることによるクラッシュを防ぐため）
    if (auto dxCommon = engine_->GetDirectXCommon()) {
        dxCommon->WaitForGPU();
    }

    if (auto baseScene = dynamic_cast<BaseScene*>(scene)) {
        baseScene->ClearGameObjects();
    }
    if (auto cm = engine_->GetCollisionManager()) {
        cm->Clear();
    }
    if (auto debugRenderer = engine_->GetDebugPrimitiveRenderer()) {
        debugRenderer->ClearInstances();
    }

    // バックアップから復元
    SceneSerializer::Load(scene, "temp/.temp_playmode");
    playState_ = PlayState::Stopped;
    engine_->SetPlayMode(false);
    engine_->SetTimeScale(1.0f);
}

void EditorManager::TogglePauseMode() {
    if (playState_ == PlayState::Playing) {
        playState_ = PlayState::Paused;
        if (engine_) {
            engine_->SetTimeScale(0.0f); // 時を止める
        }
    } else if (playState_ == PlayState::Paused) {
        playState_ = PlayState::Playing;
        if (engine_) {
            engine_->SetTimeScale(1.0f); // 時を動かす
        }
    }
}

void EditorManager::EnterPrefabMode(const std::string& prefabPath) {
    if (!engine_ || !engine_->GetSceneManager()) {
        return;
    }
    if (IsPlayMode()) {
        ExitPlayMode();
    }

    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    auto baseScene = dynamic_cast<BaseScene*>(scene);
    if (!baseScene) {
        return;
    }

    // 0. PrefabMode突入前のメインシーンカメラ状態（位置・回転・アクティブカメラ名）を完全退避
    if (engine_ && engine_->GetCameraManager()) {
        auto cm = engine_->GetCameraManager();
        savedActiveCameraName_ = cm->GetActiveCameraName();
        if (auto activeCam = cm->GetActiveCamera()) {
            savedCameraTranslate_ = activeCam->GetTranslate();
            savedCameraRotate_ = activeCam->GetRotate();
            savedCameraFov_ = activeCam->GetFovY();
            hasSavedCameraState_ = true;
        }
    }

    // 1. 現在のメインシーン状態をバックアップ
    SceneSerializer::Save(scene, "temp/.temp_prefab_backup");

    ClearSelectedObject();

    // 2. GPU描画コマンド完了待機後に既存オブジェクトを一括破棄
    if (auto dxCommon = engine_->GetDirectXCommon()) {
        dxCommon->WaitForGPU();
    }
    baseScene->ClearGameObjects();

    // 3. Prefabの読み込み
    editingPrefabRoot_ = SceneSerializer::LoadPrefab(prefabPath);
    if (!editingPrefabRoot_) {
        Log::OutPutLog(std::cerr, "Failed to load prefab: " + prefabPath);
        // 読込失敗時はバックアップからシーンを復元して安全に脱出
        SceneSerializer::Load(scene, "temp/.temp_prefab_backup");
        baseScene->WarmUpRenderState();
        return;
    }

    // 表示名はファイル名（拡張子なし）に統一
    std::string prefabName = std::filesystem::path(prefabPath).stem().string();
    editingPrefabRoot_->SetName(prefabName);
    baseScene->AddGameObject(editingPrefabRoot_);

    // 4. PrefabStage 専用エディタカメラ（純粋な Viewport Camera）をプロビジョニング
    // シーン内 GameObject ではなく純粋なカメラとして登録することで、Update ループによる強制上書きを防ぎ、
    // OrbitCameraController による自由な視点操作（回転・パン・ドリー）を可能にする
    stageCamera_ = std::make_shared<Camera>();
    int clientWidth = engine_->GetGameResolutionWidth();
    int clientHeight = engine_->GetGameResolutionHeight();
    stageCamera_->Initialize(clientWidth > 0 ? clientWidth : 1280, clientHeight > 0 ? clientHeight : 720);
    stageCamera_->SetNearZ(0.1f);
    stageCamera_->SetFarZ(1000.0f);
    stageCamera_->SetFovY(45.0f * (Irufemi::Math::PI / 180.0f));

    if (engine_ && engine_->GetCameraManager()) {
        engine_->GetCameraManager()->AddCamera("__PrefabStageCamera__", stageCamera_);
        engine_->GetCameraManager()->SetActiveCamera("__PrefabStageCamera__");
    }

    // 5. 描画ステートの事前同期（Transform・GPUデータウォームアップ）
    baseScene->WarmUpRenderState();

    // 6. オブジェクト選択 & Auto-Framing（画面中央へ最適フォーカス）
    SetSelectedObject(editingPrefabRoot_);
    FramePrefabObject();

    editContext_ = EditContext::Prefab;
    editingPrefabPath_ = prefabPath;

    // 7. Undo/Redo履歴をクリア（シーンを跨ぐ不正なUndo操作によるクラッシュを防止）
    if (actionManager_) {
        actionManager_->ClearHistory();
    }
}

void EditorManager::SaveCurrent() {
    if (IsPrefabMode()) {
        if (editingPrefabRoot_) {
            std::string prefabName = std::filesystem::path(editingPrefabPath_).stem().string();
            editingPrefabRoot_->SetName(prefabName);

            SceneSerializer::SavePrefab(editingPrefabRoot_, editingPrefabPath_);
            SceneSerializer::ClearCache();
            Log::OutPutLog(std::cout, "Prefab saved successfully: " + editingPrefabPath_);
        }
    } else if (engine_ && engine_->GetSceneManager()) {
        std::string currentSceneName = engine_->GetSceneManager()->GetCurrent();
        if (!currentSceneName.empty()) {
            SceneSerializer::Save(engine_->GetSceneManager()->GetCurrentScene(), currentSceneName);
            Log::OutPutLog(std::cout, "Scene saved successfully: " + currentSceneName);
        }
    }
}

void EditorManager::ExitPrefabMode(bool saveChanges) {
    if (!engine_ || !engine_->GetSceneManager()) {
        return;
    }
    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    auto baseScene = dynamic_cast<BaseScene*>(scene);
    if (!baseScene) {
        return;
    }

    // 1. 決定論的プレハブ保存（保持している editingPrefabRoot_ のみを保存し、カメラ混入事故を完全防止）
    if (saveChanges && editingPrefabRoot_) {
        std::string prefabName = std::filesystem::path(editingPrefabPath_).stem().string();
        editingPrefabRoot_->SetName(prefabName);

        SceneSerializer::SavePrefab(editingPrefabRoot_, editingPrefabPath_);
        SceneSerializer::ClearCache();
        Log::OutPutLog(std::cout, "Prefab saved successfully: " + editingPrefabPath_);
    }

    ClearSelectedObject();
    editingPrefabRoot_ = nullptr;

    if (engine_ && engine_->GetCameraManager()) {
        engine_->GetCameraManager()->RemoveCamera("__PrefabStageCamera__");
    }
    stageCamera_ = nullptr;

    // 2. GPU待機とステージクリーンアップ
    if (auto dxCommon = engine_->GetDirectXCommon()) {
        dxCommon->WaitForGPU();
    }
    baseScene->ClearGameObjects();

    // 3. バックアップから元のメインシーンを完全復元
    SceneSerializer::Load(scene, "temp/.temp_prefab_backup");

    // 4. メインシーン内の全 CameraComponent を CameraManager に手動登録（Editモードでも確実に登録）
    if (engine_ && engine_->GetCameraManager()) {
        auto cm = engine_->GetCameraManager();
        for (const auto& obj : baseScene->GetGameObjects()) {
            if (obj && !obj->IsDestroyed()) {
                if (auto camComp = obj->GetComponent<CameraComponent>()) {
                    camComp->Start();
                }
            }
        }

        // 5. 退避していたメインシーンのカメラ名・位置・回転・FOV を完全復元
        if (hasSavedCameraState_) {
            if (!savedActiveCameraName_.empty() && cm->GetCamera(savedActiveCameraName_)) {
                cm->SetActiveCamera(savedActiveCameraName_);
            }
            if (auto activeCam = cm->GetActiveCamera()) {
                activeCam->SetTranslate(savedCameraTranslate_);
                activeCam->SetRotate(savedCameraRotate_);
                activeCam->SetFovY(savedCameraFov_);
            }
            hasSavedCameraState_ = false;
        }
    }

    baseScene->WarmUpRenderState();

    // 6. Undo/Redo履歴をクリア（メインシーン復元後のダングリング参照を防止）
    if (actionManager_) {
        actionManager_->ClearHistory();
    }

    editContext_ = EditContext::Scene;
    editingPrefabPath_ = "";
}

void EditorManager::FramePrefabObject() {
    if (!editingPrefabRoot_ || !stageCamera_) {
        return;
    }

    // プレハブのバウンディング球・サイズを算出
    float radius = 2.0f;
    Irufemi::Vector3 centerOffset{0.0f, 0.0f, 0.0f};

    if (auto sphere = editingPrefabRoot_->GetComponent<SphereColliderComponent>()) {
        float scale = 1.0f;
        if (auto trans = editingPrefabRoot_->GetTransform()) {
            scale = (std::max)({trans->GetWorldScale().x, trans->GetWorldScale().y, trans->GetWorldScale().z});
        }
        radius = (std::max)(radius, sphere->GetLocalRadius() * scale);
        centerOffset = sphere->GetLocalOffset();
    } else if (auto aabb = editingPrefabRoot_->GetComponent<AABBColliderComponent>()) {
        auto worldAABB = aabb->GetWorldAABB();
        Irufemi::Vector3 diff = Irufemi::Math::Subtract(worldAABB.max, worldAABB.min);
        radius = (std::max)(radius, Irufemi::Math::Length(diff) * 0.5f);
        centerOffset = Irufemi::Math::Multiply(0.5f, Irufemi::Math::Add(worldAABB.min, worldAABB.max));
    } else if (auto obb = editingPrefabRoot_->GetComponent<OBBColliderComponent>()) {
        auto worldOBB = obb->GetWorldOBB();
        radius = (std::max)(radius, Irufemi::Math::Length(worldOBB.size));
        centerOffset = obb->GetLocalOffset();
    } else if (auto trans = editingPrefabRoot_->GetTransform()) {
        float scale = (std::max)({trans->GetWorldScale().x, trans->GetWorldScale().y, trans->GetWorldScale().z});
        radius = (std::max)(radius, scale * 1.5f);
    }

    // 大手エンジン標準設計: Prefab編集画面の回転軸（ピボット）はプレハブの原点 (0, 0, 0)
    // カメラコントローラー（OrbitCameraController）に対して原点を中心としたフレーミングを要求
    RequestCameraFrame(Irufemi::Vector3{0.0f, 0.0f, 0.0f}, radius);
}

void EditorManager::OnDrawUI() {
    if (!engine_ || !engine_->GetMainRenderTexture()) {
        return;
    }

    // 1. 全画面を覆う DockSpace の背景ウィンドウを作成
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    // 背景ウィンドウの装飾を全て消すフラグ
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("Editor DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // DockSpace 機能の起動
    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");

    // --- レイアウトの初期化 (Reset Layout) ---
    static bool firstLayout = true;
    if (firstLayout) {
        firstLayout = false;
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
            resetLayout_ = true;
        }
    }

    if (resetLayout_) {
        resetLayout_ = false;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

        ImGuiID dock_main_id = dockspaceId;
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        ImGuiID dock_id_right =
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_id_bottom =
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow("Scene", dock_main_id);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
        ImGui::DockBuilderDockWindow("Engine Settings", dock_id_right);
        ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
        ImGui::DockBuilderDockWindow("Console", dock_id_bottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    if (editContext_ == EditContext::Prefab) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.3f, 0.6f, 1.0f));
        if (ImGui::BeginChild("PrefabModeBanner", ImVec2(0, 32), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Text("%s PREFAB MODE: %s", ICON_FA_CUBE, editingPrefabPath_.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 320.0f);

            if (ImGui::Button(ICON_FA_CROSSHAIRS " Focus (F)", ImVec2(90, 20))) {
                FramePrefabObject();
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save & Exit", ImVec2(100, 20))) {
                ExitPrefabMode(true);
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(80, 20))) {
                ExitPrefabMode(false);
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // パフォーマンスパネルの表示状態
    static bool showPerformancePanel = false;

    // メニューバー
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // 現在のシーン名を取得して保存/読込
            std::string currentSceneName = "";
            if (engine_ && engine_->GetSceneManager()) {
                currentSceneName = engine_->GetSceneManager()->GetCurrent();
            }

            if (IsPrefabMode()) {
                if (ImGui::MenuItem("Save Prefab", "Ctrl+S")) {
                    SaveCurrent();
                }
            } else {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                    SaveCurrent();
                }
                if (ImGui::MenuItem("Load Scene")) {
                    if (engine_ && engine_->GetSceneManager() && !currentSceneName.empty()) {
                        auto* scene = engine_->GetSceneManager()->GetCurrentScene();
                        if (scene) {
                            // 既存のオブジェクトを消してからロードしたい場合はここで処理が必要
                            SceneSerializer::Load(scene, currentSceneName);
                            ClearSelectedObject(); // ロードしたら選択を解除
                        }
                    }
                }
            }

            if (ImGui::MenuItem("Exit")) {
                // 終了処理（PostQuitMessage）
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::MenuItem("Create Empty")) {
                actionManager_->CreatePrimitiveObject("Empty");
            }
            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube")) {
                    actionManager_->CreatePrimitiveObject("Cube");
                }
                if (ImGui::MenuItem("Sphere")) {
                    actionManager_->CreatePrimitiveObject("Sphere");
                }
                if (ImGui::MenuItem("Cylinder")) {
                    actionManager_->CreatePrimitiveObject("Cylinder");
                }
                if (ImGui::MenuItem("Plane")) {
                    actionManager_->CreatePrimitiveObject("Plane");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Model (MeshRenderer)")) {
                    actionManager_->CreatePrimitiveObject("Model");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("2D Object")) {
                if (ImGui::MenuItem("Sprite")) {
                    actionManager_->CreatePrimitiveObject("Sprite");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            for (auto& panel : panels_) {
                bool open = panel->IsOpen();
                if (ImGui::MenuItem(panel->GetName(), nullptr, &open)) {
                    panel->SetOpen(open);
                }
            }
            ImGui::Separator();
            ImGui::MenuItem("Performance", nullptr, &showPerformancePanel);
            ImGui::Separator();
            if (ImGui::BeginMenu("Layout")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    resetLayout_ = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Load Default Layout")) {
                    const char* presetPath = "../IrufemiEngine/EngineResources/default_imgui.ini";
                    const char* currentIni = ImGui::GetIO().IniFilename;
                    if (currentIni && std::filesystem::exists(presetPath)) {
                        std::error_code ec;
                        std::filesystem::copy_file(presetPath, currentIni,
                                                   std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            Log::OutPutLog(std::cerr, "Failed to load preset: " + ec.message());
                            MessageBoxA(nullptr, ("Failed to load preset: " + ec.message()).c_str(), "Error",
                                        MB_OK | MB_ICONERROR);
                        } else {
                            // アプリ終了時にImGuiが現在の状態をファイルへ自動保存（上書き）してしまうのを防ぐ
                            ImGui::GetIO().IniFilename = nullptr;

                            Log::OutPutLog(std::cout, "Default layout has been loaded.");
                            MessageBoxA(nullptr,
                                        "Default layout has been loaded.\nThe application will now close to apply the "
                                        "clean layout. Please restart the app.",
                                        "Restart Required", MB_OK | MB_ICONINFORMATION);
                            PostQuitMessage(0);
                        }
                    }
                }
                if (ImGui::MenuItem("Save Current as Default")) {
                    const char* currentIni = ImGui::GetIO().IniFilename;
                    if (currentIni) {
                        ImGui::SaveIniSettingsToDisk(currentIni);
                        const char* presetPath = "../IrufemiEngine/EngineResources/default_imgui.ini";
                        if (std::filesystem::exists(currentIni)) {
                            std::error_code ec;
                            std::filesystem::copy_file(currentIni, presetPath,
                                                       std::filesystem::copy_options::overwrite_existing, ec);
                            if (ec) {
                                Log::OutPutLog(std::cerr, "Failed to save preset: " + ec.message());
                                MessageBoxA(nullptr, ("Failed to save preset: " + ec.message()).c_str(), "Error",
                                            MB_OK | MB_ICONERROR);
                            } else {
                                Log::OutPutLog(std::cout, "Default layout preset saved successfully!");
                                MessageBoxA(nullptr, "Default layout preset saved successfully!", "Success",
                                            MB_OK | MB_ICONINFORMATION);
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (engine_ && engine_->GetDebugPrimitiveRenderer()) {
            auto debugRenderer = engine_->GetDebugPrimitiveRenderer();
            if (ImGui::BeginMenu("Gizmos")) {
                bool isAllEnabled = debugRenderer->IsEnabled();
                if (ImGui::Checkbox("Show All Gizmos", &isAllEnabled)) {
                    debugRenderer->SetEnabled(isAllEnabled);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(G)");

                ImGui::Separator();

                if (ImGui::Button("Select All", ImVec2(90, 0))) {
                    debugRenderer->SetCategoryMask(static_cast<uint32_t>(DebugCategory::All));
                }
                ImGui::SameLine();
                if (ImGui::Button("Deselect All", ImVec2(90, 0))) {
                    debugRenderer->SetCategoryMask(0);
                }

                ImGui::Separator();

                auto drawCategoryItem = [&](const char* label, DebugCategory cat) {
                    bool enabled = debugRenderer->IsCategoryEnabled(cat);
                    if (ImGui::Checkbox(label, &enabled)) {
                        debugRenderer->SetCategoryEnabled(cat, enabled);
                    }
                };

                drawCategoryItem("Collision (Colliders)", DebugCategory::Collision);
                drawCategoryItem("Combat (Bullets/Hitboxes)", DebugCategory::Combat);
                drawCategoryItem("Particle (Emitters)", DebugCategory::Particle);
                drawCategoryItem("Level (Spawners/Triggers)", DebugCategory::Level);
                drawCategoryItem("Path (Spline Rails)", DebugCategory::Path);
                drawCategoryItem("General (Other)", DebugCategory::General);

                ImGui::EndMenu();
            }
        }

        // --- 中央への Play / Pause / Step / Stop コントロール配置 ---
        float playButtonWidth = 45.0f;
        float playButtonHeight = 20.0f;
        float playButtonsTotalWidth = playButtonWidth * 4.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f;
        ImGui::SameLine((ImGui::GetWindowWidth() - playButtonsTotalWidth) * 0.5f);

        // 少し下にオフセットを追加して、メニューバー内で上下の余白（パディング）を作る
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

        // Play ボタン
        bool canPlay = !IsPrefabMode();
        if (!canPlay) {
            ImGui::BeginDisabled();
        }
        if (playState_ == PlayState::Playing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        }
        if (ImGui::Button(ICON_FA_PLAY, ImVec2(playButtonWidth, playButtonHeight))) {
            if (playState_ == PlayState::Stopped) {
                EnterPlayMode();
            } else if (playState_ == PlayState::Paused) {
                TogglePauseMode();
            }
        }
        ImGui::PopStyleColor();
        if (!canPlay) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("PlayMode is disabled while in Prefab Mode.\nPlease exit Prefab Mode first.");
            }
        }

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f); // 同行でも念のため再度オフセット

        // Pause ボタン
        if (playState_ == PlayState::Paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.2f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        }
        if (ImGui::Button(ICON_FA_PAUSE, ImVec2(playButtonWidth, playButtonHeight))) {
            if (IsPlayMode()) {
                TogglePauseMode();
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

        // Step ボタン
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button(ICON_FA_FORWARD_STEP, ImVec2(playButtonWidth, playButtonHeight))) {
            if (playState_ == PlayState::Paused) {
                isStepRequested_ = true;
                if (engine_) {
                    engine_->SetTimeScale(1.0f); // 時を1フレームだけ動かす
                }
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

        // Stop ボタン
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button(ICON_FA_STOP, ImVec2(playButtonWidth, playButtonHeight))) {
            if (IsPlayMode()) {
                ExitPlayMode();
            }
        }
        ImGui::PopStyleColor(2);

        // プレイモード中のピッキング許可チェックボックス
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::Checkbox("Play Picking", &isPickingAllowedInPlayMode_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Allow selecting objects in SceneView during Play Mode (useful for debugging)");
        }

        ImGui::EndMenuBar();
    }

    // 2. 各種エディタパネルの描画
    for (auto& panel : panels_) {
        if (panel->IsOpen()) {
            panel->Draw();
        }
    }

    // 3. パフォーマンスパネルの描画
    if (showPerformancePanel) {
        ImGui::Begin("Performance", &showPerformancePanel, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        if (engine_) {
            ImGui::Text("Real Delta Time: %f", engine_->GetRealDeltaTime());
            ImGui::Text("Game Delta Time: %f (Scale: %.2f)", engine_->GetDeltaTime(), engine_->GetTimeScale());
        }
        ImGui::End();
    }

    // ショートカットキー 'G' で全デバッグ描画のトグル（テキスト入力中は無視）
    if (engine_ && engine_->GetDebugPrimitiveRenderer() && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
            auto debugRenderer = engine_->GetDebugPrimitiveRenderer();
            debugRenderer->SetEnabled(!debugRenderer->IsEnabled());
        }
    }

    ImGui::End(); // Editor DockSpace
}

#endif // EditorMode
