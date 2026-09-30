#include "Framework/Component/Utility/SplineComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "Renderer/Object/Line/LineClass.h"
#include <algorithm>
#include <cmath>

void SplineComponent::OnRegisterProperties() {
    // 拡張した Float3Array を使ってウェイポイントをプロパティに登録
    RegisterProperty("Waypoints", &waypoints_);
    RegisterProperty("Draw Debug Rail", &drawDebugRail_);
    RegisterProperty("Draw Debug Nodes", &drawDebugNodes_);
    RegisterPropertyRange("Node Radius", &nodeRadius_, 0.1f, 5.0f);
}

void SplineComponent::Initialize() {
    OnAwake();
}

void SplineComponent::OnAwake() {
    debugLineBatch_ = std::make_unique<Line3DBatch>();
    debugLineBatch_->Initialize();
    UpdateDistanceTable();
}

void SplineComponent::Update() {
    // 既存の子オブジェクトが存在する場合のみ後方互換で同期（基本は空なので即リターン）
    if (gameObject_ && !gameObject_->GetChildren().empty()) {
        UpdateWaypointsFromChildren();
    }
}

void SplineComponent::Draw() {
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    if (drawDebugRail_ && debugLineBatch_ && waypoints_.size() >= 2) {
        debugLineBatch_->ClearInstances();
        const int segments = 100;
        Irufemi::Vector4 color = {0.0f, 1.0f, 0.0f, 1.0f}; // 緑色
        Irufemi::Vector3 prevPos = GetPointAt(0.0f);

        for (int i = 1; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            Irufemi::Vector3 currentPos = GetPointAt(t);
            debugLineBatch_->AddInstance(prevPos, currentPos, color);
            prevPos = currentPos;
        }
        debugLineBatch_->BuildInstanceBuffer();
        debugLineBatch_->Update();

        debugLineBatch_->SyncBeforeDraw();
        debugLineBatch_->Draw();
    }

    // ウェイポイント球体ギズモの一括描画
    if (drawDebugNodes_ && gameObject_ && !waypoints_.empty()) {
        if (auto scene = gameObject_->GetScene()) {
            if (auto engine = scene->GetEngine()) {
                if (auto debugRenderer = engine->GetDebugPrimitiveRenderer()) {
                    const Irufemi::Vector4 normalColor = {1.0f, 1.0f, 0.0f, 1.0f}; // 黄色 (中間点)
                    const Irufemi::Vector4 startColor = {0.0f, 0.8f, 1.0f, 1.0f};  // シアン (始点)
                    const Irufemi::Vector4 endColor = {1.0f, 0.2f, 0.2f, 1.0f};    // 赤 (終点)
                    for (size_t i = 0; i < waypoints_.size(); ++i) {
                        Irufemi::Vector4 c =
                            (i == 0) ? startColor : ((i == waypoints_.size() - 1) ? endColor : normalColor);
                        debugRenderer->AddSphere(waypoints_[i], nodeRadius_, c, DebugCategory::Path);
                    }
                }
            }
        }
    }
#endif
}

Irufemi::Vector3 SplineComponent::GetPointAt(float t) const {
    if (waypoints_.empty()) {
        return {0.0f, 0.0f, 0.0f};
    }
    if (waypoints_.size() == 1) {
        return waypoints_[0];
    }

    t = std::clamp(t, 0.0f, 1.0f);

    // セグメント数
    int segments = static_cast<int>(waypoints_.size()) - 1;
    // 現在のtが属するセグメント
    float scaledT = t * segments;
    int index = static_cast<int>(scaledT);
    if (index >= segments) {
        index = segments - 1;
        scaledT = static_cast<float>(segments);
    }

    // セグメント内のローカルt (0.0 ~ 1.0)
    float localT = scaledT - index;

    // Catmull-Rom スプライン補間のための制御点4つを取得
    Irufemi::Vector3 p0 = waypoints_[(std::max)(0, index - 1)];
    Irufemi::Vector3 p1 = waypoints_[index];
    Irufemi::Vector3 p2 = waypoints_[(std::min)(segments, index + 1)];
    Irufemi::Vector3 p3 = waypoints_[(std::min)(segments, index + 2)];

    float t2 = localT * localT;
    float t3 = t2 * localT;

    Irufemi::Vector3 result;
    result.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * localT + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                       (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);

    result.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * localT + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                       (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);

    result.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * localT + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                       (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);

    return result;
}

