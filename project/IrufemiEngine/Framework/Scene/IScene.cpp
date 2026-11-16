#include "Framework/Scene/IScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"

namespace Irufemi {

const std::vector<std::shared_ptr<GameObject>>& IScene::GetGameObjects() const {
    static const std::vector<std::shared_ptr<GameObject>> empty;
    return empty;
}

nlohmann::json IScene::Serialize() const {
    return nlohmann::json::object();
}

} // namespace Irufemi
