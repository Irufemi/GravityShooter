#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Math/MathFunction.h"
#include "Core/System/ComponentPool.h"
#include <cmath>

// Setters
void TransformComponent::SetPosition(const Irufemi::Vector3& position) {
    if (position_.x != position.x || position_.y != position.y || position_.z != position.z) {
        position_ = position;
        MarkLocalDirty();
    }
}

void TransformComponent::SetRotation(const Irufemi::Vector3& rotation) {
    SetRotationQuat(Irufemi::Math::ToQuaternionFromEuler(rotation));
}

void TransformComponent::SetRotationQuat(const Irufemi::Quaternion& rotation) {
    if (rotation_.x != rotation.x || rotation_.y != rotation.y || rotation_.z != rotation.z ||
        rotation_.w != rotation.w) {
        rotation_ = Irufemi::Math::Normalize(rotation);
        MarkLocalDirty();
    }
}

void TransformComponent::SetScale(const Irufemi::Vector3& scale) {
    if (scale_.x != scale.x || scale_.y != scale.y || scale_.z != scale.z) {
        scale_ = scale;
        MarkLocalDirty();
    }
}

// --- GetParentMatrixForChild ---
Irufemi::Matrix4x4 TransformComponent::GetParentMatrixForChild() const {
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                if (!inheritScale_) {
                    // スケールを除去し、回転と位置だけで再構築（せん断・マイナススケール破綻防止）
                    // 負のスケール時の回転抽出バグを防ぐため、安全に抽出されたWorldRotationQuat等を使用する
                    Irufemi::Vector3 pPos = parentT->GetWorldPosition();
                    Irufemi::Quaternion quat = parentT->GetWorldRotationQuat();
                    Irufemi::Vector3 pScale = {1.0f, 1.0f, 1.0f};
                    return Irufemi::Math::MakeAffineMatrix(pScale, quat, pPos);
                }
                return parentT->GetWorldMatrix();
            }
        }
    }
    return Irufemi::Math::MakeIdentity4x4();
}

// --- Setters (World) ---
void TransformComponent::SetWorldPosition(const Irufemi::Vector3& worldPosition) {
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                Irufemi::Vector3 pScale = parentT->GetWorldScale();
                if (std::abs(pScale.x) < 1e-6f || std::abs(pScale.y) < 1e-6f || std::abs(pScale.z) < 1e-6f) {
                    SetPosition(worldPosition);
                    return;
                }
                Irufemi::Matrix4x4 invMat = Irufemi::Math::Inverse(GetParentMatrixForChild());
                position_ = Irufemi::Math::Transform(worldPosition, invMat);
                MarkLocalDirty();
                return;
            }
        }
    }
    SetPosition(worldPosition);
}

void TransformComponent::SetWorldRotation(const Irufemi::Vector3& worldRotation) {
    SetWorldRotationQuat(Irufemi::Math::ToQuaternionFromEuler(worldRotation));
}

void TransformComponent::SetWorldRotationQuat(const Irufemi::Quaternion& worldRotation) {
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                Irufemi::Vector3 pScale = parentT->GetWorldScale();
                if (std::abs(pScale.x) < 1e-6f || std::abs(pScale.y) < 1e-6f || std::abs(pScale.z) < 1e-6f) {
                    SetRotationQuat(worldRotation);
                    return;
                }
                CheckAndComputeMatrix();
                Irufemi::Vector3 wPos = GetWorldPosition();
                Irufemi::Vector3 wScale = GetWorldScale();

                Irufemi::Matrix4x4 newWorldMat = Irufemi::Math::MakeAffineMatrix(wScale, worldRotation, wPos);
                Irufemi::Matrix4x4 invMat = Irufemi::Math::Inverse(GetParentMatrixForChild());
                Irufemi::Matrix4x4 localMat = Irufemi::Math::Multiply(newWorldMat, invMat);

                rotation_ = Irufemi::Math::ExtractRotationSafe(localMat, scale_);
                MarkLocalDirty();
                return;
            }
        }
    }
    SetRotationQuat(worldRotation);
}

