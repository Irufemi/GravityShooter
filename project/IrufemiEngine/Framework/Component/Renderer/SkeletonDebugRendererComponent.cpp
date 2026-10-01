#include "Framework/Component/Renderer/SkeletonDebugRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Object/Batch/PrimitiveBatch.h"
#include "Renderer/Object/Line/LineClass.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Resource/Model/Data/SkeletonPose.h"
#include <cmath>

namespace {
/**
 * @brief 色相(Hue 0~360度)からRGBAカラーを生成するヘルパー関数
 */
Irufemi::Vector4 HueToRGB(float hue, float alpha = 0.6f) {
    float normalizedHue = std::fmod(hue, 360.0f);
    if (normalizedHue < 0.0f) {
        normalizedHue += 360.0f;
    }
    float c = 1.0f;
    float x = c * (1.0f - std::abs(std::fmod(normalizedHue / 60.0f, 2.0f) - 1.0f));
    Irufemi::Vector4 color = {0.0f, 0.0f, 0.0f, alpha};

    if (normalizedHue < 60.0f) {
        color.x = c;
        color.y = x;
        color.z = 0.0f;
    } else if (normalizedHue < 120.0f) {
        color.x = x;
        color.y = c;
        color.z = 0.0f;
    } else if (normalizedHue < 180.0f) {
        color.x = 0.0f;
        color.y = c;
        color.z = x;
    } else if (normalizedHue < 240.0f) {
        color.x = 0.0f;
        color.y = x;
        color.z = c;
    } else if (normalizedHue < 300.0f) {
        color.x = x;
        color.y = 0.0f;
        color.z = c;
    } else {
        color.x = c;
        color.y = 0.0f;
        color.z = x;
    }
    return color;
}
} // namespace

SkeletonDebugRendererComponent::SkeletonDebugRendererComponent() {
    boneMeshes_ = std::make_unique<PrimitiveBatch>();
    debugAxesLines_ = std::make_unique<Line3DBatch>();
}

SkeletonDebugRendererComponent::~SkeletonDebugRendererComponent() {}

void SkeletonDebugRendererComponent::Initialize() {
    OnAwake();
}

void SkeletonDebugRendererComponent::OnAwake() {
    boneMeshes_->Initialize(Irufemi::PrimitiveType::Octahedron, "resources/whiteTexture.png");
    boneMeshes_->SetDepthWrite(PSOManager::DepthWrite::Off);

    debugAxesLines_->Initialize();
    debugAxesLines_->SetDepthWrite(PSOManager::DepthWrite::Off);
}

