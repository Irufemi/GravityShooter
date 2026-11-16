#pragma once

#ifdef EditorMode
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace Irufemi {
class Component;
}
using Irufemi::Component;
class IComponentEditor;
class EditorActionManager;

/**
 * @class ComponentEditorRegistry
 * @brief コンポーネントの型(Type)と、それに対応する描画エディタを紐付けて管理する
 */
class ComponentEditorRegistry {
public:
    ComponentEditorRegistry();
    ~ComponentEditorRegistry();

    /**
     * @brief 全てのカスタムエディタを登録する
     */
    void RegisterAllEditors();

    /**
     * @brief 適切なエディタを探してコンポーネントのUIを描画する
     * @param[in] component 描画対象のコンポーネント
     * @param[in] actionManager Undo/Redo 用のアクションマネージャ
     */
    void DrawComponent(Component* component, EditorActionManager* actionManager);

    template <typename TComponent, typename TEditor> void RegisterEditor() {
        editors_[typeid(TComponent)] = std::make_unique<TEditor>();
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentEditor>> editors_;
};
#endif // EditorMode
