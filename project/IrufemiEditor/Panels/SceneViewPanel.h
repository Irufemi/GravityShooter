#pragma once

#ifdef EditorMode
#include "Core/IEditorPanel.h"
#include "Core/Math/Vector3.h"
#include <imgui.h>
#include "imgui/ImGuizmo.h"
#include "Renderer/Camera/OrbitCameraController.h"

#include "Core/EditorManager.h"

/**
 * @class SceneViewPanel
 * @brief エディタのSceneビューを描画するパネル
 */
class SceneViewPanel : public IEditorPanel {
public:
    void Initialize(EditorManager* editorManager) override;
    void Draw() override;
    const char* GetName() const override {
        return "Scene";
    }

private:
    EditorManager* editorManager_ = nullptr;
    OrbitCameraController cameraController_;
    EditorModeState lastMode_ = EditorModeState::Edit;

    // --- ギズモ用状態 ---
    ImGuizmo::OPERATION currentGizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode_ = ImGuizmo::LOCAL;

    // --- Undo/Redo用の状態保存 ---
    bool wasUsingGizmo_ = false;
    Irufemi::Vector3 gizmoStartPos_;
    Irufemi::Vector3 gizmoStartRot_;
    Irufemi::Vector3 gizmoStartScale_;
    Irufemi::Vector3 gizmoStartColliderOffset_;
    Irufemi::Vector3 gizmoStartColliderSize_;

    // --- 内部ヘルパーメソッド ---
    void DrawToolbar(ImVec2 minPos, ImVec2 maxPos);
    void DrawImGuizmo(ImVec2 minPos, ImVec2 size);
    void HandleDragAndDrop(ImVec2 minPos, ImVec2 size);
    void HandlePicking(ImVec2 mousePos, ImVec2 minPos, ImVec2 maxPos, ImVec2 size);
    void SnapToGameCamera();
};

#endif // EditorMode
