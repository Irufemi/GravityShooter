#include "UI/ComponentUIHelpers.h"

#ifdef EditorMode
#include "Core/System/IrufemiEngine.h"
#include "Physics/CollisionManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Resource/Texture/TextureManager.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include "Framework/Component/Utility/SplineComponent.h"
#include "Framework/Component/Utility/SplineNodeComponent.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/EditorManager.h"
#include <algorithm>
#include <functional>

std::shared_ptr<Component> ComponentUIHelpers::GetSharedComponent(GameObject* go, Component* comp) {
    if (!go || !comp) {
        return nullptr;
    }
    for (auto& c : go->GetComponents()) {
        if (c.get() == comp) {
            return c;
        }
    }
    return nullptr;
}

void ComponentUIHelpers::DrawCollisionLayerGUI(Component* comp, EditorActionManager* actionManager, uint32_t& layer,
                                               uint32_t& mask) {
    auto* go = comp->GetGameObject();
    auto* scene = go ? go->GetScene() : nullptr;
    auto* cm = scene ? scene->GetEngine()->GetCollisionManager() : nullptr;
    if (!cm) {
        return;
    }

    const auto& layerNames = cm->GetLayerNames();
    if (layerNames.empty()) {
        return;
    }

    if (BeginPropertyTable("CollisionLayerTable")) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Collision Settings");
        ImGui::TableSetColumnIndex(1);
        ImGui::Separator();
        ImGui::TableSetColumnIndex(2);
        ImGui::Separator();

        int currentLayerIndex = 0;
        for (int i = 0; i < layerNames.size(); ++i) {
            if (layer == (1u << i)) {
                currentLayerIndex = i;
                break;
            }
        }

        ImGui::TableNextRow();
        DrawPropertyLabel("Layer");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1);
        if (ImGui::BeginCombo("##Layer", layerNames[currentLayerIndex].c_str())) {
            for (int i = 0; i < layerNames.size(); ++i) {
                bool isSelected = (currentLayerIndex == i);
                if (ImGui::Selectable(layerNames[i].c_str(), isSelected)) {
                    uint32_t newLayer = (1u << i);
                    PushInstantUndo(actionManager, layer, newLayer, &layer);
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        DrawPropertyResetButton("##LayerReset", layer != 1u, [&]() {
            uint32_t oldL = layer;
            PushInstantUndo(actionManager, oldL, 1u, &layer);
        });

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        bool treeOpen =
            ImGui::TreeNodeEx("Collision Mask", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowOverlap);

        if (treeOpen) {
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("All", ImVec2(50, 0))) {
                PushInstantUndo(actionManager, mask, 0xFFFFFFFF, &mask);
            }
            ImGui::SameLine();
            if (ImGui::Button("None", ImVec2(50, 0))) {
                PushInstantUndo(actionManager, mask, 0u, &mask);
            }

            for (int i = 0; i < layerNames.size(); ++i) {
                ImGui::TableNextRow();
                DrawPropertyLabel(layerNames[i].c_str());
                ImGui::TableSetColumnIndex(1);

                bool isMasked = (mask & (1u << i)) != 0;
                if (ImGui::Checkbox((std::string("##Mask") + std::to_string(i)).c_str(), &isMasked)) {
                    uint32_t newMask = mask;
                    if (isMasked) {
                        newMask |= (1u << i);
                    } else {
                        newMask &= ~(1u << i);
                    }
                    PushInstantUndo(actionManager, mask, newMask, &mask);
                }
            }
            ImGui::TreePop();
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Edit Layers...")) {
            ImGui::OpenPopup("Edit Layers");
        }

        EndPropertyTable();
    }

    if (ImGui::BeginPopupModal("Edit Layers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Manage Collision Layers");
        ImGui::Separator();

        for (int i = 0; i < layerNames.size(); ++i) {
            ImGui::PushID(i);
            ImGui::Text("%2d: ", i);
            ImGui::SameLine();

            if (i == 0) {
                ImGui::TextDisabled("%s (Fixed)", layerNames[i].c_str());
            } else {
                char nameBuffer[64];
                strncpy_s(nameBuffer, sizeof(nameBuffer), layerNames[i].c_str(), _TRUNCATE);

                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
                    cm->RenameLayer(i, nameBuffer);
                }
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    cm->RemoveLayer(i);
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
        }

        if (layerNames.size() < 32) {
            if (ImGui::Button("+ Add New Layer")) {
                cm->AddLayer("New Layer");
            }
        } else {
            ImGui::TextDisabled("Max layers reached (32).");
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ComponentUIHelpers::DrawFallbackPropertiesGUI(Component* component, EditorActionManager* actionManager) {
    const auto& props = component->GetProperties();
    if (props.empty()) {
        return;
    }

    ImGui::PushID(component);
    bool headerOpen = ImGui::CollapsingHeader(component->GetComponentName().c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    bool pendingRemove = false;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Remove Component")) {
            pendingRemove = true;
        }
        ImGui::EndPopup();
    }
    if (pendingRemove) {
        actionManager->PushAndExecute(std::make_unique<RemoveComponentCommand>(
            component->GetGameObject()->shared_from_this(), GetSharedComponent(component->GetGameObject(), component)));
    }

    if (headerOpen) {
        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
        if (ImGui::BeginTable("PropertiesTable", 3, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 24.0f);

            for (const auto& prop : props) {
                ImGui::PushID(prop.name.c_str());

                auto drawResetButton = [&]() {
                    if (!prop.defaultValue.is_null()) {
                        bool isModified = false;
                        auto isFloatDiff = [](float a, float b) { return std::abs(a - b) > 1e-5f; };

                        switch (prop.type) {
                        case ComponentPropertyType::Float: {
                            if (auto* v = prop.GetData<float>()) {
                                isModified = isFloatDiff(*v, prop.defaultValue.get<float>());
                            }
                            break;
                        }
                        case ComponentPropertyType::Enum:
                        case ComponentPropertyType::Int: {
                            if (auto* v = prop.GetData<int>()) {
                                isModified = (*v != prop.defaultValue.get<int>());
                            }
                            break;
                        }
                        case ComponentPropertyType::Bool: {
                            if (auto* v = prop.GetData<bool>()) {
                                isModified = (*v != prop.defaultValue.get<bool>());
                            }
                            break;
                        }
                        case ComponentPropertyType::String: {
                            if (auto* v = prop.GetData<std::string>()) {
                                isModified = (*v != prop.defaultValue.get<std::string>());
                            }
                            break;
                        }
                        case ComponentPropertyType::GameObjectRef: {
                            if (auto* v = prop.GetData<uint64_t>()) {
                                isModified = (*v != prop.defaultValue.get<uint64_t>());
                            }
                            break;
                        }
                        case ComponentPropertyType::Float2: {
                            if (auto* v = prop.GetData<Irufemi::Vector2>()) {
                                auto arr = prop.defaultValue;
                                if (arr.is_array() && arr.size() >= 2) {
                                    isModified = (isFloatDiff(v->x, arr[0].get<float>()) ||
                                                  isFloatDiff(v->y, arr[1].get<float>()));
                                }
                            }
                            break;
                        }
                        case ComponentPropertyType::Float3: {
                            if (auto* v = prop.GetData<Irufemi::Vector3>()) {
                                auto arr = prop.defaultValue;
                                if (arr.is_array() && arr.size() >= 3) {
                                    isModified = (isFloatDiff(v->x, arr[0].get<float>()) ||
                                                  isFloatDiff(v->y, arr[1].get<float>()) ||
                                                  isFloatDiff(v->z, arr[2].get<float>()));
                                }
                            }
                            break;
                        }
                        case ComponentPropertyType::Float4: {
                            if (auto* v = prop.GetData<Irufemi::Vector4>()) {
                                auto arr = prop.defaultValue;
                                if (arr.is_array() && arr.size() >= 4) {
                                    isModified = (isFloatDiff(v->x, arr[0].get<float>()) ||
                                                  isFloatDiff(v->y, arr[1].get<float>()) ||
                                                  isFloatDiff(v->z, arr[2].get<float>()) ||
                                                  isFloatDiff(v->w, arr[3].get<float>()));
                                }
                            }
                            break;
                        }
                        default:
                            break;
                        }

                        if (isModified) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                            if (ImGui::Button((std::string(ICON_FA_ARROW_ROTATE_LEFT) + "##" + prop.name).c_str(),
                                              ImVec2(20, 0))) {
                                switch (prop.type) {
                                case ComponentPropertyType::Float: {
                                    if (auto* ptr = prop.GetData<float>()) {
                                        PushInstantUndo(actionManager, *ptr, prop.defaultValue.get<float>(), ptr,
                                                        prop.onChanged);
                                    }
                                    break;
                                }
                                case ComponentPropertyType::Enum:
                                case ComponentPropertyType::Int: {
                                    if (auto* ptr = prop.GetData<int>()) {
                                        PushInstantUndo(actionManager, *ptr, prop.defaultValue.get<int>(), ptr,
                                                        prop.onChanged);
                                    }
                                    break;
                                }
                                case ComponentPropertyType::Bool: {
                                    if (auto* ptr = prop.GetData<bool>()) {
                                        PushInstantUndo(actionManager, *ptr, prop.defaultValue.get<bool>(), ptr,
                                                        prop.onChanged);
                                    }
                                    break;
                                }
                                case ComponentPropertyType::String: {
                                    if (auto* ptr = prop.GetData<std::string>()) {
                                        PushInstantUndo(actionManager, *ptr, prop.defaultValue.get<std::string>(), ptr,
                                                        prop.onChanged);
                                    }
                                    break;
                                }
                                case ComponentPropertyType::GameObjectRef: {
                                    if (auto* ptr = prop.GetData<uint64_t>()) {
                                        PushInstantUndo(actionManager, *ptr, prop.defaultValue.get<uint64_t>(), ptr,
                                                        prop.onChanged);
                                    }
                                    break;
                                }
                                case ComponentPropertyType::Float2: {
                                    if (auto* ptr = prop.GetData<Irufemi::Vector2>()) {
                                        auto arr = prop.defaultValue;
                                        if (arr.is_array() && arr.size() >= 2) {
                                            Irufemi::Vector2 defaultVal{arr[0].get<float>(), arr[1].get<float>()};
                                            PushInstantUndo(actionManager, *ptr, defaultVal, ptr, prop.onChanged);
                                        }
                                    }
                                    break;
                                }
                                case ComponentPropertyType::Float3: {
                                    if (auto* ptr = prop.GetData<Irufemi::Vector3>()) {
                                        auto arr = prop.defaultValue;
                                        if (arr.is_array() && arr.size() >= 3) {
                                            Irufemi::Vector3 defaultVal{arr[0].get<float>(), arr[1].get<float>(),
                                                                        arr[2].get<float>()};
                                            PushInstantUndo(actionManager, *ptr, defaultVal, ptr, prop.onChanged);
                                        }
                                    }
                                    break;
                                }
                                case ComponentPropertyType::Float4: {
                                    if (auto* ptr = prop.GetData<Irufemi::Vector4>()) {
                                        auto arr = prop.defaultValue;
                                        if (arr.is_array() && arr.size() >= 4) {
                                            Irufemi::Vector4 defaultVal{arr[0].get<float>(), arr[1].get<float>(),
                                                                        arr[2].get<float>(), arr[3].get<float>()};
                                            PushInstantUndo(actionManager, *ptr, defaultVal, ptr, prop.onChanged);
                                        }
                                    }
                                    break;
                                }
                                default:
                                    break;
                                }
                            }
                            ImGui::PopStyleColor();
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Reset to Default");
                            }
                        }
                    }
                };

                ImGui::TableNextRow();

                if (prop.type == ComponentPropertyType::Header) {
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", prop.name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Separator();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Separator();
                } else if (prop.type == ComponentPropertyType::Separator) {
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Separator();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Separator();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Separator();
                } else if (prop.type == ComponentPropertyType::Float3Array) {
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    bool treeOpen =
                        ImGui::TreeNodeEx(("##Tree" + prop.name).c_str(),
                                          ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen |
                                              ImGuiTreeNodeFlags_AllowOverlap,
                                          "%s", prop.name.c_str());
                    if (!prop.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("%s", prop.tooltip.c_str());
                    }

                    ImGui::TableSetColumnIndex(1);
                    auto* arr = static_cast<std::vector<Irufemi::Vector3>*>(prop.GetRawData());
                    int size = static_cast<int>(arr->size());
                    ImGui::PushItemWidth(-1);
                    if (ImGui::InputInt(("##Size" + prop.name).c_str(), &size)) {
                        if (size >= 0) {
                            std::vector<Irufemi::Vector3> oldArr = *arr;
                            arr->resize(size);
                            std::vector<Irufemi::Vector3> newArr = *arr;
                            PushInstantUndo(actionManager, oldArr, newArr, arr);
                        }
                    }
                    ImGui::PopItemWidth();

                    if (treeOpen) {
                        for (size_t i = 0; i < arr->size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);

                            ImGui::AlignTextToFramePadding();
                            ImGui::TreeNodeEx("ElementNode",
                                              ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                  ImGuiTreeNodeFlags_Bullet,
                                              "Element %d", (int)i);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
                            ImGui::DragFloat3("##Element", &(*arr)[i].x, 0.1f);
                            CheckUndoRedoDrag(actionManager, &(*arr)[i]);
                            ImGui::PopItemWidth();

                            ImGui::SameLine();
                            if (ImGui::Button("-", ImVec2(24, 0))) {
                                std::vector<Irufemi::Vector3> oldArr = *arr;
                                arr->erase(arr->begin() + i);
                                std::vector<Irufemi::Vector3> newArr = *arr;
                                PushInstantUndo(actionManager, oldArr, newArr, arr);
                                ImGui::PopID();
                                break;
                            }
                            ImGui::PopID();
                        }
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::Button("+", ImVec2(-1, 0))) {
                            std::vector<Irufemi::Vector3> oldArr = *arr;
                            arr->push_back(Irufemi::Vector3{0, 0, 0});
                            std::vector<Irufemi::Vector3> newArr = *arr;
                            PushInstantUndo(actionManager, oldArr, newArr, arr);
                        }
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%s", prop.name.c_str());
                    if (!prop.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("%s", prop.tooltip.c_str());
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushItemWidth(-1);
                    std::string hiddenName = "##" + prop.name;

                    switch (prop.type) {
                    case ComponentPropertyType::Float: {
                        float* ptr = static_cast<float*>(prop.GetRawData());
                        if (prop.minVal != prop.maxVal) {
                            if (ImGui::SliderFloat(hiddenName.c_str(), ptr, prop.minVal, prop.maxVal)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        } else {
                            if (ImGui::DragFloat(hiddenName.c_str(), ptr, 0.1f)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        }
                        CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
                        break;
                    }
                    case ComponentPropertyType::Enum: {
                        int* ptr = static_cast<int*>(prop.GetRawData());
                        if (!prop.enumNames.empty()) {
                            std::vector<const char*> cStrs;
                            for (const auto& s : prop.enumNames) {
                                cStrs.push_back(s.c_str());
                            }
                            int oldVal = *ptr;
                            if (ImGui::Combo(hiddenName.c_str(), ptr, cStrs.data(), static_cast<int>(cStrs.size()))) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                                PushInstantUndo(actionManager, oldVal, *ptr, ptr, prop.onChanged);
                            }
                        }
                        break;
                    }
                    case ComponentPropertyType::Int: {
                        int* ptr = static_cast<int*>(prop.GetRawData());
                        if (prop.minVal != prop.maxVal) {
                            if (ImGui::SliderInt(hiddenName.c_str(), ptr, static_cast<int>(prop.minVal),
                                                 static_cast<int>(prop.maxVal))) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        } else {
                            if (ImGui::DragInt(hiddenName.c_str(), ptr, 1)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        }
                        CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
                        break;
                    }
                    case ComponentPropertyType::Bool: {
                        bool* ptr = static_cast<bool*>(prop.GetRawData());
                        bool oldVal = *ptr;
                        if (ImGui::Checkbox(hiddenName.c_str(), ptr)) {
                            if (prop.onChanged) {
                                prop.onChanged();
                            }
                            PushInstantUndo(actionManager, oldVal, *ptr, ptr, prop.onChanged);
                        }
                        break;
                    }
                    case ComponentPropertyType::Float2: {
                        Irufemi::Vector2* ptr = reinterpret_cast<Irufemi::Vector2*>(prop.GetRawData());
                        if (ImGui::DragFloat2(hiddenName.c_str(), &ptr->x, 0.1f)) {
                            if (prop.onChanged) {
                                prop.onChanged();
                            }
                        }
                        CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
                        break;
                    }
                    case ComponentPropertyType::Float3: {
                        Irufemi::Vector3* ptr = reinterpret_cast<Irufemi::Vector3*>(prop.GetRawData());
                        if (prop.name.find("Color") != std::string::npos ||
                            prop.name.find("color") != std::string::npos) {
                            if (ImGui::ColorEdit3(hiddenName.c_str(), &ptr->x)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        } else {
                            if (ImGui::DragFloat3(hiddenName.c_str(), &ptr->x, 0.1f)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        }
                        CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
                        break;
                    }
                    case ComponentPropertyType::Float4: {
                        Irufemi::Vector4* ptr = reinterpret_cast<Irufemi::Vector4*>(prop.GetRawData());
                        if (prop.name.find("Color") != std::string::npos ||
                            prop.name.find("color") != std::string::npos) {
                            if (ImGui::ColorEdit4(hiddenName.c_str(), &ptr->x)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        } else {
                            if (ImGui::DragFloat4(hiddenName.c_str(), &ptr->x, 0.1f)) {
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                        }
                        CheckUndoRedoDrag(actionManager, ptr, prop.onChanged);
                        break;
                    }
                    case ComponentPropertyType::String: {
                        auto* str = static_cast<std::string*>(prop.GetRawData());
                        std::string lowerName = prop.name;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

                        bool isModel = (lowerName.find("model") != std::string::npos ||
                                        lowerName.find("mesh") != std::string::npos);
                        bool isTexture = (lowerName.find("texture") != std::string::npos ||
                                          lowerName.find("image") != std::string::npos);
                        bool isAnimation = (lowerName.find("animation") != std::string::npos ||
                                            lowerName.find("anim") != std::string::npos);

                        std::vector<std::string> comboItems;
                        IrufemiEngine* engine = nullptr;
                        if (component->GetGameObject() && component->GetGameObject()->GetScene()) {
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
                        } else {
                            char buffer[256];
                            strncpy_s(buffer, sizeof(buffer), str->c_str(), _TRUNCATE);

                            bool isPrefabProp = (lowerName.find("prefab") != std::string::npos);
                            if (isPrefabProp) {
                                ImGui::SetNextItemWidth((std::max)(50.0f, ImGui::GetContentRegionAvail().x - 70.0f));
                            }

                            static std::string startStr;
                            if (ImGui::InputText(hiddenName.c_str(), buffer, sizeof(buffer))) {
                                *str = buffer;
                                if (prop.onChanged) {
                                    prop.onChanged();
                                }
                            }
                            if (ImGui::IsItemActivated()) {
                                startStr = *str;
                            }
                            if (ImGui::IsItemDeactivatedAfterEdit()) {
                                std::string endStr = *str;
                                auto cb = prop.onChanged;
                                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                                    startStr, endStr, [str, cb](const std::string& v) {
                                        *str = v;
                                        if (cb) {
                                            cb();
                                        }
                                    }));
                            }

                            if (isPrefabProp && !str->empty()) {
                                ImGui::SameLine();
                                if (ImGui::Button((std::string(ICON_FA_WRENCH " Open##") + prop.name).c_str(),
                                                  ImVec2(65.0f, 0))) {
                                    if (auto em = EditorManager::GetInstance()) {
                                        em->EnterPrefabMode(*str);
                                    }
                                }
                                if (ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("Open in Prefab Edit Mode");
                                }
                            }
                        }

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                                const char* path = (const char*)payload->Data;
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
                        break;
                    }
                    case ComponentPropertyType::GameObjectRef: {
                        uint64_t* ptr = static_cast<uint64_t*>(prop.GetRawData());
                        std::vector<std::shared_ptr<GameObject>> allObjs;
                        if (component->GetGameObject() && component->GetGameObject()->GetScene()) {
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
                        if (*ptr != 0 && component->GetGameObject() && component->GetGameObject()->GetScene()) {
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
                        break;
                    }
                    default:
                        break;
                    }
                    ImGui::PopItemWidth();

                    ImGui::TableSetColumnIndex(2);
                    drawResetButton();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (component->GetComponentName() == "SplineComponent") {
            ImGui::Spacing();
            auto* spline = static_cast<SplineComponent*>(component);
            if (ImGui::Button("Add Waypoint at End", ImVec2(-1, 0))) {
                auto waypoints = spline->GetWaypoints();
                Irufemi::Vector3 newPos = {0.0f, 15.0f, 0.0f};
                if (waypoints.size() >= 2) {
                    const auto& p1 = waypoints[waypoints.size() - 2];
                    const auto& p2 = waypoints[waypoints.size() - 1];
                    Irufemi::Vector3 dir = {p2.x - p1.x, p2.y - p1.y, p2.z - p1.z};
                    newPos = {p2.x + dir.x, p2.y + dir.y, p2.z + dir.z};
                } else if (waypoints.size() == 1) {
                    newPos = {waypoints[0].x, waypoints[0].y, waypoints[0].z + 20.0f};
                }
                waypoints.push_back(newPos);
                spline->SetWaypoints(waypoints);
            }
        }
    }
    ImGui::PopID();
}
bool ComponentUIHelpers::BeginPropertyTable(const char* tableId) {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable(tableId, 3, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        return true;
    }
    return false;
}

void ComponentUIHelpers::EndPropertyTable() {
    ImGui::EndTable();
}

void ComponentUIHelpers::DrawPropertyLabel(const char* label, const char* tooltip) {
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void ComponentUIHelpers::DrawPropertyResetButton(const char* id, bool isModified, std::function<void()> resetAction) {
    ImGui::TableSetColumnIndex(2);
    if (isModified) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        if (ImGui::Button((std::string(ICON_FA_ARROW_ROTATE_LEFT) + id).c_str(), ImVec2(20, 0))) {
            if (resetAction) {
                resetAction();
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to Default");
        }
    }
}
#endif // EditorMode
