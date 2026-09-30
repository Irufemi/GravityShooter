#include "Level/WaveManagerComponent.h"
#include "Level/WaveEventHandlers.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/JsonUtility.h"
#include <iostream>
#include "Framework/Component/Utility/SplineComponent.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "Core/Math/MathFunction.h"
#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h"
#include "Resource/Model/ModelManager.h"
#include "Framework/Component/Logic/SpawnPointComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Combat/EnemySpawnerComponent.h"

WaveManagerComponent::WaveManagerComponent() {
    // デフォルトハンドラの登録（コンポーネント自身のストラテジーとして自己完結カプセル化）
    RegisterHandler("SpawnEnemy", std::make_shared<SpawnEnemyHandler>());
    RegisterHandler("PlayBGM", std::make_shared<PlayBGMHandler>());
}

void WaveManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Level Data Path", &levelDataPath_);
    RegisterGameObjectRef("Target Spline", &targetSplineID_);
    RegisterGameObjectRef("Enemy Spawner", &targetEnemySpawnerID_);
    RegisterProperty("Editor Preview Distance", &editorPreviewDistance_);
}

void WaveManagerComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);
    ReloadLevelData();
}

void WaveManagerComponent::Initialize() {
    // ハンドラがクリアされている等の場合のみフォールバック登録
    if (handlers_.empty()) {
        RegisterHandler("SpawnEnemy", std::make_shared<SpawnEnemyHandler>());
        RegisterHandler("PlayBGM", std::make_shared<PlayBGMHandler>());
    }

    ReloadLevelData();
}

std::shared_ptr<ModelBatchRendererComponent>
WaveManagerComponent::GetPreviewBatchRenderer(const std::string& modelPath) {
    auto engine = GetEngine();
    if (!engine) {
        return nullptr;
    }

    if (currentPreviewModelPath_ != modelPath || !previewBatch_) {
        currentPreviewModelPath_ = modelPath;
        if (!previewBatch_) {
            previewBatch_ = std::make_shared<ModelBatchRendererComponent>();
            previewBatch_->SetGameObject(gameObject_);
            previewBatch_->SetUseGPUCulling(false); // プレビュー用なのでカリング無効化
            previewBatch_->Initialize();
        }
        previewBatch_->LoadModel(modelPath);
    }

    return previewBatch_;
}

void WaveManagerComponent::Draw() {
#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
    auto engine = GetEngine();
    if (engine && engine->GetSelectedObject().get() == gameObject_) {
        auto scene = gameObject_->GetScene();
        if (!scene) {
            return;
        }

        if (previewBatch_) {
            previewBatch_->ClearInstances();
        }

        // 1. レールスプラインの取得: インスペクターで設定された Target Spline を唯一の真実（Single Source of
        // Truth）として使用
        SplineComponent* spline = nullptr;
        if (targetSplineID_ != 0) {
            auto splineObj = scene->FindGameObjectByID(targetSplineID_);
            if (splineObj) {
                spline = splineObj->GetComponent<SplineComponent>();
            }
        }

        // 2. プレイヘッドのギズモ描画
        if (spline && engine->GetDebugPrimitiveRenderer()) {
            Irufemi::Vector3 phPos = spline->GetPointAtDistance(editorPreviewDistance_);
            Irufemi::Vector3 scale = {3.0f, 3.0f, 3.0f};
            Irufemi::Matrix4x4 transform =
                Irufemi::Math::MakeAffineMatrix(scale, Irufemi::Vector3{0.0f, 0.0f, 0.0f}, phPos);
            Irufemi::Vector4 color = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow for Playhead
            engine->GetDebugPrimitiveRenderer()->AddCube(transform, color, DebugCategory::Level);
        }

        // 3. イベントのプレビュー描画（Target Spline を基準レールとして使用）
        if (spline) {
            auto drawEvent = [this, spline](const WaveEventData& ev) {
                auto it = handlers_.find(ev.eventType);
                if (it != handlers_.end() && it->second) {
                    Irufemi::Vector3 pos = spline->GetPointAtDistance(ev.triggerDistance);
                    Irufemi::Vector3 fwd = spline->GetTangentAtDistance(ev.triggerDistance);
                    Irufemi::Vector3 up = {0.0f, 1.0f, 0.0f};
                    Irufemi::Vector3 right = Irufemi::Math::Normalize(Irufemi::Math::Cross(up, fwd));

                    it->second->DrawEditorPreview(this, ev, pos, fwd, right);
                }
            };

            if (selectedEventIndex_ >= 0 && selectedEventIndex_ < (int)allEvents_.size()) {
                // タイムラインで選択中のイベントのみを単独プレビュー！
                drawEvent(allEvents_[selectedEventIndex_]);
            } else {
                // 未選択時は、プレイヘッド距離に最も近い直近の1イベントのみプレビュー
                const WaveEventData* nearestEv = nullptr;
                float minDiff = 150.0f;
                for (const auto& ev : allEvents_) {
                    float diff = std::abs(ev.triggerDistance - editorPreviewDistance_);
                    if (diff < minDiff) {
                        minDiff = diff;
                        nearestEv = &ev;
                    }
                }
                if (nearestEv) {
                    drawEvent(*nearestEv);
                }
            }
        }

        if (previewBatch_) {
            previewBatch_->Draw();
        }
    }
#endif
}

