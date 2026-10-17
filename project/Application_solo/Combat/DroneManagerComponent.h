#pragma once
#include "Framework/Component/Component.h"
#include "Core/Utility/ObjectPool.h"
#include <memory>
#include <unordered_map>
#include <vector>

class GameObject;
class BossBulletManagerComponent;
#include "Renderer/Object/Batch/ModelBatch.h"

class DroneManagerComponent : public Component {
public:
    DroneManagerComponent();
    ~DroneManagerComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    std::string GetComponentName() const override {
        return "DroneManagerComponent";
    }
    void OnRegisterProperties() override;

    /**
     * @brief ボスの周囲にドローンを一斉展開する
     */
    void DeployDrones(std::weak_ptr<GameObject> boss, int count, BossBulletManagerComponent* bulletMgr);

    /**
     * @brief ドローンを全回収する
     */
    void RecallAllDrones();

private:
    int maxDrones_ = 50;
    std::unique_ptr<ObjectPool<GameObject>> dronePool_;

    struct DroneAnimData {
        float orbitAngle = 0.0f;
        float fireTimer = 0.0f;
    };

    struct ActiveDrone {
        ObjectPool<GameObject>::Handle handle;
        std::shared_ptr<GameObject> gameObject;
    };

    std::vector<ActiveDrone> activeDrones_;
    std::vector<DroneAnimData> animDataList_;

    class ModelBatchRendererComponent* batchRenderer_ = nullptr;
    int activeDroneCount_ = 0;

    // 定数パラメーター（インスペクターで調整可能にすることも可）
    float orbitRadius_ = 15.0f;
    float orbitSpeed_ = 1.0f;
    float fireInterval_ = 3.0f;
    std::string dronePrefabPath_ = "resources/prefabs/BossDrone.json";

    std::weak_ptr<GameObject> boss_;
    std::weak_ptr<GameObject> player_;
    BossBulletManagerComponent* bulletManager_ = nullptr;
};
