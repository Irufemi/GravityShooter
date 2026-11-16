#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/Object/2D/SpriteBatch/SpriteBatch.h"
#include "Core/Math/Vector2.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace Irufemi { class GameObject; }

struct LockonMarkerState {
    std::weak_ptr<Irufemi::GameObject> target;
    float currentScale = 2.0f; ///< 初期は大きく（シュッと縮小させるため）
    float targetScale = 1.0f;  ///< 最終的なスケール（距離に依存）
    float animationT = 0.0f;   ///< イージング用タイマー (0.0 ～ 1.0)
};

/**
 * @class LockonMarkerUIComponent
 * @brief ロックオンマーカーのUI描画およびアニメーションを管理するコンポーネント
 */
class LockonMarkerUIComponent : public Irufemi::Component, public std::enable_shared_from_this<LockonMarkerUIComponent> {
public:
    LockonMarkerUIComponent() = default;
    ~LockonMarkerUIComponent() override = default;

    void Initialize() override;
    void Update() override;

    // ターゲットのリストを同期する（GravityPlayerComponent等から呼ばれる）
    void SyncTargets(const std::vector<std::shared_ptr<Irufemi::GameObject>>& targets);
    void SetMaxLockonCount(size_t count) {
        maxLockonCount_ = count;
    }

    std::string GetComponentName() const override {
        return "LockonMarkerUIComponent";
    }

private:
    size_t maxLockonCount_ = 1;
    std::vector<LockonMarkerState> activeMarkers_;
    std::unique_ptr<Irufemi::SpriteBatch> markerBatch_;

    // ゼロアロケーション用のキャッシュコンテナ
    std::vector<LockonMarkerState> nextMarkersCache_;
    std::unordered_map<uint64_t, int> targetCountsCache_;
    std::unordered_map<uint64_t, int> drawCountsCache_;
};