void TransformComponent::SetWorldScale(const Irufemi::Vector3& worldScale) {
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                Irufemi::Vector3 pScale = parentT->GetWorldScale();
                if (std::abs(pScale.x) < 1e-6f || std::abs(pScale.y) < 1e-6f || std::abs(pScale.z) < 1e-6f) {
                    SetScale(worldScale);
                    return;
                }
                CheckAndComputeMatrix();
                Irufemi::Vector3 wPos = GetWorldPosition();
                Irufemi::Quaternion wRot = GetWorldRotationQuat();

                Irufemi::Matrix4x4 newWorldMat = Irufemi::Math::MakeAffineMatrix(worldScale, wRot, wPos);
                Irufemi::Matrix4x4 invMat = Irufemi::Math::Inverse(GetParentMatrixForChild());
                Irufemi::Matrix4x4 localMat = Irufemi::Math::Multiply(newWorldMat, invMat);

                Irufemi::Vector3 xaxis = {localMat.m[0][0], localMat.m[0][1], localMat.m[0][2]};
                Irufemi::Vector3 yaxis = {localMat.m[1][0], localMat.m[1][1], localMat.m[1][2]};
                Irufemi::Vector3 zaxis = {localMat.m[2][0], localMat.m[2][1], localMat.m[2][2]};

                // worldScale の符号と親の符号から、正確なローカルスケールの符号を逆算する
                float pSignX = 1.0f;
                float pSignY = 1.0f;
                float pSignZ = 1.0f;
                if (inheritScale_) {
                    Irufemi::Vector3 pScale = parentT->GetWorldScale();
                    pSignX = std::copysign(1.0f, pScale.x);
                    pSignY = std::copysign(1.0f, pScale.y);
                    pSignZ = std::copysign(1.0f, pScale.z);
                }

                scale_ = {std::copysign(Irufemi::Math::Length(xaxis), worldScale.x * pSignX),
                          std::copysign(Irufemi::Math::Length(yaxis), worldScale.y * pSignY),
                          std::copysign(Irufemi::Math::Length(zaxis), worldScale.z * pSignZ)};

                MarkLocalDirty();
                return;
            }
        }
    }
    SetScale(worldScale);
}

void TransformComponent::SetWorldMatrix(const Irufemi::Matrix4x4& worldMatrix) {
    Irufemi::Matrix4x4 localMat = worldMatrix;
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                Irufemi::Vector3 pScale = parentT->GetWorldScale();
                if (std::abs(pScale.x) < 1e-6f || std::abs(pScale.y) < 1e-6f || std::abs(pScale.z) < 1e-6f) {
                    // 親がゼロスケールの場合は逆算不能なため、worldMatrixをそのままローカル行列として扱う
                } else {
                    Irufemi::Matrix4x4 invMat = Irufemi::Math::Inverse(GetParentMatrixForChild());
                    localMat = Irufemi::Math::Multiply(worldMatrix, invMat);
                }
            }
        }
    }

    Irufemi::Math::DecomposeAffineMatrixSafe(localMat, scale_, position_, rotation_, scale_);
    MarkLocalDirty();
}

void TransformComponent::UpdateMatrixImmediate() {
    MarkWorldDirty();
    ComputeMatrix(true);
}

void TransformComponent::ExtractWorldTransform() const {
    if (isWorldTransformExtracted_) {
        return;
    }

    // 親のワールドスケールの符号と自身のローカルスケールの符号から、現在のワールドスケールの符号を決定する
    Irufemi::Vector3 worldSign = scale_;
    if (gameObject_) {
        if (auto parent = gameObject_->GetParent()) {
            if (auto parentT = parent->GetComponent<TransformComponent>()) {
                if (inheritScale_) {
                    Irufemi::Vector3 pScale = parentT->GetWorldScale();
                    // 浮動小数点のオーバーフロー/アンダーフローを防ぐため、符号のみを伝播させる
                    worldSign.x = std::copysign(1.0f, worldSign.x) * std::copysign(1.0f, pScale.x);
                    worldSign.y = std::copysign(1.0f, worldSign.y) * std::copysign(1.0f, pScale.y);
                    worldSign.z = std::copysign(1.0f, worldSign.z) * std::copysign(1.0f, pScale.z);
                }
            }
        }
    }

    Irufemi::Math::DecomposeAffineMatrixSafe(worldMatrix_, worldSign, worldPosition_, worldRotation_, worldScale_);
    isWorldTransformExtracted_ = true;
}

// Lazy Evaluated World Getters
const Irufemi::Vector3& TransformComponent::GetWorldPosition() const {
    CheckAndComputeMatrix();
    ExtractWorldTransform();
    return worldPosition_;
}

Irufemi::Vector3 TransformComponent::GetWorldRotation() const {
    return Irufemi::Math::ToEuler(GetWorldRotationQuat());
}

const Irufemi::Quaternion& TransformComponent::GetWorldRotationQuat() const {
    CheckAndComputeMatrix();
    ExtractWorldTransform();
    return worldRotation_;
}

const Irufemi::Vector3& TransformComponent::GetWorldScale() const {
    CheckAndComputeMatrix();
    ExtractWorldTransform();
    return worldScale_;
}

Irufemi::Vector3 TransformComponent::GetWorldRight() const {
    CheckAndComputeMatrix();
    return Irufemi::Math::Normalize(Irufemi::Vector3{worldMatrix_.m[0][0], worldMatrix_.m[0][1], worldMatrix_.m[0][2]});
}

