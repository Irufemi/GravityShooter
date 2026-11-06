#pragma once
#ifdef EditorMode
#include "IrufemiEditor/Core/IComponentEditor.h"

#include "Core/Utility/JsonUtility.h"
#include <string>

class BossComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override;

private:
    nlohmann::json cachedJson_;
    std::string cachedPath_;
    bool isJsonLoaded_ = false;
};
#endif
