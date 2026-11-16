#pragma once

#ifdef EditorMode
#include "Core/IComponentEditor.h"
#include <string>

namespace Irufemi {
class GlobalPostProcessComponent;
}
using Irufemi::GlobalPostProcessComponent;

/**
 * @class GlobalPostProcessComponentEditor
 * @brief GlobalPostProcessComponent の専用エディタ UI
 */
class GlobalPostProcessComponentEditor : public IComponentEditor {
public:
    GlobalPostProcessComponentEditor() = default;
    ~GlobalPostProcessComponentEditor() override = default;

    void Draw(Component* component, EditorActionManager* actionManager) override;

private:
    void DrawFloatProperty(const char* label, float& value, float defaultValue, float minVal, float maxVal,
                           EditorActionManager* actionManager);
    void DrawBoolProperty(const char* label, bool& value, bool defaultValue, EditorActionManager* actionManager);
};

#endif // EditorMode