Irufemi::Vector3 TransformComponent::GetWorldUp() const {
    CheckAndComputeMatrix();
    return Irufemi::Math::Normalize(Irufemi::Vector3{worldMatrix_.m[1][0], worldMatrix_.m[1][1], worldMatrix_.m[1][2]});
}

Irufemi::Vector3 TransformComponent::GetWorldForward() const {
    CheckAndComputeMatrix();
    return Irufemi::Math::Normalize(Irufemi::Vector3{worldMatrix_.m[2][0], worldMatrix_.m[2][1], worldMatrix_.m[2][2]});
}

void TransformComponent::ComputeMatrix(bool force) const {
    bool parentChanged = false;

    GameObject* currentParent = gameObject_ ? gameObject_->GetParent().get() : nullptr;
    TransformComponent* parentT = currentParent ? currentParent->GetComponent<TransformComponent>() : nullptr;

    if (parentT) {
        // 親がDirtyなら計算させる（再帰的）
        parentT->CheckAndComputeMatrix();

        uint64_t currentParentVersion = parentT->GetTransformVersion();
        uint64_t currentParentId = currentParent->GetInstanceID();
        if (parentTransformVersionLastComputed_ != currentParentVersion ||
            parentInstanceIdLastComputed_ != currentParentId) {
            parentChanged = true;
            parentTransformVersionLastComputed_ = currentParentVersion;
            parentInstanceIdLastComputed_ = currentParentId;
        }
    } else {
        if (parentInstanceIdLastComputed_ != 0) {
            parentChanged = true;
            parentInstanceIdLastComputed_ = 0;
            parentTransformVersionLastComputed_ = 0;
        }
    }

    // 強制更新でもなく、自分も親も変わっていないならスキップ (完全な遅延評価)
    if (!force && !isLocalDirty_ && !isWorldDirty_ && !parentChanged) {
        return;
    }

    // ローカル行列の計算（変更があった場合のみ）
    if (isLocalDirty_) {
        localMatrix_ = Irufemi::Math::MakeAffineMatrix(scale_, rotation_, position_);
        isLocalDirty_ = false;
        isWorldDirty_ = true; // ローカルが変わればワールドも必ず変わる
    }

    // ワールド行列の計算
    if (isWorldDirty_ || parentChanged || force) {
        if (parentT) {
            worldMatrix_ = Irufemi::Math::Multiply(localMatrix_, GetParentMatrixForChild());
        } else {
            worldMatrix_ = localMatrix_;
        }
        isWorldDirty_ = false;
        isWorldTransformExtracted_ = false; // ワールド座標が変化したので抽出フラグを下ろす
        transformVersion_++;                // ワールド行列が更新されたので自身のバージョンを上げる
    }
}

void TransformComponent::UpdateAll() {
    ComponentPool<TransformComponent>::GetInstance().ForEach(
        [](TransformComponent& transform) { transform.ComputeMatrix(false); });
}

nlohmann::json TransformComponent::Serialize() {
    nlohmann::json j;
    j["position"] = {position_.x, position_.y, position_.z};
    Irufemi::Vector3 euler = Irufemi::Math::ToEuler(rotation_);
    j["rotation"] = {euler.x, euler.y, euler.z};
    j["scale"] = {scale_.x, scale_.y, scale_.z};
    j["inheritScale"] = inheritScale_;
    return j;
}

void TransformComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3) {
        position_.x = j["position"][0];
        position_.y = j["position"][1];
        position_.z = j["position"][2];
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 3) {
        Irufemi::Vector3 euler;
        euler.x = j["rotation"][0];
        euler.y = j["rotation"][1];
        euler.z = j["rotation"][2];
        rotation_ = Irufemi::Math::ToQuaternionFromEuler(euler);
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3) {
        scale_.x = j["scale"][0];
        scale_.y = j["scale"][1];
        scale_.z = j["scale"][2];
    }
    if (j.contains("inheritScale") && j["inheritScale"].is_boolean()) {
        inheritScale_ = j["inheritScale"];
    }
    MarkLocalDirty(); // Deserialize時にDirtyにする
}

std::shared_ptr<Component> TransformComponent::Clone() {
    std::shared_ptr<TransformComponent> clone;
    if constexpr (IsPooledComponent<TransformComponent>::value) {
        clone = ComponentPool<TransformComponent>::GetInstance().Create();
    } else {
        clone = std::make_shared<TransformComponent>();
    }
    clone->CopyPropertiesFrom(this);
    clone->position_ = this->position_;
    clone->rotation_ = this->rotation_;
    clone->scale_ = this->scale_;
    clone->inheritScale_ = this->inheritScale_;
    // 行列等は初期化後にDirtyフラグ経由で再計算される
    return clone;
}
