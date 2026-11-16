#pragma once

#ifdef EditorMode
#include "Core/IComponentEditor.h"

namespace Irufemi {
class EffectMaskComponent;
}
using Irufemi::EffectMaskComponent;

class EffectMaskComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override;
};
#endif // EditorMode