void WaveManagerComponent::ReloadLevelData() {
    std::priority_queue<WaveEventData, std::vector<WaveEventData>, std::greater<WaveEventData>> emptyQueue;
    std::swap(eventQueue_, emptyQueue);
    allEvents_.clear();

    LoadLevelData(levelDataPath_);
}

void WaveManagerComponent::LoadLevelData(const std::string& filePath) {
    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(filePath, j)) {
        Log::OutPutLog(std::cout, "[WaveManager] Failed to load level data: " + filePath + "\n");
        return;
    }

    if (j.contains("Stage1_LevelData") && j["Stage1_LevelData"].contains("Events")) {
        for (const auto& eventJson : j["Stage1_LevelData"]["Events"]) {
            WaveEventData data;
            data.triggerDistance = eventJson.value("TriggerDistance", 0.0f);
            data.eventType = eventJson.value("Type", "Unknown");

            data.parameters = eventJson;

            eventQueue_.push(data);
            allEvents_.push_back(data);
        }
    }
    Log::OutPutLog(std::cout,
                   "[WaveManager] Loaded " + std::to_string(eventQueue_.size()) + " events from " + filePath + "\n");
}

void WaveManagerComponent::RegisterHandler(const std::string& eventType, std::shared_ptr<IWaveEventHandler> handler) {
    handlers_[eventType] = handler;
}

void WaveManagerComponent::Update() {
    if (!hasCachedSpawnPoints_) {
        CacheSpawnPoints();
        hasCachedSpawnPoints_ = true;
    }

    auto engine = GetEngine();
    bool isPlayMode = engine && engine->IsPlayMode();
    if (!isPlayMode) {
        return; // エディタモードではイベントの消費とスポーンを行わない
    }

    if (cachedPlayerCart_.expired()) {
        // PlayerCart または Player にアタッチされている SplineFollowerComponent を探す
        auto scene = gameObject_->GetScene();
        if (scene) {
            auto cartObj = scene->FindGameObject("PlayerCart");
            if (cartObj) {
                cachedPlayerCart_ = cartObj;
            } else {
                auto playerObj = scene->FindGameObject("Player");
                if (playerObj) {
                    cachedPlayerCart_ = playerObj;
                }
            }
        }
    }

    auto playerObj = cachedPlayerCart_.lock();
    if (!playerObj) {
        return;
    }

    auto playerFollower = playerObj->GetComponent<SplineFollowerComponent>();
    if (!playerFollower) {
        return;
    }

    float currentDist = playerFollower->GetCurrentDistance();
    auto spline = playerFollower->GetCachedPath();

    // 進行距離が先頭イベントのトリガー距離を超えていたら発火
    while (!eventQueue_.empty()) {
        const auto& nextEvent = eventQueue_.top();
        if (currentDist >= nextEvent.triggerDistance) {
            // ハンドラを探して実行
            auto it = handlers_.find(nextEvent.eventType);
            if (it != handlers_.end() && it->second) {
                Irufemi::Vector3 pos = {0, 0, 0};
                Irufemi::Vector3 fwd = {0, 0, 1};
                Irufemi::Vector3 right = {1, 0, 0};

                if (spline) {
                    pos = spline->GetPointAtDistance(nextEvent.triggerDistance);
                    fwd = spline->GetTangentAtDistance(nextEvent.triggerDistance);
                    // 右ベクトルを計算 (上を Y軸(0,1,0) と仮定)
                    Irufemi::Vector3 up = {0.0f, 1.0f, 0.0f};
                    right = {up.y * fwd.z - up.z * fwd.y, up.z * fwd.x - up.x * fwd.z, up.x * fwd.y - up.y * fwd.x};
                    // 正規化
                    float len = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
                    if (len > 0.0001f) {
                        right.x /= len;
                        right.y /= len;
                        right.z /= len;
                    }
                }

                it->second->Execute(this, nextEvent, pos, fwd, right);
            } else {
                Log::OutPutLog(std::cout, "[WaveManager] Warning: No handler registered for event type: " +
                                              nextEvent.eventType + "\n");
            }

            eventQueue_.pop();
        } else {
            // Priority queue なので、先頭が条件を満たしていなければ以降も満たさない
            break;
        }
    }
}

