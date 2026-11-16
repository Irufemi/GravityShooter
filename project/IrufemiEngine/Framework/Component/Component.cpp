#include "Framework/Component/Component.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/ComponentFactory.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"

namespace Irufemi {

TransformComponent* Component::GetTransform() const {
    if (gameObject_) {
        return gameObject_->GetTransform();
    }
    return nullptr;
}

BaseScene* Component::GetScene() const {
    if (gameObject_) {
        return gameObject_->GetScene();
    }
    return nullptr;
}

IrufemiEngine* Component::GetEngine() const {
    if (gameObject_ && gameObject_->GetScene()) {
        return gameObject_->GetScene()->GetEngine();
    }
    return nullptr;
}

std::shared_ptr<Component> Component::Clone() {
    auto clone = ComponentFactory::Create(GetComponentName());
    if (clone) {
        // Safe fallback for unoptimized components: JSON serialization route
        clone->Deserialize(this->Serialize());
    }
    return clone;
}

} // namespace Irufemi

