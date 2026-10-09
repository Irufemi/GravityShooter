#include "UI/PropertyDrawer.h"

#ifdef EditorMode
#include "UI/ComponentUIHelpers.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/Utility/FileSystem.h"
#include "Core/Utility/JsonUtility.h"
#include "Core/EditorManager.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include "imgui/imgui.h"
#include <filesystem>
#include <algorithm>
#include <vector>

namespace {

// ============================================================================
// FloatPropertyDrawer
// ============================================================================
class FloatPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        float* ptr = prop.GetData<float>();
        if (!ptr) {
            ptr = static_cast<float*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool changed = false;
        if (prop.minVal != prop.maxVal) {
            if (ImGui::SliderFloat(hiddenName.c_str(), ptr, prop.minVal, prop.maxVal)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        } else {
            if (ImGui::DragFloat(hiddenName.c_str(), ptr, 0.1f)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        }
        ComponentUIHelpers::CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
        return changed;
    }
};

// ============================================================================
// IntPropertyDrawer
// ============================================================================
class IntPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        int* ptr = prop.GetData<int>();
        if (!ptr) {
            ptr = static_cast<int*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool changed = false;
        if (prop.minVal != prop.maxVal) {
            if (ImGui::SliderInt(hiddenName.c_str(), ptr, static_cast<int>(prop.minVal),
                                 static_cast<int>(prop.maxVal))) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        } else {
            if (ImGui::DragInt(hiddenName.c_str(), ptr, 1)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        }
        ComponentUIHelpers::CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
        return changed;
    }
};

// ============================================================================
// BoolPropertyDrawer
// ============================================================================
class BoolPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        bool* ptr = prop.GetData<bool>();
        if (!ptr) {
            ptr = static_cast<bool*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool oldVal = *ptr;
        if (ImGui::Checkbox(hiddenName.c_str(), ptr)) {
            if (prop.onChanged) {
                prop.onChanged();
            }
            ComponentUIHelpers::PushInstantUndo(actionManager, oldVal, *ptr, ptr, prop.onChanged);
            return true;
        }
        return false;
    }
};

// ============================================================================
// Float2PropertyDrawer
// ============================================================================
class Float2PropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        auto* ptr = prop.GetData<Irufemi::Vector2>();
        if (!ptr) {
            ptr = reinterpret_cast<Irufemi::Vector2*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool changed = false;
        if (ImGui::DragFloat2(hiddenName.c_str(), &ptr->x, 0.1f)) {
            changed = true;
            if (prop.onChanged) {
                prop.onChanged();
            }
        }
        ComponentUIHelpers::CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
        return changed;
    }
};

// ============================================================================
// Float3PropertyDrawer
// ============================================================================
class Float3PropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        auto* ptr = prop.GetData<Irufemi::Vector3>();
        if (!ptr) {
            ptr = reinterpret_cast<Irufemi::Vector3*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool changed = false;
        if (prop.name.find("Color") != std::string::npos || prop.name.find("color") != std::string::npos) {
            if (ImGui::ColorEdit3(hiddenName.c_str(), &ptr->x)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        } else {
            if (ImGui::DragFloat3(hiddenName.c_str(), &ptr->x, 0.1f)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        }
        ComponentUIHelpers::CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
        return changed;
    }
};

// ============================================================================
// Float4PropertyDrawer
// ============================================================================
class Float4PropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        auto* ptr = prop.GetData<Irufemi::Vector4>();
        if (!ptr) {
            ptr = reinterpret_cast<Irufemi::Vector4*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        bool changed = false;
        if (prop.name.find("Color") != std::string::npos || prop.name.find("color") != std::string::npos) {
            if (ImGui::ColorEdit4(hiddenName.c_str(), &ptr->x)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        } else {
            if (ImGui::DragFloat4(hiddenName.c_str(), &ptr->x, 0.1f)) {
                changed = true;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
        }
        ComponentUIHelpers::CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
        return changed;
    }
};

// ============================================================================
// EnumPropertyDrawer
// ============================================================================
class EnumPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* /*component*/, EditorActionManager* actionManager) override {
        int* ptr = prop.GetData<int>();
        if (!ptr) {
            ptr = static_cast<int*>(prop.GetRawData());
        }
        if (!ptr || prop.enumNames.empty()) {
            return false;
        }

        std::vector<const char*> cStrs;
        for (const auto& s : prop.enumNames) {
            cStrs.push_back(s.c_str());
        }
        int oldVal = *ptr;
        if (ImGui::Combo(hiddenName.c_str(), ptr, cStrs.data(), static_cast<int>(cStrs.size()))) {
            if (prop.onChanged) {
                prop.onChanged();
            }
            ComponentUIHelpers::PushInstantUndo(actionManager, oldVal, *ptr, ptr, prop.onChanged);
            return true;
        }
        return false;
    }
};

