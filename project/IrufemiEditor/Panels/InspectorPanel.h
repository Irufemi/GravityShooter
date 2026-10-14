#pragma once

#ifdef EditorMode
#include "Core/IEditorPanel.h"
#include <string>

/**
 * @class InspectorPanel
 * @brief 選択されたGameObjectのコンポーネントを描画するパネル
 */
class InspectorPanel : public IEditorPanel {
public:
    void Initialize(EditorManager* editorManager) override;
    void Draw() override;
    const char* GetName() const override {
        return "Inspector";
    }

private:
    EditorManager* editorManager_ = nullptr;
    std::string editingStartName_; ///< 名前変更Undo用キャッシュ
};

#endif // EditorMode
