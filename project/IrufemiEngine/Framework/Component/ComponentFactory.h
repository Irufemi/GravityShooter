#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "Framework/Component/Component.h"

namespace Irufemi {

class ComponentFactory {
public:
    using CreatorFunc = std::function<std::shared_ptr<Component>()>;

    struct ComponentRegistration {
        const char* category;
        CreatorFunc creator;
    };

    /**
     * @brief Register を実行する。
     */
    static void Register(const std::string& typeName, const char* category, CreatorFunc func);
    /**
     * @brief Create を実行する。
     */
    static std::shared_ptr<Component> Create(const std::string& typeName);
    static const std::unordered_map<std::string, ComponentRegistration>& GetFactoryMap();

    /// @brief エンジン組み込みのコンポーネントを一括登録する
    static void RegisterAllCoreComponents();

private:
    static std::unordered_map<std::string, ComponentRegistration>& GetMap();
};

} // namespace Irufemi

