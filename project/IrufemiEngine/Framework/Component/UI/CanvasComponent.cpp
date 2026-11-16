#include "Framework/Component/UI/CanvasComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"

namespace Irufemi {

void CanvasComponent::OnRegisterProperties() {
    RegisterProperty("Group Alpha", &groupAlpha_);
}

void CanvasComponent::Initialize() {}

static void ApplyAlphaRecursive(GameObject* obj, float groupAlpha) {
    if (!obj) {
        return;
    }

    // 自身のSpriteRendererがあればAlphaを元カラーに乗算して適用
    auto sprite = obj->GetComponent<SpriteRendererComponent>();
    if (sprite && sprite->GetSprite()) {
        Irufemi::Vector4 color = sprite->GetColor();
        color.w *= groupAlpha;
        sprite->GetSprite()->SetColor(color);
    }

    // 子へ再帰
    for (auto& child : obj->GetChildren()) {
        ApplyAlphaRecursive(child.get(), groupAlpha);
    }
}

void CanvasComponent::Update() {
    if (!gameObject_) {
        return;
    }

    // グループアルファが変更された時のみ子階層へ乗算反映
    if (std::abs(lastAppliedAlpha_ - groupAlpha_) > 0.0001f) {
        ApplyAlphaRecursive(gameObject_, groupAlpha_);
        lastAppliedAlpha_ = groupAlpha_;
    }
}

} // namespace Irufemi
