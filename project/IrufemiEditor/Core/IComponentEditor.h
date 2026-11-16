#pragma once

#ifdef EditorMode
namespace Irufemi {
class Component;
}
using Irufemi::Component;
class EditorActionManager;

/**
 * @class IComponentEditor
 * @brief 特定のコンポーネントのInspectorUIを描画するクラスの基底インターフェース
 */
class IComponentEditor {
public:
    virtual ~IComponentEditor() = default;

    /**
     * @brief 渡されたコンポーネントのプロパティをImGuiで描画する
     * @param[in] component 描画対象の基底コンポーネントポインタ
     * @param[in] actionManager Undo/Redo 用のアクションマネージャ
     */
    virtual void Draw(Component* component, EditorActionManager* actionManager) = 0;
};
#endif // EditorMode
