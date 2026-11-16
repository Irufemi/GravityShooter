#pragma once

#ifdef EditorMode
#include "IrufemiEditor/Core/IComponentEditor.h"
#include "Level/WaveManagerComponent.h"
#include <vector>

class WaveManagerComponentEditor : public IComponentEditor {
public:
    WaveManagerComponentEditor() = default;
    ~WaveManagerComponentEditor() override = default;

    void Draw(Irufemi::Component* component, class EditorActionManager* actionManager) override;

private:
    std::vector<WaveEventData> oldState_;
    bool isDraggingModified_ = false;
    int draggingNodeIndex_ = -1;
};
#endif
