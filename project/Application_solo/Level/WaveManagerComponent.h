#pragma once
#include "Framework/Component/Component.h"
#include "Level/WaveEvent.h"
#include <queue>
#include <unordered_map>
#include <memory>
#include <string>

class SplineFollowerComponent;
class SpawnPointComponent;
class ModelBatchRendererComponent;
class EnemySpawnerComponent;

/**
 * @class WaveManagerComponent
 * @brief 外部JSONデータに基づき、レール上の進行距離に応じてイベントをディスパッチする
 */
class WaveManagerComponent : public Component {
public:
    WaveManagerComponent();
    ~WaveManagerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    bool CanUpdateInEditMode() const override {
        return true;
    }

    void Deserialize(const nlohmann::json& j) override;
    std::string GetComponentName() const override {
        return "WaveManagerComponent";
    }
    void OnRegisterProperties() override;
    void OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) override;

    void ReloadLevelData();

    std::shared_ptr<ModelBatchRendererComponent> GetPreviewBatchRenderer(const std::string& modelPath);

    /**
     * @brief 特定のイベント種類に対するハンドラを登録する
     */
    void RegisterHandler(const std::string& eventType, std::shared_ptr<IWaveEventHandler> handler);

    /**
     * @brief シーン内の SpawnPointComponent を取得してキャッシュする
     */
    void CacheSpawnPoints();
    const std::vector<SpawnPointComponent*>& GetSpawnPoints(const std::string& waveId) const;

    /**
     * @brief 登録済みまたはシーン内の EnemySpawnerComponent を取得する
     */
    EnemySpawnerComponent* GetEnemySpawner() const;

    void SetAllEvents(const std::vector<WaveEventData>& events) {
        allEvents_ = events;
    }
    const std::vector<WaveEventData>& GetAllEvents() const {
        return allEvents_;
    }
    void SortEventsByDistance();
    const std::string& GetLevelDataPath() const {
        return levelDataPath_;
    }
    void SetLevelDataPath(const std::string& path) {
        levelDataPath_ = path;
    }

    void SaveLevelData();
    void SaveLevelData(const std::string& filePath);

    float GetEditorPreviewDistance() const {
        return editorPreviewDistance_;
    }
    void SetEditorPreviewDistance(float dist) {
        editorPreviewDistance_ = dist;
    }

    int GetSelectedEventIndex() const {
        return selectedEventIndex_;
    }
    void SetSelectedEventIndex(int index) {
        selectedEventIndex_ = index;
    }

private:
    void LoadLevelData(const std::string& filePath);

    std::priority_queue<WaveEventData, std::vector<WaveEventData>, std::greater<WaveEventData>> eventQueue_;
    std::vector<WaveEventData> allEvents_; // パース済みの全イベントリスト（エディタのプレビューおよびUI用）
    std::unordered_map<std::string, std::shared_ptr<IWaveEventHandler>> handlers_;

    // キャッシュ
    std::unordered_map<std::string, std::vector<SpawnPointComponent*>> spawnPointsMap_;
    bool hasCachedSpawnPoints_ = false;

    std::weak_ptr<GameObject> cachedPlayerCart_;
    std::string levelDataPath_ = "resources/GameData/WaveData_Stage1.json";

    float editorPreviewDistance_ = 0.0f;
    int selectedEventIndex_ = -1;
    std::string currentPreviewModelPath_ = "";
    std::shared_ptr<ModelBatchRendererComponent> previewBatch_;
    uint64_t targetSplineID_ = 0;
    uint64_t targetEnemySpawnerID_ = 0;
};