Irufemi::Vector3 SplineComponent::GetTangentAt(float t) const {
    if (waypoints_.size() < 2) {
        return {0.0f, 0.0f, 1.0f}; // デフォルトの進行方向
    }

    t = std::clamp(t, 0.0f, 1.0f);

    int segments = static_cast<int>(waypoints_.size()) - 1;
    float scaledT = t * segments;
    int index = static_cast<int>(scaledT);
    if (index >= segments) {
        index = segments - 1;
        scaledT = static_cast<float>(segments);
    }

    float localT = scaledT - index;

    // Catmull-Rom スプラインの4制御点
    Irufemi::Vector3 p0 = waypoints_[(std::max)(0, index - 1)];
    Irufemi::Vector3 p1 = waypoints_[index];
    Irufemi::Vector3 p2 = waypoints_[(std::min)(segments, index + 1)];
    Irufemi::Vector3 p3 = waypoints_[(std::min)(segments, index + 2)];

    // Catmull-Rom スプラインの代数導関数（一次微分による高精度な接線算出）
    // p'(u) = 0.5 * ((-p0 + p2) + 2(2p0 - 5p1 + 4p2 - p3)u + 3(-p0 + 3p1 - 3p2 + p3)u^2)
    float u = localT;
    float u2 = u * u;

    Irufemi::Vector3 tangent;
    tangent.x = 0.5f * ((-p0.x + p2.x) + 2.0f * (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * u +
                        3.0f * (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * u2);
    tangent.y = 0.5f * ((-p0.y + p2.y) + 2.0f * (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * u +
                        3.0f * (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * u2);
    tangent.z = 0.5f * ((-p0.z + p2.z) + 2.0f * (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * u +
                        3.0f * (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * u2);

    float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
    if (length > 0.0001f) {
        tangent.x /= length;
        tangent.y /= length;
        tangent.z /= length;
    } else {
        tangent = {0.0f, 0.0f, 1.0f};
    }

    return tangent;
}

void SplineComponent::UpdateWaypointsFromChildren() {
    if (!gameObject_) {
        return;
    }
    auto children = gameObject_->GetChildren();

    if (!children.empty()) {
        bool changed = false;
        if (children.size() != lastChildCount_) {
            changed = true;
            lastChildCount_ = static_cast<int>(children.size());
        } else {
            // Check if any position changed (simple check)
            for (size_t i = 0; i < children.size() && i < waypoints_.size(); ++i) {
                if (auto transform = children[i]->GetComponent<TransformComponent>()) {
                    Irufemi::Vector3 pos = transform->GetPosition();
                    if (std::abs(pos.x - waypoints_[i].x) > 0.001f || std::abs(pos.y - waypoints_[i].y) > 0.001f ||
                        std::abs(pos.z - waypoints_[i].z) > 0.001f) {
                        changed = true;
                        break;
                    }
                }
            }
        }

        if (changed) {
            waypoints_.clear();
            for (const auto& child : children) {
                if (auto transform = child->GetComponent<TransformComponent>()) {
                    waypoints_.push_back(transform->GetPosition());
                }
            }
            UpdateDistanceTable();
        }
    }
}

void SplineComponent::UpdateDistanceTable() {
    distanceLUT_.clear();
    distanceLUT_.push_back(0.0f);
    totalLength_ = 0.0f;

    if (waypoints_.size() < 2) {
        return;
    }

    int numSamples = (std::max)(10, static_cast<int>(waypoints_.size()) * 20);
    Irufemi::Vector3 prevPos = GetPointAt(0.0f);
    for (int i = 1; i <= numSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numSamples);
        Irufemi::Vector3 currPos = GetPointAt(t);
        Irufemi::Vector3 diff = {currPos.x - prevPos.x, currPos.y - prevPos.y, currPos.z - prevPos.z};
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        totalLength_ += dist;
        distanceLUT_.push_back(totalLength_);
        prevPos = currPos;
    }
}

float SplineComponent::GetTotalLength() const {
    if (distanceLUT_.empty() && waypoints_.size() >= 2) {
        const_cast<SplineComponent*>(this)->UpdateDistanceTable();
    }
    return totalLength_;
}

Irufemi::Vector3 SplineComponent::GetPointAtDistance(float distance) const {
    if (waypoints_.empty()) {
        return {0.0f, 0.0f, 0.0f};
    }
    if (waypoints_.size() == 1 || distanceLUT_.empty() || totalLength_ <= 0.0f) {
        return waypoints_[0];
    }

    float t = 0.0f;
    if (distance <= 0.0f) {
        t = 0.0f;
    } else if (distance >= totalLength_) {
        t = 1.0f;
    } else {
        int numSamples = static_cast<int>(distanceLUT_.size()) - 1;
        for (int i = 0; i < numSamples; ++i) {
            if (distanceLUT_[i] <= distance && distance <= distanceLUT_[i + 1]) {
                float diff = distanceLUT_[i + 1] - distanceLUT_[i];
                float ratio = (diff > 0.0001f) ? ((distance - distanceLUT_[i]) / diff) : 0.0f;
                t = (static_cast<float>(i) + ratio) / static_cast<float>(numSamples);
                break;
            }
        }
    }
    return GetPointAt(t);
}

Irufemi::Vector3 SplineComponent::GetTangentAtDistance(float distance) const {
    if (waypoints_.size() < 2) {
        return {0.0f, 0.0f, 1.0f};
    }

    if (distanceLUT_.empty() || totalLength_ <= 0.0f) {
        return GetTangentAt(0.0f);
    }

    float t = 0.0f;
    if (distance <= 0.0f) {
        t = 0.0f;
    } else if (distance >= totalLength_) {
        t = 1.0f;
    } else {
        int numSamples = static_cast<int>(distanceLUT_.size()) - 1;
        for (int i = 0; i < numSamples; ++i) {
            if (distanceLUT_[i] <= distance && distance <= distanceLUT_[i + 1]) {
                float diff = distanceLUT_[i + 1] - distanceLUT_[i];
                float ratio = (diff > 0.0001f) ? ((distance - distanceLUT_[i]) / diff) : 0.0f;
                t = (static_cast<float>(i) + ratio) / static_cast<float>(numSamples);
                break;
            }
        }
    }
    return GetTangentAt(t);
}
