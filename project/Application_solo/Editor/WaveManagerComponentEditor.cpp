#ifdef EditorMode
#include "Editor/WaveManagerComponentEditor.h"
#include "Level/WaveManagerComponent.h"
#include "Commands/EditorCommands.h"
#include "Commands/EditorActionManager.h"
#include <imgui/imgui.h>
#include <string>
#include <algorithm>

void WaveManagerComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto waveManager = dynamic_cast<WaveManagerComponent*>(component);
    if (!waveManager) {
        return;
    }

    auto events = waveManager->GetAllEvents();

    // --- Undo/Redo Setup ---
    auto pushUndo = [&](const std::vector<WaveEventData>& oldData) {
        if (!actionManager) {
            return;
        }
        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::vector<WaveEventData>>>(
            oldData, events, [waveManager](const std::vector<WaveEventData>& val) { waveManager->SetAllEvents(val); }));
    };

    auto handleItemUndo = [&]() {
        if (ImGui::IsItemActivated()) {
            oldState_ = events;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            pushUndo(oldState_);
        }
    };
    // -----------------------

    ImGui::Text("Graphical WaveManager Editor");
    ImGui::Separator();

    std::string path = waveManager->GetLevelDataPath();
    char buffer[1024];
    strncpy_s(buffer, sizeof(buffer), path.c_str(), _TRUNCATE);
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText("Level Data Path", buffer, sizeof(buffer))) {
        waveManager->SetLevelDataPath(buffer);
    }

    if (ImGui::Button("Reload from JSON", ImVec2(150, 0))) {
        waveManager->ReloadLevelData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save to JSON##Top", ImVec2(150, 0))) {
        std::sort(events.begin(), events.end(),
                  [](const WaveEventData& a, const WaveEventData& b) { return a.triggerDistance < b.triggerDistance; });
        waveManager->SaveLevelData();
    }

    ImGui::Separator();

    int selectedEventIndex = waveManager->GetSelectedEventIndex();

    // Timeline drawing
    ImGui::Text("Event Timeline (Distance)");
    float timelineHeight = 100.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = timelineHeight;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(30, 30, 30, 255));
    drawList->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(100, 100, 100, 255));

    float maxDist = 500.0f;
    for (const auto& ev : events) {
        if (ev.triggerDistance > maxDist) {
            maxDist = ev.triggerDistance + 100.0f;
        }
    }
    float scale = size.x / maxDist;

    // Grid lines
    for (float d = 0; d <= maxDist; d += 100.0f) {
        float dx = p.x + d * scale;
        drawList->AddLine(ImVec2(dx, p.y), ImVec2(dx, p.y + size.y), IM_COL32(60, 60, 60, 255));

        char distLabel[32];
        snprintf(distLabel, sizeof(distLabel), "%.0f", d);
        drawList->AddText(ImVec2(dx + 2, p.y + 2), IM_COL32(150, 150, 150, 255), distLabel);
    }

    // Capture input for timeline
    ImGui::InvisibleButton("timeline_bg", size);
    bool isTimelineHovered = ImGui::IsItemHovered();
    bool isTimelineActive = ImGui::IsItemActive();
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isTimelineHovered) {
        oldState_ = events; // Capture state before dragging
        isDraggingModified_ = false;
        int hitIndex = -1;
        // Search backwards to hit topmost (if overlapped)
        for (int i = (int)events.size() - 1; i >= 0; --i) {
            float x = p.x + events[i].triggerDistance * scale;
            if (mousePos.x >= x - 6.0f && mousePos.x <= x + 6.0f && mousePos.y >= p.y &&
                mousePos.y <= p.y + timelineHeight) {
                hitIndex = i;
                break;
            }
        }
        if (hitIndex != -1) {
            selectedEventIndex = hitIndex;
            draggingNodeIndex_ = hitIndex;
            waveManager->SetSelectedEventIndex(hitIndex);
        } else {
            float newDist = (mousePos.x - p.x) / scale;
            waveManager->SetEditorPreviewDistance((std::max)(0.0f, newDist));
            selectedEventIndex = -1;
            waveManager->SetSelectedEventIndex(-1);
        }
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (draggingNodeIndex_ != -1 && draggingNodeIndex_ < events.size()) {
            if (ImGui::GetIO().MouseDelta.x != 0.0f) {
                events[draggingNodeIndex_].triggerDistance += ImGui::GetIO().MouseDelta.x / scale;
                if (events[draggingNodeIndex_].triggerDistance < 0) {
                    events[draggingNodeIndex_].triggerDistance = 0.0f;
                }
                isDraggingModified_ = true;
            }
        } else if (isTimelineActive) {
            float newDist = (mousePos.x - p.x) / scale;
            waveManager->SetEditorPreviewDistance((std::max)(0.0f, newDist));
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (draggingNodeIndex_ != -1 && isDraggingModified_) {
            pushUndo(oldState_);
        }
        draggingNodeIndex_ = -1;
        isDraggingModified_ = false;
    }

    // Draw Event Nodes
    for (int i = 0; i < (int)events.size(); ++i) {
        float x = p.x + events[i].triggerDistance * scale;
        float yCenter = p.y + timelineHeight / 2.0f;
        ImVec2 nodeMin(x - 5, yCenter - 12);
        ImVec2 nodeMax(x + 5, yCenter + 12);

        ImU32 color = IM_COL32(100, 150, 255, 255);
        if (events[i].eventType == "SpawnBoss") {
            color = IM_COL32(255, 50, 50, 255);
        }
        if (events[i].eventType == "SpawnDebris") {
            color = IM_COL32(50, 200, 50, 255);
        }
        if (i == selectedEventIndex) {
            color = IM_COL32(255, 200, 0, 255);
        }

        drawList->AddRectFilled(nodeMin, nodeMax, color);
        drawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 255, 255));
    }

    // Draw Playhead
    float previewDist = waveManager->GetEditorPreviewDistance();
    float phX = p.x + previewDist * scale;
    drawList->AddLine(ImVec2(phX, p.y), ImVec2(phX, p.y + size.y), IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::Text("Preview Distance: %.1f", previewDist);

    ImGui::Spacing();

    // Tool buttons
    if (ImGui::Button("Add Event")) {
        auto preEdit = events;
        WaveEventData newEvent;
        newEvent.triggerDistance = waveManager->GetEditorPreviewDistance();
        newEvent.eventType = "SpawnEnemy";
        events.push_back(newEvent);
        selectedEventIndex = (int)events.size() - 1;
        waveManager->SetSelectedEventIndex(selectedEventIndex);
        pushUndo(preEdit);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected") && selectedEventIndex >= 0 && selectedEventIndex < events.size()) {
        auto preEdit = events;
        events.erase(events.begin() + selectedEventIndex);
        selectedEventIndex = -1;
        waveManager->SetSelectedEventIndex(-1);
        pushUndo(preEdit);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save to JSON##Bottom")) {
        // 距離でソートしてから保存する
        std::sort(events.begin(), events.end(),
                  [](const WaveEventData& a, const WaveEventData& b) { return a.triggerDistance < b.triggerDistance; });
        waveManager->SaveLevelData();
    }

    // Event Properties Editor
    if (selectedEventIndex >= 0 && selectedEventIndex < events.size()) {
        ImGui::Separator();
        ImGui::Text("Event Properties (Index: %d)", selectedEventIndex);
        auto& ev = events[selectedEventIndex];

        ImGui::InputFloat("Trigger Distance", &ev.triggerDistance);
        handleItemUndo();

        char typeBuf[64];
        strncpy_s(typeBuf, sizeof(typeBuf), ev.eventType.c_str(), _TRUNCATE);
        typeBuf[sizeof(typeBuf) - 1] = '\0';
        if (ImGui::InputText("Event Type", typeBuf, sizeof(typeBuf))) {
            ev.eventType = typeBuf;
        }
        handleItemUndo();

        ImGui::Text("Parameters (JSON):");

        auto editStringParam = [&](const char* key) {
            bool hasKey = ev.parameters.contains(key);
            std::string val = hasKey ? ev.parameters[key].get<std::string>() : "";
            char buf[128];
            strncpy_s(buf, sizeof(buf), val.c_str(), _TRUNCATE);

            if (ImGui::InputText(key, buf, sizeof(buf))) {
                ev.parameters[key] = std::string(buf);
            }
            handleItemUndo();
            ImGui::SameLine();
            if (hasKey) {
                if (ImGui::Button((std::string("Remove##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters.erase(key);
                    pushUndo(preEdit);
                }
            } else {
                if (ImGui::Button((std::string("Add##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters[key] = "";
                    pushUndo(preEdit);
                }
            }
        };

        auto editIntParam = [&](const char* key) {
            bool hasKey = ev.parameters.contains(key);
            int val = hasKey ? ev.parameters[key].get<int>() : 0;
            if (ImGui::InputInt(key, &val)) {
                ev.parameters[key] = val;
            }
            handleItemUndo();
            ImGui::SameLine();
            if (hasKey) {
                if (ImGui::Button((std::string("Remove##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters.erase(key);
                    pushUndo(preEdit);
                }
            } else {
                if (ImGui::Button((std::string("Add##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters[key] = 0;
                    pushUndo(preEdit);
                }
            }
        };

        auto editFloatParam = [&](const char* key, float defaultVal, float speed, float minVal, float maxVal) {
            bool hasKey = ev.parameters.contains(key);
            float val = hasKey ? ev.parameters[key].get<float>() : defaultVal;
            if (ImGui::DragFloat(key, &val, speed, minVal, maxVal, "%.2f")) {
                ev.parameters[key] = val;
            }
            handleItemUndo();
            ImGui::SameLine();
            if (hasKey) {
                if (ImGui::Button((std::string("Remove##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters.erase(key);
                    pushUndo(preEdit);
                }
            } else {
                if (ImGui::Button((std::string("Add##") + key).c_str())) {
                    auto preEdit = events;
                    ev.parameters[key] = defaultVal;
                    pushUndo(preEdit);
                }
            }
        };

        editStringParam("WaveId");
        editStringParam("BossID");
        editStringParam("Formation");
        editIntParam("Count");
        editFloatParam("Scale", 1.0f, 0.05f, 0.2f, 5.0f);
        editFloatParam("CombatDuration", 7.5f, 0.5f, 1.0f, 60.0f);
        editFloatParam("TargetDistance", 65.0f, 1.0f, 10.0f, 200.0f);

        bool hasOffset = ev.parameters.contains("OffsetFromRail");
        if (hasOffset) {
            float offset[3] = {0, 0, 0};
            auto& jOffset = ev.parameters["OffsetFromRail"];
            if (jOffset.contains("x")) {
                offset[0] = jOffset["x"].get<float>();
            }
            if (jOffset.contains("y")) {
                offset[1] = jOffset["y"].get<float>();
            }
            if (jOffset.contains("z")) {
                offset[2] = jOffset["z"].get<float>();
            }

            if (ImGui::DragFloat3("OffsetFromRail", offset, 0.1f)) {
                jOffset["x"] = offset[0];
                jOffset["y"] = offset[1];
                jOffset["z"] = offset[2];
            }
            handleItemUndo();
            ImGui::SameLine();
            if (ImGui::Button("Remove Offset")) {
                auto preEdit = events;
                ev.parameters.erase("OffsetFromRail");
                pushUndo(preEdit);
            }
        } else {
            if (ImGui::Button("Add OffsetFromRail")) {
                auto preEdit = events;
                ev.parameters["OffsetFromRail"] = {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}};
                pushUndo(preEdit);
            }
        }

        ImGui::Spacing();
        ImGui::Text("Raw JSON:");
        std::string rawJson = ev.parameters.dump(4);
        char jsonBuf[1024];
        strncpy_s(jsonBuf, sizeof(jsonBuf), rawJson.c_str(), _TRUNCATE);
        if (ImGui::InputTextMultiline("##RawJson", jsonBuf, sizeof(jsonBuf), ImVec2(-1, 100))) {
            try {
                ev.parameters = nlohmann::json::parse(jsonBuf);
            } catch (...) {
            }
        }
        handleItemUndo();
    }

    waveManager->SetAllEvents(events);
}
#endif