// ============================================================================
// StringPropertyDrawer
// ============================================================================
class StringPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* component, EditorActionManager* actionManager) override {
        auto* str = prop.GetData<std::string>();
        if (!str) {
            str = static_cast<std::string*>(prop.GetRawData());
        }
        if (!str) {
            return false;
        }

        std::string lowerName = prop.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool isModel = (lowerName.find("model") != std::string::npos || lowerName.find("mesh") != std::string::npos);
        bool isTexture = (lowerName.find("texture") != std::string::npos || lowerName.find("image") != std::string::npos);
        bool isAnimation = (lowerName.find("animation") != std::string::npos || lowerName.find("anim") != std::string::npos);
        bool isPrefabProp = (lowerName.find("prefab") != std::string::npos);

        std::vector<std::string> comboItems;
        IrufemiEngine* engine = nullptr;
        if (component && component->GetGameObject() && component->GetGameObject()->GetScene()) {
            engine = component->GetGameObject()->GetScene()->GetEngine();
        }

        if (engine && isModel && engine->GetObjModelManager()) {
            auto* mgr = engine->GetObjModelManager();
#ifndef NDEBUG
            mgr->RefreshAvailableModels();
#endif
            comboItems = mgr->GetAvailableModels();
        } else if (engine && isTexture && engine->GetTextureManager()) {
            comboItems = engine->GetTextureManager()->GetTextureNamesForDebug();
        } else if (engine && isAnimation && engine->GetAnimationManager()) {
            auto* mgr = engine->GetAnimationManager();
#ifndef NDEBUG
            mgr->RefreshAvailableAnimations();
#endif
            comboItems = mgr->GetAvailableAnimations();
        }

