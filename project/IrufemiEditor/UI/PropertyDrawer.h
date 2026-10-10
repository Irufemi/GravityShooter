#pragma once

#ifdef EditorMode
#include <string>
#include <memory>
#include <unordered_map>
#include "Framework/Component/Component.h"

class EditorActionManager;

/**
 * @class IPropertyDrawer
 * @brief 個別プロパティ型の ImGui 描画および Undo コマンド生成を担当する戦略インターフェース (Property Drawer Pattern)
 */
class IPropertyDrawer {
public:
    virtual ~IPropertyDrawer() = default;

    /**
     * @brief プロパティの ImGui ウィジェットを描画する
     * @param hiddenName ウィジェット識別用ラベル (例: "##Health")
     * @param prop プロパティ定義
     * @param component オーナーコンポーネント
     * @param actionManager エディタ操作（Undo/Redo）マネージャ
     * @return 値が変更されたかどうか
     */
    virtual bool Draw(const std::string& hiddenName, const ComponentProperty& prop, Component* component,
                      EditorActionManager* actionManager) = 0;
};

/**
 * @class PropertyDrawerRegistry
 * @brief プロパティ型ごとの Drawer を管理・ディスパッチするレジストリ (Singleton)
 */
class PropertyDrawerRegistry {
public:
    static PropertyDrawerRegistry& GetInstance();

    /**
     * @brief 指定したプロパティ型に Drawer を登録する
     */
    void RegisterDrawer(ComponentPropertyType type, std::unique_ptr<IPropertyDrawer> drawer);

    /**
     * @brief 指定したプロパティ型に対応する Drawer を取得する
     */
    IPropertyDrawer* GetDrawer(ComponentPropertyType type) const;

private:
    PropertyDrawerRegistry();
    void RegisterDefaultDrawers();

    std::unordered_map<ComponentPropertyType, std::unique_ptr<IPropertyDrawer>> drawers_;
};

#endif
