#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneObjectRegistry.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"

#include "Renderer/Camera/OrbitCameraController.h"
#include "Renderer/Data/CameraForGPU.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/AreaLight.h"
#include "Framework/GameObject/GameObject.h"
#include "Physics/CollisionManager.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"

#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/TargetFollowComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/Utility/Log.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <regex>
#include <limits>

#ifdef USE_IMGUI
#include "Framework/UI/DebugUI.h"
#endif

BaseScene::BaseScene() {
    objectRegistry_ = std::make_unique<SceneObjectRegistry>();
}
BaseScene::~BaseScene() {
    ClearGameObjects();
}

std::shared_ptr<GameObject> BaseScene::FindGameObject(const std::string& name) {
    if (auto obj = objectRegistry_->FindByName(name)) {
        return obj;
    }
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    for (const auto& obj : pendingAdds_) {
        if (obj && !obj->IsDestroyed() && obj->GetName() == name) {
            return obj;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<GameObject>> BaseScene::FindGameObjects(const std::string& name) {
    auto result = objectRegistry_->FindAllByName(name);
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    for (const auto& obj : pendingAdds_) {
        if (obj && !obj->IsDestroyed() && obj->GetName() == name) {
            result.push_back(obj);
        }
    }
    return result;
}

std::shared_ptr<GameObject> BaseScene::FindGameObjectByID(uint64_t instanceId) {
    if (auto obj = objectRegistry_->FindById(instanceId)) {
        return obj;
    }
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    for (const auto& obj : pendingAdds_) {
        if (obj && obj->GetInstanceID() == instanceId && !obj->IsDestroyed()) {
            return obj;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<GameObject>> BaseScene::FindGameObjectsWithTag(const std::string& tag) {
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    std::vector<std::shared_ptr<GameObject>> all;
    all.reserve(gameObjects_.size() + pendingAdds_.size());
    all.insert(all.end(), gameObjects_.begin(), gameObjects_.end());
    all.insert(all.end(), pendingAdds_.begin(), pendingAdds_.end());
    return objectRegistry_->FindByTag(tag, all);
}

std::string BaseScene::GetUniqueObjectName(const std::string& baseName) {
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    return objectRegistry_->GenerateUniqueName(baseName, pendingAdds_);
}

void BaseScene::OnGameObjectNameChanged(const std::shared_ptr<GameObject>& obj, const std::string& oldName,
                                        const std::string& newName) {
    objectRegistry_->OnNameChanged(obj, oldName, newName);
}

void BaseScene::Initialize(IrufemiEngine* engine) {
    engine_ = engine;

    // --- カメラマネージャーの初期化はエンジン側で行われるため、ここではメインカメラの登録のみ行う ---
    auto mainCamera = std::make_shared<Camera>();
    mainCamera->Initialize(engine_->GetGameResolutionWidth(), engine_->GetGameResolutionHeight());
    mainCamera->SetTranslate({0.0f, 0.0f, -50.0f});
    mainCamera->UpdateMatrix();
    engine_->GetCameraManager()->AddCamera("Main", mainCamera);

    // --- デバッグカメラの初期化とマネージャーへの登録 ---
    debugCamera_ = std::make_shared<Camera>();
    debugCamera_->Initialize(engine_->GetGameResolutionWidth(), engine_->GetGameResolutionHeight());
    engine_->GetCameraManager()->AddCamera("Debug", debugCamera_);

    debugCameraController_ = std::make_unique<OrbitCameraController>();
    // 初期状態の同期
    debugCameraController_->SyncTargetFromCamera(debugCamera_.get(), 50.0f);

    // --- デフォルトライティングの初期化 ---
    directionalLight_ = std::make_unique<DirectionalLight>();
    directionalLight_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    directionalLight_->direction = {0.5f, -0.7f, 1.0f};
    if (engine_) {
        engine_->GetCollisionManager()->Initialize(engine_->GetDebugPrimitiveRenderer());
    }
}

void BaseScene::Update() {
    // デバッグカメラのトグル機能などをここに入れることも可能
    // 今回は各シーンが個別に実装しているケースを考慮し、Updateでのカメラ行列上書き処理を共通化
    // デバッグカメラの更新と切り替え処理
    if (isDebugCameraMode_) {
        debugCameraController_->UpdateCameraInput(debugCamera_.get(), engine_->GetInputManager());
    }
    engine_->GetCameraManager()->Update();

    bool isPlayMode = true;
#ifdef EditorMode
    if (engine_) {
        isPlayMode = engine_->IsPlayMode();
    }
#endif

    bool isPaused = false;
    if (engine_) {
        isPaused = (engine_->GetTimeScale() == 0.0f);
    }

    auto debugRenderer = engine_ ? engine_->GetDebugPrimitiveRenderer() : nullptr;
    if (debugRenderer && isPlayMode && !isPaused) {
        debugRenderer->BeginSimulationFrame();
    }

    // --- 遅延キューの処理 ---
    {
        std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
        for (auto& obj : pendingAdds_) {
            gameObjects_.push_back(obj);
            objectRegistry_->Register(obj);
            if (obj && !obj->IsSpawned()) {
                obj->NotifySpawned();
            }
        }
        pendingAdds_.clear();

        for (auto& obj : pendingRemoves_) {
            auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
            if (it != gameObjects_.end()) {
                gameObjects_.erase(it);
                objectRegistry_->Unregister(obj);
            }
        }
        pendingRemoves_.clear();
    }

    // --- Start() フェーズ ---
    if (isPlayMode) {
        for (auto& obj : gameObjects_) {
            if (obj && !obj->GetParent() && !obj->IsDestroyed() && !obj->IsStarted()) {
                obj->Start();
            }
        }
    }

    // --- GameObject の更新 (バッチ並列化) ---
    std::vector<GameObject*> updateTargets;
    updateTargets.reserve(gameObjects_.size());
    for (const auto& obj : gameObjects_) {
        if (obj && !obj->GetParent() && !obj->IsDestroyed()) {
            updateTargets.push_back(obj.get());
        }
    }

    const size_t updateCount = updateTargets.size();
    if (updateCount > 0) {
        ThreadPool* threadPool = engine_ ? engine_->GetThreadPool() : nullptr;
        size_t threadCount = threadPool ? threadPool->GetTotalThreadCount() : 0;

        // オブジェクト数が少数（16個以下）またはスレッドプール無しの場合は直列実行
        if (threadCount <= 1 || updateCount <= 16) {
            for (GameObject* obj : updateTargets) {
                obj->Update(isPlayMode);
            }
        } else {
            // スレッド数に応じて均等にチャンク分割（最大でも threadCount 個のタスクに集約）
            size_t numTasks = (std::min)(threadCount, (updateCount + 15) / 16);
            size_t chunkSize = (updateCount + numTasks - 1) / numTasks;

            std::vector<std::future<void>> updateFutures;
            updateFutures.reserve(numTasks);

            for (size_t taskIdx = 0; taskIdx < numTasks; ++taskIdx) {
                size_t start = taskIdx * chunkSize;
                size_t end = (std::min)(start + chunkSize, updateCount);
                if (start >= end) {
                    break;
                }

                updateFutures.push_back(threadPool->Enqueue([&updateTargets, start, end, isPlayMode]() {
                    for (size_t i = start; i < end; ++i) {
                        updateTargets[i]->Update(isPlayMode);
                    }
                }));
            }

            for (auto& future : updateFutures) {
                future.wait();
            }
        }
    }

    // --- Irufemi::Transform の DOD一括更新 ---
    // (GameObjectのUpdateによって移動した最新の位置を描画・当たり判定に反映させるため、必ずUpdateの後に呼ぶ)
    TransformComponent::UpdateAll();

    // PlayMode 時のみ衝突判定（イベント発火など）を行う
    if (isPlayMode && engine_) {
        engine_->GetCollisionManager()->CheckAllCollisions();
    } else if (engine_) {
        // 非プレイモード（エディタ・プレハブ編集モード）時もコライダーの登録・削除キューを同期
        engine_->GetCollisionManager()->FlushPendingCommands();
    }

    // 破棄フラグが立った子オブジェクトを一括削除 (GC)
    for (const auto& obj : gameObjects_) {
        if (obj) {
            obj->CleanupDestroyedChildren();
        }
    }

    // 破棄フラグが立ったオブジェクトを一括削除 (GC - C++20 std::erase_if) と同時にレジストリからも解除
    std::erase_if(gameObjects_, [this](const std::shared_ptr<GameObject>& obj) {
        if (!obj || obj->IsDestroyed()) {
            if (obj && objectRegistry_) {
                objectRegistry_->Unregister(obj);
            }
            return true;
        }
        return false;
    });

    if (debugRenderer && isPlayMode && !isPaused) {
        debugRenderer->EndSimulationFrame();
    }

    SubmitFrameData();
}

void BaseScene::Draw() {
    // --- GameObject の描画 (バッチ並列化) ---
    std::vector<GameObject*> drawTargets;
    drawTargets.reserve(gameObjects_.size());
    for (const auto& obj : gameObjects_) {
        if (obj && !obj->GetParent()) {
            drawTargets.push_back(obj.get());
        }
    }

    const size_t drawCount = drawTargets.size();
    if (drawCount > 0) {
        ThreadPool* threadPool = engine_ ? engine_->GetThreadPool() : nullptr;
        size_t threadCount = threadPool ? threadPool->GetTotalThreadCount() : 0;

        if (threadCount <= 1 || drawCount <= 16) {
            for (GameObject* obj : drawTargets) {
                obj->Draw();
            }
        } else {
            size_t numTasks = (std::min)(threadCount, (drawCount + 15) / 16);
            size_t chunkSize = (drawCount + numTasks - 1) / numTasks;

            std::vector<std::future<void>> drawFutures;
            drawFutures.reserve(numTasks);

            for (size_t taskIdx = 0; taskIdx < numTasks; ++taskIdx) {
                size_t start = taskIdx * chunkSize;
                size_t end = (std::min)(start + chunkSize, drawCount);
                if (start >= end) {
                    break;
                }

                drawFutures.push_back(threadPool->Enqueue([&drawTargets, start, end]() {
                    for (size_t i = start; i < end; ++i) {
                        drawTargets[i]->Draw();
                    }
                }));
            }

            for (auto& future : drawFutures) {
                future.wait();
            }
        }
    }

#ifdef EditorMode
    GameObject* selectedObj = nullptr;
    if (engine_) {
        auto sel = engine_->GetSelectedObject();
        if (sel) {
            selectedObj = sel.get();
        }
    }

    // 選択中のオブジェクトに対してアウトラインマスク用の描画コマンドを発行
    if (selectedObj) {
        selectedObj->DrawOutlineMask();
    }

    if (engine_) {
        engine_->GetCollisionManager()->DrawDebug(selectedObj);
    }
#elif defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    engine_->GetCollisionManager()->DrawDebug();
#endif

    if (engine_ && engine_->GetDebugPrimitiveRenderer()) {
        engine_->GetDebugPrimitiveRenderer()->Update();
        engine_->GetDebugPrimitiveRenderer()->Draw();
    }
}

void BaseScene::WarmUpRenderState() {
    // 1. 遅延登録キューのフラッシュ（ロード済みオブジェクトをリストに確定）
    {
        std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
        for (auto& obj : pendingAdds_) {
            gameObjects_.push_back(obj);
            objectRegistry_->Register(obj);
        }
        pendingAdds_.clear();

        for (auto& obj : pendingRemoves_) {
            auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
            if (it != gameObjects_.end()) {
                gameObjects_.erase(it);
                objectRegistry_->Unregister(obj);
            }
        }
        pendingRemoves_.clear();
    }

    // 2. 未実行オブジェクトの Start() を先行実行（Scene Priming: ゲーム実行中のみ開始し、Editモード時の暴発を防止）
    if (engine_ && engine_->IsPlayMode()) {
        for (auto& obj : gameObjects_) {
            if (obj && !obj->GetParent() && !obj->IsDestroyed() && !obj->IsStarted()) {
                obj->Start();
            }
        }
    }

    // 3. 全Transformのワールド行列を一括計算 (DOD) - 第1パス: 各オブジェクトの初期Transformを確定
    TransformComponent::UpdateAll();

    // 4. カメラ追従などの依存Transformの初期同期 (Camera Cut)
    // 確定したターゲットのワールド座標を用いて、カメラを即座にスナップ
    for (auto& obj : gameObjects_) {
        if (obj && !obj->IsDestroyed()) {
            if (auto followComp = obj->GetComponent<TargetFollowComponent>()) {
                followComp->SnapToTarget();
            }
        }
    }

    // 5. カメラマネージャーの最新化
    if (engine_ && engine_->GetCameraManager()) {
        engine_->GetCameraManager()->Update();
    }

    // 6. 全GameObjectの描画前ステート同期（スキニングの初期ポーズ計算 & ComputeTask先行登録 & マスク描画パラメータ初期化）
    for (const auto& obj : gameObjects_) {
        if (obj && !obj->GetParent() && !obj->IsDestroyed()) {
            obj->SyncRenderState();
            auto maskComps = obj->GetComponentsInChildren<EffectMaskComponent>();
            for (auto mask : maskComps) {
                if (mask) {
                    mask->Update();
                }
            }
        }
    }

    // 7. 初回フレームデータ（カメラ行列、ライト、フォグ等）をレンダラーへ提出
    SubmitFrameData();

    // 6. 演出サブシステム（Voxel / GPU Particle）のシーン開始前ウォームアップ
    if (engine_) {
        if (auto vpm = engine_->GetVoxelParticleManager()) {
            vpm->WarmUp();
        }
        if (auto gpm = engine_->GetGPUParticleManager()) {
            gpm->WarmUp();
        }
    }
}

void BaseScene::AddGameObject(std::shared_ptr<GameObject> obj) {
    if (obj) {
        std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
        obj->SetScene(this);
        if (std::find(pendingAdds_.begin(), pendingAdds_.end(), obj) == pendingAdds_.end() &&
            std::find(gameObjects_.begin(), gameObjects_.end(), obj) == gameObjects_.end()) {
            pendingAdds_.push_back(obj);
        }
    }
}

void BaseScene::InsertGameObject(std::shared_ptr<GameObject> obj, size_t index) {
    // Insert は直接 gameObjects_ を操作するため、今回はそのまま mutex で保護し直接追加（または仕様に合わせて変更）。
    // 基本的に実行時の並行 Insert は想定しないが、安全のためロック。
    if (!obj) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    obj->SetScene(this);
    if (index >= gameObjects_.size()) {
        gameObjects_.push_back(obj);
    } else {
        gameObjects_.insert(gameObjects_.begin() + index, obj);
    }
    objectRegistry_->Register(obj);
}

void BaseScene::RemoveGameObject(std::shared_ptr<GameObject> obj) {
    if (!obj) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    if (std::find(pendingRemoves_.begin(), pendingRemoves_.end(), obj) == pendingRemoves_.end()) {
        pendingRemoves_.push_back(obj);
    }
}

void BaseScene::ClearGameObjects() {
    std::lock_guard<std::recursive_mutex> lock(sceneMutex_);
    for (auto& obj : gameObjects_) {
        if (obj && !obj->IsDestroyed()) {
            obj->Destroy();
        }
    }
    for (auto& obj : pendingAdds_) {
        if (obj && !obj->IsDestroyed()) {
            obj->Destroy();
        }
    }
    gameObjects_.clear();
    pendingAdds_.clear();
    pendingRemoves_.clear();
    objectRegistry_->Clear();

    // シーン上の全オブジェクトが消去されたため、エンジン側の物理・描画・パーティクルの残留データを完全にクリア
    if (engine_) {
        if (auto cm = engine_->GetCollisionManager()) {
            cm->Clear();
        }
        if (auto dm = engine_->GetDrawManager()) {
            dm->ClearAllQueues();
        }
        if (auto pm = engine_->GetGPUParticleManager()) {
            pm->ClearAllParticles();
        }
        if (auto vm = engine_->GetVoxelParticleManager()) {
            vm->Clear();
        }
    }
}

size_t BaseScene::GetGameObjectIndex(std::shared_ptr<GameObject> obj) const {
    auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
    if (it != gameObjects_.end()) {
        return std::distance(gameObjects_.begin(), it);
    }
    return (std::numeric_limits<size_t>::max)();
}

void BaseScene::SubmitFrameData() {
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    CameraForGPU cameraForGpu;
    cameraForGpu.view = activeCam->GetViewMatrix();
    cameraForGpu.projection = activeCam->GetPerspectiveFovMatrix();
    cameraForGpu.worldPosition = activeCam->GetTranslate();

    std::vector<PointLight*> pLights;
    for (const auto& light : pointLights_) {
        pLights.push_back(light.get());
    }
    std::vector<SpotLight*> sLights;
    for (const auto& light : spotLights_) {
        sLights.push_back(light.get());
    }
    std::vector<AreaLight*> aLights;
    for (const auto& light : areaLights_) {
        aLights.push_back(light.get());
    }

    if (directionalLight_) {
        engine_->GetDrawManager()->SetFrameData(cameraForGpu, engine_->GetGameTime(), engine_->GetGameDeltaTime(),
                                                *directionalLight_, pLights, sLights, aLights,
                                                {activeCam->GetViewportWidth(), activeCam->GetViewportHeight()});
    }
}

void BaseScene::DrawDebugTabItem() {
#ifdef USE_IMGUI
    if (ImGui::BeginTabBar("SceneSubTabs")) {
        // --- 1. Camera タブ ---
        if (ImGui::BeginTabItem("Camera")) {
            bool prevMode = isDebugCameraMode_;
            if (ImGui::Checkbox("Debug Camera Mode", &isDebugCameraMode_)) {
                if (isDebugCameraMode_ && !prevMode) {
                    // デバッグモードON時: 現在のアクティブカメラの名前を記憶し、状態をコピーする
                    previousActiveCameraName_ = engine_->GetCameraManager()->GetActiveCameraName();
                    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
                    if (activeCam && activeCam != debugCamera_.get()) {
                        debugCamera_->SetTranslate(activeCam->GetTranslate());
                        debugCamera_->SetRotate(activeCam->GetRotate());
                        debugCamera_->SetViewMatrix(activeCam->GetViewMatrix());
                        debugCamera_->SetPerspectiveFovMatrix(activeCam->GetPerspectiveFovMatrix());
                        debugCameraController_->SyncTargetFromCamera(debugCamera_.get());
                    }
                    engine_->GetCameraManager()->SetActiveCamera("Debug");
                } else if (!isDebugCameraMode_ && prevMode) {
                    // デバッグモードOFF時: 記憶しておいたカメラに戻す
                    if (previousActiveCameraName_.empty() || previousActiveCameraName_ == "Debug") {
                        previousActiveCameraName_ = "Main";
                    }
                    engine_->GetCameraManager()->SetActiveCamera(previousActiveCameraName_);
                }
            }
            if (isDebugCameraMode_ && debugCameraController_ && debugCamera_) {
                if (ImGui::Button("Top-Down")) {
                    debugCameraController_->SetPreset(OrbitCameraController::Preset::TopDown, debugCamera_.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Diagonal")) {
                    debugCameraController_->SetPreset(OrbitCameraController::Preset::Diagonal, debugCamera_.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Front")) {
                    debugCameraController_->SetPreset(OrbitCameraController::Preset::Front, debugCamera_.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Sync to Main")) {
                    Camera* mainCam = engine_->GetCameraManager()->GetCamera("Main");
                    if (mainCam) {
                        debugCamera_->SetTranslate(mainCam->GetTranslate());
                        debugCamera_->SetRotate(mainCam->GetRotate());
                        debugCameraController_->SyncTargetFromCamera(debugCamera_.get());
                    }
                }
                ImGui::Separator();
                ImGui::Text("Debug Camera Controls (Orbit/Pan/Zoom)");
                debugCamera_->DrawDebugContents();
            } else {
                Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
                if (activeCam) {
                    activeCam->DrawDebugContents();
                }
            }
            ImGui::EndTabItem();
        }

        // --- 2. Lights タブ (内部で BeginTabItem("Lights") を実行) ---
        DebugUI::DebugLights(directionalLight_.get(), pointLights_, spotLights_, areaLights_);

        ImGui::EndTabBar();
    }
#endif
}

// ── 入力ヘルパ ──
bool BaseScene::DownVK(uint8_t vk) const {
    return engine_->GetInputManager()->IsKeyDown(vk);
}
bool BaseScene::PressedVK(uint8_t vk) const {
    return engine_->GetInputManager()->IsKeyPressed(vk);
}
bool BaseScene::ReleasedVK(uint8_t vk) const {
    return engine_->GetInputManager()->IsKeyReleased(vk);
}

std::shared_ptr<GameObject> BaseScene::InstantiatePrefab(const std::string& prefabPath,
                                                         const Irufemi::Vector3& position) {
    auto obj = SceneSerializer::LoadPrefab(prefabPath);
    if (obj) {
        // プレハブリンク機能により、プレハブ由来でもシリアライズ可能とする
        obj->SetIsSerializable(true);
        obj->SetSourcePrefabPath(prefabPath);

        if (auto transform = obj->GetComponent<TransformComponent>()) {
            transform->SetPosition(position);
        }
        AddGameObject(obj);
    }
    return obj;
}

bool BaseScene::DownDIK(uint8_t dik) const {
    return engine_->GetInputManager()->IsKeyDownDIK(dik);
}
bool BaseScene::PressedDIK(uint8_t dik) const {
    return engine_->GetInputManager()->IsKeyPressedDIK(dik);
}
bool BaseScene::ReleasedDIK(uint8_t dik) const {
    return engine_->GetInputManager()->IsKeyReleasedDIK(dik);
}

bool BaseScene::IsButtonDown(unsigned short button) const {
    return engine_->GetInputManager()->IsButtonDown(button);
}
bool BaseScene::IsButtonPressed(unsigned short button) const {
    return engine_->GetInputManager()->IsButtonPressed(button);
}

nlohmann::json BaseScene::Serialize() const {
    nlohmann::json j;
    j["version"] = 1;

    nlohmann::json settings = nlohmann::json::object();
    if (directionalLight_) {
        nlohmann::json dl;
        dl["color"] = {directionalLight_->color.x, directionalLight_->color.y, directionalLight_->color.z,
                       directionalLight_->color.w};
        dl["direction"] = {directionalLight_->direction.x, directionalLight_->direction.y,
                           directionalLight_->direction.z};
        dl["intensity"] = directionalLight_->intensity;
        settings["directionalLight"] = dl;
    }
    // settings が空でなければ追加（今回は DirectionalLight が必ずある前提）
    if (!settings.empty()) {
        j["settings"] = settings;
    }

    nlohmann::json goArray = nlohmann::json::array();
    for (const auto& obj : gameObjects_) {
        // 親がいない（ルートの）オブジェクトのみをシリアライズ（子は再帰的に処理される）
        if (obj && !obj->GetParent() && !obj->IsDestroyed() && obj->IsSerializable()) {
            goArray.push_back(obj->Serialize());
        }
    }
    j["gameObjects"] = goArray;

    return j;
}

void BaseScene::Deserialize(const nlohmann::json& j) {
    if (j.contains("settings")) {
        const auto& settings = j["settings"];
        if (settings.contains("directionalLight")) {
            const auto& dl = settings["directionalLight"];
            if (!directionalLight_) {
                directionalLight_ = std::make_unique<DirectionalLight>();
            }
            if (dl.contains("color") && dl["color"].size() == 4) {
                directionalLight_->color = {dl["color"][0], dl["color"][1], dl["color"][2], dl["color"][3]};
            }
            if (dl.contains("direction") && dl["direction"].size() == 3) {
                directionalLight_->direction = {dl["direction"][0], dl["direction"][1], dl["direction"][2]};
            }
            if (dl.contains("intensity")) {
                directionalLight_->intensity = dl["intensity"];
            }
        }
    }

    // 古い形式（ルートが配列）への後方互換性対応
    const nlohmann::json* goArray = nullptr;
    if (j.is_array()) {
        goArray = &j;
    } else if (j.contains("gameObjects") && j["gameObjects"].is_array()) {
        goArray = &j["gameObjects"];
    } else {
        Log::OutPutLog(std::cerr, "[BaseScene] Warning: Invalid JSON format. 'gameObjects' array not found.\n");
    }

    if (goArray) {
        for (const auto& objJson : *goArray) {
            auto obj = std::make_shared<GameObject>();
            obj->SetScene(this);
            obj->Deserialize(objJson);
            obj->Initialize();
            AddGameObject(obj);
        }
    }
}

void BaseScene::Finalize() {
    ClearGameObjects();
}