        if (!comboItems.empty()) {
            if (ImGui::BeginCombo(hiddenName.c_str(), str->c_str())) {
                for (const auto& item : comboItems) {
                    bool isSelected = (*str == item);
                    if (ImGui::Selectable(item.c_str(), isSelected)) {
                        std::string oldVal = *str;
                        *str = item;
                        auto cb = prop.onChanged;
                        if (cb) {
                            cb();
                        }
                        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                            oldVal, *str, [str, cb](const std::string& v) {
                                *str = v;
                                if (cb) {
                                    cb();
                                }
                            }));
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else if (isPrefabProp) {
            static std::vector<std::string> cachedPrefabs;
            static float lastCacheTime = -10.0f;
            float curTime = static_cast<float>(ImGui::GetTime());
            if (curTime - lastCacheTime > 2.0f || cachedPrefabs.empty()) {
                cachedPrefabs.clear();
                std::string prefabDir = FileSystem::GetResourcePath("prefabs");
                if (!std::filesystem::exists(prefabDir)) {
                    prefabDir = "resources/prefabs";
                }
                if (std::filesystem::exists(prefabDir)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(prefabDir)) {
                        if (entry.is_regular_file()) {
                            auto ext = entry.path().extension().string();
                            if (ext == ".json" || ext == ".prefab") {
                                cachedPrefabs.push_back("resources/prefabs/" + entry.path().filename().generic_string());
                            }
                        }
                    }
                }
                std::sort(cachedPrefabs.begin(), cachedPrefabs.end());
                lastCacheTime = curTime;
            }

            ImGui::SetNextItemWidth((std::max)(50.0f, ImGui::GetContentRegionAvail().x - 70.0f));

            static std::unordered_map<std::string, std::vector<std::string>> prefabComponentCache;
            auto PrefabHasComponent = [](const std::string& path, const std::string& requiredComp) -> bool {
                if (requiredComp.empty()) {
                    return true;
                }
                auto it = prefabComponentCache.find(path);
                if (it == prefabComponentCache.end()) {
                    std::vector<std::string> compNames;
                    nlohmann::json j;
                    if (Irufemi::JsonUtility::LoadFromFile(path, j)) {
                        if (j.contains("components") && j["components"].is_array()) {
                            for (const auto& compObj : j["components"]) {
                                if (compObj.contains("type") && compObj["type"].is_string()) {
                                    compNames.push_back(compObj["type"].get<std::string>());
                                }
                            }
                        }
                    }
                    it = prefabComponentCache.emplace(path, std::move(compNames)).first;
                }
                return std::find(it->second.begin(), it->second.end(), requiredComp) != it->second.end();
            };

            std::vector<std::string> displayPrefabs;
            for (const auto& p : cachedPrefabs) {
                if (PrefabHasComponent(p, prop.prefabFilterComponent)) {
                    displayPrefabs.push_back(p);
                }
            }

            if (ImGui::BeginCombo(hiddenName.c_str(), str->c_str())) {
                for (const auto& item : displayPrefabs) {
                    bool isSelected = (*str == item);
                    if (ImGui::Selectable(item.c_str(), isSelected)) {
                        std::string oldVal = *str;
                        *str = item;
                        auto cb = prop.onChanged;
                        if (cb) {
                            cb();
                        }
                        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                            oldVal, *str, [str, cb](const std::string& v) {
                                *str = v;
                                if (cb) {
                                    cb();
                                }
                            }));
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (!str->empty()) {
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FA_WRENCH " Open##") + prop.name).c_str(), ImVec2(65.0f, 0))) {
                    if (auto em = EditorManager::GetInstance()) {
                        em->EnterPrefabMode(*str);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Open in Prefab Edit Mode");
                }
            }
        } else {
            char buffer[256];
            strncpy_s(buffer, sizeof(buffer), str->c_str(), _TRUNCATE);

            static std::unordered_map<const void*, std::string> activeStringSessions;
            if (ImGui::InputText(hiddenName.c_str(), buffer, sizeof(buffer))) {
                *str = buffer;
                if (prop.onChanged) {
                    prop.onChanged();
                }
            }
            if (ImGui::IsItemActivated()) {
                activeStringSessions[str] = *str;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                auto it = activeStringSessions.find(str);
                if (it != activeStringSessions.end()) {
                    std::string startStr = it->second;
                    std::string endStr = *str;
                    activeStringSessions.erase(it);
                    auto cb = prop.onChanged;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                        startStr, endStr, [str, cb](const std::string& v) {
                            *str = v;
                            if (cb) {
                                cb();
                            }
                        }));
                }
            }
        }

        // ドラッグ＆ドロップ対応
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                const char* path = static_cast<const char*>(payload->Data);
                std::string droppedPathStr = path;
                std::replace(droppedPathStr.begin(), droppedPathStr.end(), '\\', '/');

                std::string lowerPath = droppedPathStr;
                std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

                size_t resPos = lowerPath.find("resources/");
                if (resPos != std::string::npos) {
                    droppedPathStr = droppedPathStr.substr(resPos);
                }

                std::string oldVal = *str;
                *str = droppedPathStr;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                    oldVal, droppedPathStr, [str](const std::string& v) { *str = v; }));
            }
            ImGui::EndDragDropTarget();
        }
        return true;
    }
};

