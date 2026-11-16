#pragma once
#include "Framework/Component/Component.h"
#include "Core/Utility/ObjectPool.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace Irufemi {
class GameObject;
}
class BossBulletManagerComponent;
namespace Irufemi {
class EffectMaskComponent;
}
namespace Irufemi {
class ModelBatchRendererComponent;
}
#include "Renderer/Object/Batch/ModelBatch.h"

class DroneManagerComponent : public Irufemi::Component {
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
     * @brief ドローンプールを事前構築する（ロード時ウォームアップ）
     */
    void WarmupPool();

    /**
     * @brief ボスの周囲にドローンを一斉展開する
     */
    void DeployDrones(std::weak_ptr<Irufemi::GameObject> boss, int count, BossBulletManagerComponent* bulletMgr);

    /**
     * @brief ドローンを全回収する
     */
    void RecallAllDrones();

private:
    int maxDrones_ = 50;
    std::unique_ptr<Irufemi::ObjectPool<Irufemi::GameObject>> dronePool_;

    struct DroneAnimData {
        float orbitAngle = 0.0f;
        float fireTimer = 0.0f;
    };

    struct ActiveDrone {
        Irufemi::ObjectPool<Irufemi::GameObject>::Handle handle;
        std::shared_ptr<Irufemi::GameObject> gameObject;
    };

    std::vector<ActiveDrone> activeDrones_;
    std::vector<DroneAnimData> animDataList_;

    Irufemi::ModelBatchRendererComponent* batchRenderer_ = nullptr;
    Irufemi::EffectMaskComponent* effectMask_ = nullptr;
    int activeDroneCount_ = 0;

    // 定数パラメーター（インスペクターで調整可能にすることも可）
    float orbitRadius_ = 15.0f;
    float orbitSpeed_ = 1.0f;
    float fireInterval_ = 3.0f;
    float bulletSpeed_ = 45.0f;
    std::string dronePrefabPath_ = "resources/prefabs/BossDrone.json";

    std::weak_ptr<Irufemi::GameObject> boss_;
    std::weak_ptr<Irufemi::GameObject> player_;
    BossBulletManagerComponent* bulletManager_ = nullptr;
};
