#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>

class SplineComponent;
class Line3DBatch;

/**
 * @class SplineFollowerComponent
 * @brief 指定されたスプラインレール上を移動し、自身のTransformを更新するコンポーネント
 */
class SplineFollowerComponent : public Component {
public:
    SplineFollowerComponent() = default;
    ~SplineFollowerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void Draw() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "SplineFollowerComponent";
    }

    float GetCurrentDistance() const {
        return currentDistance_;
    }
    SplineComponent* GetCachedPath() const {
        return cachedPath_;
    }
    uint64_t GetTargetPathID() const {
        return targetPathID_;
    }

    /**
     * @brief 指定した進行距離に即時スナップし、Transformの座標と回転を更新します。
     * @param dist スプライン上の距離 (m)
     */
    void SnapToDistance(float dist);

    void OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) override;

private:
    float currentDistance_ = 0.0f;          ///< ルート（軌道）上の進み具合 (m)
    float speed_ = 10.0f;                   ///< 自動前進するスピード (m/s)
    uint64_t targetPathID_ = 0;             ///< 追従対象のオブジェクトID
    SplineComponent* cachedPath_ = nullptr; ///< キャッシュされた対象のパス
};