// ============================================================================
// GameObjectRefPropertyDrawer
// ============================================================================
class GameObjectRefPropertyDrawer : public IPropertyDrawer {
public:
    bool Draw(const std::string& hiddenName, const ComponentProperty& prop,
              Component* component, EditorActionManager* actionManager) override {
        uint64_t* ptr = prop.GetData<uint64_t>();
        if (!ptr) {
            ptr = static_cast<uint64_t*>(prop.GetRawData());
        }
        if (!ptr) {
            return false;
        }

        std::vector<std::shared_ptr<GameObject>> allObjs;
        if (component && component->GetGameObject() && component->GetGameObject()->GetScene()) {
            auto rootObjs = component->GetGameObject()->GetScene()->GetGameObjects();
            std::function<void(const std::vector<std::shared_ptr<GameObject>>&)> addObjs =
                [&](const std::vector<std::shared_ptr<GameObject>>& objs) {
                    for (const auto& o : objs) {
                        if (o && !o->IsDestroyed()) {
                            allObjs.push_back(o);
                            addObjs(o->GetChildren());
                        }
                    }
                };
            addObjs(rootObjs);
        }

        std::string currentName = "None";
        if (*ptr != 0 && component && component->GetGameObject() && component->GetGameObject()->GetScene()) {
            auto currentObj = component->GetGameObject()->GetScene()->FindGameObjectByID(*ptr);
            if (currentObj) {
                currentName = currentObj->GetName();
            }
        }

        if (ImGui::BeginCombo(hiddenName.c_str(), currentName.c_str())) {
            if (ImGui::Selectable("None", *ptr == 0)) {
                uint64_t oldVal = *ptr;
                *ptr = 0;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<uint64_t>>(
                    oldVal, 0, [ptr](const uint64_t& v) { *ptr = v; }));
            }
            for (const auto& obj : allObjs) {
                if (!obj || obj->IsDestroyed()) {
                    continue;
                }
                bool isSelected = (*ptr == obj->GetInstanceID());
                std::string displayName = obj->GetName();
                if (displayName.empty()) {
                    displayName = "Unnamed Object";
                }

                if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                    uint64_t oldVal = *ptr;
                    uint64_t newVal = obj->GetInstanceID();
                    *ptr = newVal;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<uint64_t>>(
                        oldVal, newVal, [ptr](const uint64_t& v) { *ptr = v; }));
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return true;
    }
};

} // namespace

// ============================================================================
// PropertyDrawerRegistry 実装
// ============================================================================

PropertyDrawerRegistry& PropertyDrawerRegistry::GetInstance() {
    static PropertyDrawerRegistry instance;
    return instance;
}

PropertyDrawerRegistry::PropertyDrawerRegistry() {
    RegisterDefaultDrawers();
}

void PropertyDrawerRegistry::RegisterDrawer(ComponentPropertyType type, std::unique_ptr<IPropertyDrawer> drawer) {
    drawers_[type] = std::move(drawer);
}

IPropertyDrawer* PropertyDrawerRegistry::GetDrawer(ComponentPropertyType type) const {
    auto it = drawers_.find(type);
    if (it != drawers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void PropertyDrawerRegistry::RegisterDefaultDrawers() {
    RegisterDrawer(ComponentPropertyType::Float,         std::make_unique<FloatPropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Int,           std::make_unique<IntPropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Bool,          std::make_unique<BoolPropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Float2,        std::make_unique<Float2PropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Float3,        std::make_unique<Float3PropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Float4,        std::make_unique<Float4PropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::Enum,          std::make_unique<EnumPropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::String,        std::make_unique<StringPropertyDrawer>());
    RegisterDrawer(ComponentPropertyType::GameObjectRef, std::make_unique<GameObjectRefPropertyDrawer>());
}

#endif
