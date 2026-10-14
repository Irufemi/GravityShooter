#include "RailMechanics/RailRelativeFollowerComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "Framework/Component/Utility/SplineComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/Math/MathFunction.h"
#include <algorithm>
#include <cmath>

void RailRelativeFollowerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterGameObjectRef("Target Object", &targetObjectID_);
    RegisterProperty("Distance Offset", &distanceOffset_);
    RegisterProperty("Local Offset X", &localOffset_.x);
    RegisterProperty("Local Offset Y", &localOffset_.y);
    RegisterProperty("Local Offset Z", &localOffset_.z);
}

void RailRelativeFollowerComponent::Initialize() {
    retryCounter_ = 0;
}

void RailRelativeFollowerComponent::OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {
    if (targetObjectID_ != 0) {
        auto it = idMap.find(targetObjectID_);
        if (it != idMap.end()) {
            targetObjectID_ = it->second;
        }
    }
}

void RailRelativeFollowerComponent::Start() {
    if (!gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (scene) {
        // IDでターゲットオブジェクトを検索 (O(1)で高速かつ確実)
        auto target = scene->FindGameObjectByID(targetObjectID_);
        if (target) {
            targetObject_ = target;
            targetFollower_ = target->GetComponent<SplineFollowerComponent>();
            if (targetFollower_) {
                cachedPath_ = targetFollower_->GetCachedPath();
            }
        }
    }
}

void RailRelativeFollowerComponent::Update() {
    if (!gameObject_) {
        return;
    }

    // ターゲットまたはパスが未解決の場合は再取得を試みる
    if (!targetFollower_ || !cachedPath_) {
        // targetObject_ が既に取得できている場合は FindGameObject (Start) の呼び出しを避ける
        if (auto target = targetObject_.lock()) {
            if (!targetFollower_) {
                targetFollower_ = target->GetComponent<SplineFollowerComponent>();
            }
            if (targetFollower_ && !cachedPath_) {
                cachedPath_ = targetFollower_->GetCachedPath();
            }
            if (!targetFollower_ || !cachedPath_) {
                return;
            }
        } else {
            // FindGameObject はシーンのミューテックスをロックするため、
            // 毎フレーム複数スレッドから呼ばれると深刻なスレッド競合（ガタつき）の原因になる。
            // そのためリトライ頻度を落とす。
            if (++retryCounter_ < kSearchRetryInterval) {
                return;
            }
            retryCounter_ = 0;

            Start();
            if (!targetFollower_ || !cachedPath_) {
                return; // それでも見つからなければ終了
            }
        }
    }

    // ターゲットの現在のレール上での距離を取得
    float baseDistance = targetFollower_->GetCurrentDistance();

    // 自身が位置すべきレール上の距離を計算
    float targetDistance = baseDistance + distanceOffset_;
    float totalLength = cachedPath_->GetTotalLength();

    // 距離のクランプ（レールの終端を超えないようにする）
    if (targetDistance > totalLength) {
        targetDistance = totalLength;
    }
    if (targetDistance < 0.0f) {
        targetDistance = 0.0f;
    }

    // レール上の座標と接線（進行方向）を取得
    Irufemi::Vector3 basePos = cachedPath_->GetPointAtDistance(targetDistance);
    Irufemi::Vector3 tangent = cachedPath_->GetTangentAtDistance(targetDistance);

    // 基本となる回転（Z前方）を計算
    float yaw = std::atan2(tangent.x, tangent.z);
    float pitch = std::asin(std::clamp(-tangent.y, -1.0f, 1.0f));
    Irufemi::Vector3 rotation = {pitch, yaw, 0.0f};

    // XYローカルオフセットの適用（レール中心から上下左右へのズレ）
    Irufemi::Matrix4x4 rotMat = Irufemi::Math::MakeRotateXYZMatrix(rotation);
    Irufemi::Vector3 right = {rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2]};
    Irufemi::Vector3 up = {rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2]};
    Irufemi::Vector3 forward = {rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2]};

    Irufemi::Vector3 finalPos = basePos;
    finalPos = Irufemi::Math::Add(finalPos, Irufemi::Math::Multiply(localOffset_.x, right));
    finalPos = Irufemi::Math::Add(finalPos, Irufemi::Math::Multiply(localOffset_.y, up));
    finalPos = Irufemi::Math::Add(finalPos, Irufemi::Math::Multiply(localOffset_.z, forward));

    auto transform = GetTransform();
    if (transform) {
        transform->SetWorldPosition(finalPos);
        transform->SetWorldRotation(rotation);
    }
}