void WaveManagerComponent::CacheSpawnPoints() {
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }

    spawnPointsMap_.clear();
    const auto& objs = scene->GetGameObjects();
    for (const auto& obj : objs) {
        if (!obj) {
            continue;
        }

        // 最初のバグ (this == 0xF) 対策: 生ポインタが異常に小さい値かどうかをチェック
        auto* rawPtr = obj.get();
        if (reinterpret_cast<uintptr_t>(rawPtr) < 0x1000) {
            Log::OutPutLog(std::cerr, "[WaveManager] CRITICAL ERROR: Caught invalid GameObject pointer (0x" +
                                          std::format("{:X}", reinterpret_cast<uintptr_t>(rawPtr)) +
                                          ") in CacheSpawnPoints!\n");
            continue;
        }

        if (!obj->GetIsActive()) {
            continue;
        }
        auto sp = obj->GetComponent<SpawnPointComponent>();
        if (sp) {
            spawnPointsMap_[sp->GetWaveId()].push_back(sp);
        }
    }
}

const std::vector<SpawnPointComponent*>& WaveManagerComponent::GetSpawnPoints(const std::string& waveId) const {
    static const std::vector<SpawnPointComponent*> emptyList;
    auto it = spawnPointsMap_.find(waveId);
    if (it != spawnPointsMap_.end()) {
        return it->second;
    }
    return emptyList;
}

EnemySpawnerComponent* WaveManagerComponent::GetEnemySpawner() const {
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (!scene) {
        return nullptr;
    }
    if (targetEnemySpawnerID_ != 0) {
        if (auto spawnerObj = scene->FindGameObjectByID(targetEnemySpawnerID_)) {
            if (auto spawner = spawnerObj->GetComponent<EnemySpawnerComponent>()) {
                return spawner;
            }
        }
    }
    // フォールバック: 従来のシーン内名前探索
    if (auto spawnerObj = scene->FindGameObject("EnemySpawner")) {
        return spawnerObj->GetComponent<EnemySpawnerComponent>();
    }
    return nullptr;
}

void WaveManagerComponent::OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {
    if (targetSplineID_ != 0) {
        auto it = idMap.find(targetSplineID_);
        if (it != idMap.end()) {
            targetSplineID_ = it->second;
        }
    }
    if (targetEnemySpawnerID_ != 0) {
        auto it = idMap.find(targetEnemySpawnerID_);
        if (it != idMap.end()) {
            targetEnemySpawnerID_ = it->second;
        }
    }
}

void WaveManagerComponent::SaveLevelData() {
    SaveLevelData(levelDataPath_);
}

void WaveManagerComponent::SaveLevelData(const std::string& filePath) {
    nlohmann::json j;
    auto& eventsJson = j["Stage1_LevelData"]["Events"];
    eventsJson = nlohmann::json::array();

    for (const auto& ev : allEvents_) {
        nlohmann::json eventJson = ev.parameters;
        eventJson["TriggerDistance"] = ev.triggerDistance;
        eventJson["Type"] = ev.eventType;
        eventsJson.push_back(eventJson);
    }

    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump(4);
    } else {
        Log::OutPutLog(std::cout, "[WaveManager] Failed to save level data: " + filePath + "\n");
    }
}