void SkeletonDebugRendererComponent::Update() {
    boneMeshes_->ClearInstances();
    debugAxesLines_->ClearInstances();

    if (!showBones_ && !showAxes_) {
        return;
    }

    auto skinnedMesh = GetGameObject()->GetComponent<SkinnedMeshRendererComponent>();
    if (!skinnedMesh || !skinnedMesh->GetRawObject()) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    auto rawObj = skinnedMesh->GetRawObject();
    const SkeletonData* skeletonData = rawObj->GetSkeletonData();
    const SkeletonPose* currentPose = rawObj->GetInternalSkeletonPose(); // スキンメッシュの最新ポーズ

    if (!skeletonData || !currentPose || currentPose->jointPoses.empty()) {
        return;
    }

    Irufemi::Matrix4x4 worldMatrix = Irufemi::Math::MakeAffineMatrix(
        transform->GetWorldScale(), transform->GetWorldRotation(), transform->GetWorldPosition());

    // ジョイント階層深度の一括事前計算 (O(N))
    std::vector<int> jointDepths(currentPose->jointPoses.size(), 0);
    for (size_t i = 0; i < currentPose->jointPoses.size(); ++i) {
        if (currentPose->data->joints[i].parent) {
            int32_t pIdx = *currentPose->data->joints[i].parent;
            if (pIdx >= 0 && static_cast<size_t>(pIdx) < jointDepths.size()) {
                jointDepths[i] = jointDepths[pIdx] + 1;
            }
        }
    }

    for (size_t i = 0; i < currentPose->jointPoses.size(); ++i) {
        const Irufemi::Matrix4x4& jointMat = currentPose->jointPoses[i].skeletonSpaceMatrix;
        Irufemi::Matrix4x4 jointWorldMat = jointMat * worldMatrix;
        Irufemi::Vector3 jointPosition = {jointWorldMat.m[3][0], jointWorldMat.m[3][1], jointWorldMat.m[3][2]};

        float currentBoneLength = 0.05f; // Fallback for root or bones without parents
        Irufemi::Vector3 parentPosition = jointPosition;
        int depth = 0;

        if (currentPose->data->joints[i].parent) {
            const int32_t parentIndex = *currentPose->data->joints[i].parent;
            const Irufemi::Matrix4x4& parentMat = currentPose->jointPoses[parentIndex].skeletonSpaceMatrix;
            Irufemi::Matrix4x4 parentWorldMat = parentMat * worldMatrix;
            parentPosition = {parentWorldMat.m[3][0], parentWorldMat.m[3][1], parentWorldMat.m[3][2]};

            Irufemi::Vector3 dir = {jointPosition.x - parentPosition.x, jointPosition.y - parentPosition.y,
                                    jointPosition.z - parentPosition.z};
            currentBoneLength = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

            depth = (parentIndex >= 0 && static_cast<size_t>(parentIndex) < jointDepths.size())
                        ? jointDepths[parentIndex]
                        : 0;
        }

        // ボーン接続の描画（八面体）
        if (showBones_ && currentPose->data->joints[i].parent && currentBoneLength > 0.0f) {
            Irufemi::Vector3 dir = {jointPosition.x - parentPosition.x, jointPosition.y - parentPosition.y,
                                    jointPosition.z - parentPosition.z};
            Irufemi::Vector3 yAxis = {dir.x / currentBoneLength, dir.y / currentBoneLength, dir.z / currentBoneLength};
            Irufemi::Vector3 xAxis;
            if (std::abs(yAxis.y) > 0.999f) {
                xAxis = {1.0f, 0.0f, 0.0f};
            } else {
                Irufemi::Vector3 up = {0.0f, 1.0f, 0.0f};
                xAxis = {up.y * yAxis.z - up.z * yAxis.y, up.z * yAxis.x - up.x * yAxis.z,
                         up.x * yAxis.y - up.y * yAxis.x};
                float xl = std::sqrt(xAxis.x * xAxis.x + xAxis.y * xAxis.y + xAxis.z * xAxis.z);
                xAxis.x /= xl;
                xAxis.y /= xl;
                xAxis.z /= xl;
            }
            Irufemi::Vector3 zAxis = {xAxis.y * yAxis.z - xAxis.z * yAxis.y, xAxis.z * yAxis.x - xAxis.x * yAxis.z,
                                      xAxis.x * yAxis.y - xAxis.y * yAxis.x};

            float thickness = currentBoneLength * 0.1f;
            xAxis.x *= thickness;
            xAxis.y *= thickness;
            xAxis.z *= thickness;
            yAxis.x *= currentBoneLength;
            yAxis.y *= currentBoneLength;
            yAxis.z *= currentBoneLength;
            zAxis.x *= thickness;
            zAxis.y *= thickness;
            zAxis.z *= thickness;

            // clang-format off
            Irufemi::Matrix4x4 boneWorld = {
                xAxis.x, xAxis.y, xAxis.z, 0.0f,
                yAxis.x, yAxis.y, yAxis.z, 0.0f,
                zAxis.x, zAxis.y, zAxis.z, 0.0f,
                parentPosition.x, parentPosition.y, parentPosition.z, 1.0f
            };
            // clang-format on

            float hue = depth * 30.0f;
            Irufemi::Vector4 color = HueToRGB(hue, 0.6f);
            boneMeshes_->AddInstanceWorld(boneWorld, color);
        }

        // 全てのボーンのローカル座標系（XYZ軸）を描画
        if (showAxes_) {
            Irufemi::Vector3 axisX = {jointWorldMat.m[0][0], jointWorldMat.m[0][1], jointWorldMat.m[0][2]};
            Irufemi::Vector3 axisY = {jointWorldMat.m[1][0], jointWorldMat.m[1][1], jointWorldMat.m[1][2]};
            Irufemi::Vector3 axisZ = {jointWorldMat.m[2][0], jointWorldMat.m[2][1], jointWorldMat.m[2][2]};

            auto Normalize = [](Irufemi::Vector3& v) {
                float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
                if (l > 0.0f) {
                    v.x /= l;
                    v.y /= l;
                    v.z /= l;
                }
            };
            Normalize(axisX);
            Normalize(axisY);
            Normalize(axisZ);

            // ボーンの長さに比例して軸の長さを決定
            float lineLen = currentBoneLength * axisScale_;

            debugAxesLines_->AddInstance(jointPosition,
                                         {jointPosition.x + axisX.x * lineLen, jointPosition.y + axisX.y * lineLen,
                                          jointPosition.z + axisX.z * lineLen},
                                         {1.0f, 0.0f, 0.0f, 1.0f});
            debugAxesLines_->AddInstance(jointPosition,
                                         {jointPosition.x + axisY.x * lineLen, jointPosition.y + axisY.y * lineLen,
                                          jointPosition.z + axisY.z * lineLen},
                                         {0.0f, 1.0f, 0.0f, 1.0f});
            debugAxesLines_->AddInstance(jointPosition,
                                         {jointPosition.x + axisZ.x * lineLen, jointPosition.y + axisZ.y * lineLen,
                                          jointPosition.z + axisZ.z * lineLen},
                                         {0.0f, 0.5f, 1.0f, 1.0f});
        }
    }
}

void SkeletonDebugRendererComponent::Draw() {
    if (showBones_) {
        boneMeshes_->SyncBeforeDraw();
        boneMeshes_->Draw();
    }
    if (showAxes_) {
        debugAxesLines_->SyncBeforeDraw();
        debugAxesLines_->Draw();
    }
}

nlohmann::json SkeletonDebugRendererComponent::Serialize() {
    nlohmann::json j = nlohmann::json::object();
    j["Show Bones"] = showBones_;
    j["Show Axes"] = showAxes_;
    j["Axis Scale"] = axisScale_;
    return j;
}

void SkeletonDebugRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("Show Bones")) {
        showBones_ = j["Show Bones"].get<bool>();
    }
    if (j.contains("Show Axes")) {
        showAxes_ = j["Show Axes"].get<bool>();
    }
    if (j.contains("Axis Scale")) {
        axisScale_ = j["Axis Scale"].get<float>();
    }
}

void SkeletonDebugRendererComponent::OnRegisterProperties() {
    RegisterProperty("Show Bones", &showBones_).SetTooltip("Toggle drawing of bone connections (octahedrons)");
    RegisterProperty("Show Axes", &showAxes_).SetTooltip("Toggle drawing of local XYZ axes for all joints");
    RegisterProperty("Axis Scale", &axisScale_)
        .SetTooltip("Scale multiplier for the local XYZ axes (dynamically scaled by bone length)")
        .SetMinMax(0.01f, 5.0f);
}
