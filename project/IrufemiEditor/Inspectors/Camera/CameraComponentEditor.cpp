#include "Inspectors/Camera/CameraComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/Math/MathFunction.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Object/Line/LineClass.h"
#include "UI/ComponentUIHelpers.h"
#include <cmath>

using namespace Irufemi;

CameraComponentEditor::CameraComponentEditor() {
    debugLineBatch_ = std::make_unique<Line3DBatch>();
    debugLineBatch_->Initialize();
}

CameraComponentEditor::~CameraComponentEditor() = default;

void CameraComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* cameraComp = dynamic_cast<CameraComponent*>(component);
    if (!cameraComp) {
        return;
    }

    // FOV (Rad to Deg)
    float fovRad = cameraComp->GetFovAngleY();
    float fovDeg = fovRad * 180.0f / Irufemi::Math::PI;

    if (ImGui::SliderFloat("FOV (Degree)", &fovDeg, 10.0f, 170.0f)) {
        cameraComp->SetFovAngleY(fovDeg * Irufemi::Math::PI / 180.0f);
    }
    ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &fovDeg, [cameraComp](float val) {
        cameraComp->SetFovAngleY(val * Irufemi::Math::PI / 180.0f);
    });

    // Near Z
    float nearZ = cameraComp->GetNearZ();
    if (ImGui::DragFloat("Near Z", &nearZ, 0.1f, 0.01f, 1000.0f)) {
        cameraComp->SetNearZ(nearZ);
    }
    ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &nearZ,
                                          [cameraComp](float val) { cameraComp->SetNearZ(val); });

    // Far Z
    float farZ = cameraComp->GetFarZ();
    if (ImGui::DragFloat("Far Z", &farZ, 1.0f, nearZ + 0.1f, 10000.0f)) {
        cameraComp->SetFarZ(farZ);
    }
    ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &farZ,
                                          [cameraComp](float val) { cameraComp->SetFarZ(val); });

    // --- Frustum Gizmo Draw ---
    auto transform = cameraComp->GetTransform();
    if (transform) {
        auto* engine = cameraComp->GetEngine();
        float aspect = 16.0f / 9.0f;
        if (engine) {
            float w = static_cast<float>(engine->GetGameResolutionWidth());
            float h = static_cast<float>(engine->GetGameResolutionHeight());
            if (h > 0) {
                aspect = w / h;
            }
        }

        float fovHalf = cameraComp->GetFovAngleY() * 0.5f;
        float tanFov = std::tan(fovHalf);

        float nearH = tanFov * nearZ;
        float nearW = nearH * aspect;

        float farH = tanFov * farZ;
        float farW = farH * aspect;

        // ローカル座標の8頂点
        Irufemi::Vector3 ln[4] = {
            {nearW, nearH, nearZ}, {-nearW, nearH, nearZ}, {-nearW, -nearH, nearZ}, {nearW, -nearH, nearZ}};
        Irufemi::Vector3 lf[4] = {{farW, farH, farZ}, {-farW, farH, farZ}, {-farW, -farH, farZ}, {farW, -farH, farZ}};

        // ワールド座標へ変換
        Irufemi::Matrix4x4 worldMat = transform->GetWorldMatrix();
        Irufemi::Vector3 wn[4];
        Irufemi::Vector3 wf[4];
        for (int i = 0; i < 4; ++i) {
            wn[i] = Irufemi::Math::Transform(ln[i], worldMat);
            wf[i] = Irufemi::Math::Transform(lf[i], worldMat);
        }

        // ラインの追加
        debugLineBatch_->ClearInstances();
        Irufemi::Vector4 color = {0.0f, 1.0f, 1.0f, 1.0f}; // シアン

        for (int i = 0; i < 4; ++i) {
            int next = (i + 1) % 4;
            // Near面の四角形
            debugLineBatch_->AddInstance(wn[i], wn[next], color);
            // Far面の四角形
            debugLineBatch_->AddInstance(wf[i], wf[next], color);
            // Near面からFar面への線
            debugLineBatch_->AddInstance(wn[i], wf[i], color);
        }

        debugLineBatch_->BuildInstanceBuffer(true);
        debugLineBatch_->Draw();
    }
}
#endif // EditorMode
