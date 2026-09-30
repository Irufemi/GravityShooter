#include "Framework/Component/Camera/TargetFollowComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include <cmath>

void TargetFollowComponent::OnRegisterProperties() {
    RegisterGameObjectRef("Target Object", &targetObjectID_);
    RegisterProperty("Offset", &offset_);
    RegisterProperty("FollowDelay", &followDelay_);
}

void TargetFollowComponent::Initialize() {
    targetObj_.reset();
    isFirstFrame_ = true;
}

void TargetFollowComponent::Start() {
    targetObj_.reset();
    isFirstFrame_ = true;
    if (gameObject_ && targetObjectID_ != 0) {
        if (auto scene = gameObject_->GetScene()) {
            if (auto target = scene->FindGameObjectByID(targetObjectID_)) {
                targetObj_ = target;
            }
        }
    }
    SnapToTarget();
}

void TargetFollowComponent::OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {
    if (targetObjectID_ != 0) {
        auto it = idMap.find(targetObjectID_);
        if (it != idMap.end()) {
            targetObjectID_ = it->second;
        }
    }
}

void TargetFollowComponent::SnapToTarget() {
    if (!gameObject_) {
        return;
    }

    // ターゲットが未キャッシュの場合はシーン内から指定されたIDで探索
    auto target = targetObj_.lock();
    if (!target && targetObjectID_ != 0) {
        auto scene = gameObject_->GetScene();
        if (scene) {
            target = scene->FindGameObjectByID(targetObjectID_);
            if (target) {
                targetObj_ = target;
            }
        }
    }

    if (!target) {
        return;
    }

    auto targetTransform = target->GetTransform();
    if (!targetTransform) {
        return;
    }

    auto myTransform = GetTransform();
    if (!myTransform) {
        return;
    }

    // プレイヤーの向き（ワールド姿勢行列）から基底ベクトルを取得
    Irufemi::Vector3 forward = targetTransform->GetWorldForward();
    Irufemi::Vector3 right = targetTransform->GetWorldRight();
    Irufemi::Vector3 up = targetTransform->GetWorldUp();

    // プレイヤー位置に、プレイヤーの向きに基づいたローカルオフセットを足す
    Irufemi::Vector3 targetCamPos = {
        targetTransform->GetWorldPosition().x + right.x * offset_.x + up.x * offset_.y + forward.x * offset_.z,
        targetTransform->GetWorldPosition().y + right.y * offset_.x + up.y * offset_.y + forward.y * offset_.z,
        targetTransform->GetWorldPosition().z + right.z * offset_.x + up.z * offset_.y + forward.z * offset_.z};

    Irufemi::Vector3 targetRotWorld = targetTransform->GetWorldRotation();

    // 計算したワールド座標・回転からワールド行列を作成し、一度に設定する (Lerpなしで即時スナップ)
    Irufemi::Matrix4x4 targetWorldMat = Irufemi::Math::MakeAffineMatrix({1, 1, 1}, targetRotWorld, targetCamPos);
    myTransform->SetWorldMatrix(targetWorldMat);
    myTransform->UpdateMatrixImmediate();

    isFirstFrame_ = false;

    // 同一GameObject内のCameraComponentへ最新座標・姿勢を即時同期
    if (auto camComp = gameObject_->GetComponent<CameraComponent>()) {
        camComp->Update();
    }
}

void TargetFollowComponent::Update() {
    if (!gameObject_) {
        return;
    }

    // 初回フレーム時は即時スナップ（Camera Cut）を行い、不要なLerp補間遅延を回避
    if (isFirstFrame_) {
        SnapToTarget();
        return;
    }

    // 正確なデルタタイムの取得
    auto engine = GetEngine();
    float deltaTime = engine ? engine->GetGameDeltaTime() : (1.0f / 60.0f);
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    // ターゲットが未キャッシュの場合はシーン内から指定されたIDで探索
    auto target = targetObj_.lock();
    if (!target && targetObjectID_ != 0) {
        auto scene = gameObject_->GetScene();
        if (scene) {
            target = scene->FindGameObjectByID(targetObjectID_);
            if (target) {
                targetObj_ = target;
            }
        }
    }

    if (!target) {
        return;
    }

    auto targetTransform = target->GetTransform();
    if (!targetTransform) {
        return;
    }

    auto myTransform = GetTransform();
    if (!myTransform) {
        return;
    }

    // プレイヤーの向き（ワールド姿勢行列）から基底ベクトルを取得
    Irufemi::Vector3 forward = targetTransform->GetWorldForward();
    Irufemi::Vector3 right = targetTransform->GetWorldRight();
    Irufemi::Vector3 up = targetTransform->GetWorldUp();

    // プレイヤー位置に、プレイヤーの向きに基づいたローカルオフセットを足す
    Irufemi::Vector3 targetCamPos = {
        targetTransform->GetWorldPosition().x + right.x * offset_.x + up.x * offset_.y + forward.x * offset_.z,
        targetTransform->GetWorldPosition().y + right.y * offset_.x + up.y * offset_.y + forward.y * offset_.z,
        targetTransform->GetWorldPosition().z + right.z * offset_.x + up.z * offset_.y + forward.z * offset_.z};

    // 滑らかな追従 (線形補間/Lerp) を行う (フレームレート非依存)
    float t = 1.0f - std::pow(followDelay_, deltaTime);
    Irufemi::Vector3 currentPosWorld = myTransform->GetWorldPosition();
    Irufemi::Vector3 newPosWorld = currentPosWorld;
    newPosWorld.x += (targetCamPos.x - newPosWorld.x) * t;
    newPosWorld.y += (targetCamPos.y - newPosWorld.y) * t;
    newPosWorld.z += (targetCamPos.z - newPosWorld.z) * t;
    // カメラの向き（角度）もプレイヤーの向きに追従させる（Lerpで滑らかに旋回）
    Irufemi::Vector3 currentRotWorld = myTransform->GetWorldRotation();
    Irufemi::Vector3 newRotWorld = currentRotWorld;
    newRotWorld.x += (targetTransform->GetWorldRotation().x - newRotWorld.x) * t;
    newRotWorld.y += (targetTransform->GetWorldRotation().y - newRotWorld.y) * t;
    newRotWorld.z += (targetTransform->GetWorldRotation().z - newRotWorld.z) * t;

    // 計算したワールド座標・回転からワールド行列を作成し、一度に設定する
    Irufemi::Matrix4x4 targetWorldMat = Irufemi::Math::MakeAffineMatrix({1, 1, 1}, newRotWorld, newPosWorld);
    myTransform->SetWorldMatrix(targetWorldMat);

    // 次のコンポーネント（CameraComponent等）のために強制更新
    myTransform->UpdateMatrixImmediate();

    // 同一GameObject内のCameraComponentへ最新座標・姿勢を即時同期（1フレーム遅れジッターを解消）
    if (auto camComp = gameObject_->GetComponent<CameraComponent>()) {
        camComp->Update();
    }
}
