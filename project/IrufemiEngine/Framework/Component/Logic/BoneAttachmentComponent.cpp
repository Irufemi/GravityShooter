#include "Framework/Component/Logic/BoneAttachmentComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Resource/Model/Data/SkeletonPose.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Core/Math/MathFunction.h"

namespace Irufemi {

BoneAttachmentComponent::BoneAttachmentComponent() = default;
BoneAttachmentComponent::~BoneAttachmentComponent() = default;

void BoneAttachmentComponent::Initialize() {}

void BoneAttachmentComponent::Update() {
    if (targetName_.empty() || targetBoneName_.empty()) {
        return;
    }

    auto gameObject = GetGameObject();
    if (!gameObject) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    // キャッシュの検証と再構築
    auto targetObj = cachedTargetObj_.lock();
    if (!targetObj || cachedBoneIndex_ < 0 || !cachedRenderer_ || !cachedTargetTransform_) {
        auto scene = gameObject->GetScene();
        if (!scene) {
            return;
        }

        targetObj = scene->FindGameObject(targetName_);
        if (!targetObj) {
            return;
        }

        cachedTargetObj_ = targetObj;
        cachedTargetTransform_ = targetObj->GetComponent<TransformComponent>();
        cachedRenderer_ = targetObj->GetComponent<SkinnedMeshRendererComponent>();

        if (cachedRenderer_ && cachedRenderer_->GetRawObject()) {
            const SkeletonPose* pose = cachedRenderer_->GetRawObject()->GetInternalSkeletonPose();
            if (pose && pose->data) {
                auto it = pose->data->jointMap.find(targetBoneName_);
                if (it != pose->data->jointMap.end()) {
                    cachedBoneIndex_ = it->second;
                }
            }
        }
    }

    // O(1) で即座にボーン姿勢を取得して同期
    if (cachedRenderer_ && cachedRenderer_->GetRawObject() && cachedBoneIndex_ >= 0) {
        const SkeletonPose* pose = cachedRenderer_->GetRawObject()->GetInternalSkeletonPose();
        if (pose && cachedBoneIndex_ < static_cast<int>(pose->jointPoses.size())) {
            // ローカルのボーン行列を取得
            Irufemi::Matrix4x4 localMat = pose->jointPoses[cachedBoneIndex_].skeletonSpaceMatrix;

            // 親のワールド行列と掛けてボーンの最終ワールド行列を算出
            Irufemi::Matrix4x4 boneWorldMat = localMat;
            if (cachedTargetTransform_) {
                boneWorldMat = localMat * cachedTargetTransform_->GetWorldMatrix();
            }

            // 自身のTransformComponentに適用するため、ワールド行列をそのまま渡す
            transform->SetWorldMatrix(boneWorldMat);

            // 更新を即座に反映させる（次のコンポーネントが描画などに使うため）
            transform->UpdateMatrixImmediate();
        }
    }
}

void BoneAttachmentComponent::OnRegisterProperties() {
    RegisterProperty("Target Name", &targetName_);
    RegisterProperty("Target Bone Name", &targetBoneName_);
}

nlohmann::json BoneAttachmentComponent::Serialize() {
    nlohmann::json j = nlohmann::json::object();
    j["Target Name"] = targetName_;
    j["Target Bone Name"] = targetBoneName_;
    return j;
}

void BoneAttachmentComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("Target Name")) {
        targetName_ = j["Target Name"];
    }
    if (j.contains("Target Bone Name")) {
        targetBoneName_ = j["Target Bone Name"];
    }
    InvalidateCache();
}

} // namespace Irufemi
