#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Physics/CollisionManager.h"

ColliderComponent::~ColliderComponent() {
    OnDestroy();
}

void ColliderComponent::Start() {
    // 有効なシーンに所属している実体のみ登録
    if (collisionManager_ && gameObject_ && gameObject_->GetScene() && gameObject_->GetIsActive()) {
        collisionManager_->RegisterCollider(this);
    }
}

void ColliderComponent::OnDestroy() {
    if (collisionManager_) {
        collisionManager_->UnregisterCollider(this);
    }
}

void ColliderComponent::OnEnable() {
    // シーンに正式に追加されている実体のみ物理マネージャに登録（テンプレート等の誤登録を防止）
    if (collisionManager_ && gameObject_ && gameObject_->GetScene()) {
        collisionManager_->RegisterCollider(this);
    }
}

void ColliderComponent::OnDisable() {
    if (collisionManager_) {
        collisionManager_->UnregisterCollider(this);
    }
}

void ColliderComponent::OnSetScene(BaseScene* scene) {
    if (collisionManager_ && gameObject_) {
        if (scene && gameObject_->GetIsActive()) {
            collisionManager_->RegisterCollider(this);
        } else if (!scene) {
            collisionManager_->UnregisterCollider(this);
        }
    }
}

void ColliderComponent::OnRegisterProperties() {
    RegisterProperty("Is Trigger", &isTrigger_);
    RegisterProperty("Is Static", &isStatic_);
    RegisterProperty("Pushback Mask X", &pushbackMask_.x);
    RegisterProperty("Pushback Mask Y", &pushbackMask_.y);
    RegisterProperty("Pushback Mask Z", &pushbackMask_.z);
}

nlohmann::json ColliderComponent::Serialize() {
    nlohmann::json j;
    j["isTrigger"] = isTrigger_;
    j["isStatic"] = isStatic_;
    j["layer"] = layer_;
    j["mask"] = mask_;
    j["pushbackMask"] = {pushbackMask_.x, pushbackMask_.y, pushbackMask_.z};
    return j;
}

void ColliderComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("isTrigger")) {
        isTrigger_ = j["isTrigger"];
    }
    if (j.contains("isStatic")) {
        isStatic_ = j["isStatic"];
    }
    if (j.contains("layer")) {
        layer_ = j["layer"];
    }
    if (j.contains("mask")) {
        mask_ = j["mask"];
    }
    if (j.contains("pushbackMask") && j["pushbackMask"].is_array() && j["pushbackMask"].size() == 3) {
        pushbackMask_.x = j["pushbackMask"][0];
        pushbackMask_.y = j["pushbackMask"][1];
        pushbackMask_.z = j["pushbackMask"][2];
    }
}

void ColliderComponent::CopyPropertiesFrom(const Component* other) {
    Component::CopyPropertiesFrom(other);
    if (const auto* otherCol = dynamic_cast<const ColliderComponent*>(other)) {
        layer_ = otherCol->layer_;
        mask_ = otherCol->mask_;
        isTrigger_ = otherCol->isTrigger_;
        isStatic_ = otherCol->isStatic_;
        pushbackMask_ = otherCol->pushbackMask_;
        debugCategory_ = otherCol->debugCategory_;
        debugCustomColor_ = otherCol->debugCustomColor_;
    }
}
