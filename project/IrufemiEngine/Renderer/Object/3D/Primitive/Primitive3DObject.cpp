#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Camera/CameraManager.h"

#include <algorithm>

#include "Renderer/Object/PrimitiveManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"
#include "Framework/UI/DebugUI.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Math.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/Frustum.h"
#include "Core/Shape/Sphere.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace Irufemi;

// 静的メンバの初期化
Irufemi::TextureManager* Primitive3DObject::textureManager_ = nullptr;
Irufemi::DrawManager* Primitive3DObject::drawManager_ = nullptr;
DebugUI* Primitive3DObject::ui_ = nullptr;
IrufemiEngine* Primitive3DObject::engine_ = nullptr;

#include "Core/System/IrufemiEngine.h"

// --- Primitive3DObject ---

void Primitive3DObject::Initialize(Irufemi::PrimitiveType type, const std::string& texturePath) {
    auto* texM = GetTextureManagerInstance();

    // 形状の初期化
    mesh_.ChangeMesh(type);

    // マテリアルの初期化
    material_.texturePath = texturePath;
    if (texM) {
        // 現在のテクスチャ名からインデックスを復元（Debug UI用）
        auto textureNames = texM->GetTextureNamesForDebug();
        auto it = std::find(textureNames.begin(), textureNames.end(), texturePath);
        material_.selectedTextureIndex =
            (it != textureNames.end()) ? static_cast<int>(std::distance(textureNames.begin(), it)) : 0;
    }

    // デフォルトのマテリアル設定反映
    material_.UpdateMaterial(mesh_.resource.get(), texM);

    // 初回のトランスフォーム更新
    transform_.isDirty = true;
    Update();
}

void Primitive3DObject::ReinitializeMesh(const PrimitiveData& data) {
    mesh_.ChangeMesh(data);
    transform_.isDirty = true;
}

void Primitive3DObject::Update() {
    auto* engine = GetEngineInstance();
    auto* texM = GetTextureManagerInstance();
    if (!mesh_.resource || !engine) {
        return;
    }
    Camera* activeCam = engine->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // トランスフォームの同期（トランスフォームまたはカメラが動いた場合のみ実行）
    const Irufemi::Matrix4x4& currentViewProj = activeCam->GetViewProjectionMatrix3D();
    bool cameraChanged = (std::memcmp(&lastCameraViewProj_, &currentViewProj, sizeof(Irufemi::Matrix4x4)) != 0);

    if (transform_.isDirty) {
        transform_.UpdateTransform(mesh_.resource.get(), *activeCam);
        lastCameraViewProj_ = currentViewProj;
    } else if (cameraChanged) {
        mesh_.resource->UpdateTransform(*activeCam);
        lastCameraViewProj_ = currentViewProj;
    }

    // マテリアル情報の最新化
    material_.UpdateMaterial(mesh_.resource.get(), texM);
}

void Primitive3DObject::Draw() {
    auto* engine = GetEngineInstance();
    if (!engine) {
        return;
    }
    if (Camera* activeCam = engine->GetCameraManager()->GetActiveCamera()) {
        Draw(*activeCam, false);
    }
}

void Primitive3DObject::Draw(bool isUI) {
    auto* engine = GetEngineInstance();
    if (!engine) {
        return;
    }
    if (Camera* activeCam = engine->GetCameraManager()->GetActiveCamera()) {
        Draw(*activeCam, isUI);
    }
}

void Primitive3DObject::Draw(const Camera& camera) {
    Draw(camera, false);
}

void Primitive3DObject::Draw(const Camera& camera, bool isUI) {
    auto* drawM = GetDrawManagerInstance();
    if (!mesh_.resource || !drawM) {
        return;
    }

    // 視錐台カリング
    if (isCullingEnabled_) {
        // 形状に応じた基本半径（ユニットサイズ1.0想定）
        float baseRadius = 0.5f;
        switch (mesh_.type) {
        case Irufemi::PrimitiveType::Cube:
            baseRadius = 0.866f;
            break; // 1/2 * sqrt(3)
        case Irufemi::PrimitiveType::Cylinder:
        case Irufemi::PrimitiveType::Cone:
        case Irufemi::PrimitiveType::Plane:
        case Irufemi::PrimitiveType::Triangle:
        case Irufemi::PrimitiveType::Tetra:
            baseRadius = 0.707f;
            break; // 1/2 * sqrt(2)
        default:
            baseRadius = 0.500f;
            break;
        }

        // スケールを考慮した最終半径（異方性スケールの最大値を採用）
        float maxScale =
            (std::max)({transform_.transform.scale.x, transform_.transform.scale.y, transform_.transform.scale.z});
        float finalRadius = baseRadius * maxScale;

        Irufemi::Sphere boundingSphere;
        boundingSphere.center = transform_.transform.translate;
        boundingSphere.radius = finalRadius * 1.1f; // 10%のマージン

        if (!Irufemi::Collision::IsCollision(camera.GetFrustum(), boundingSphere)) {
            return; // 描画スキップ
        }
    }

    // 描画実行直前のバッファ同期（コールバックを発動させるため自身の SyncBeforeDraw を呼ぶ）
    SyncBeforeDraw();

    // 描画実行
    if (isUI) {
        drawM->SubmitUI3D(mesh_.resource.get(), nullptr);
    } else if (isTransparent_) {
        drawM->SubmitTransparent3D(mesh_.resource.get(), nullptr, castShadows_);
    } else {
        drawM->SubmitStandard3D(mesh_.resource.get(), nullptr, castShadows_);
    }
}

void Primitive3DObject::DrawOutlineMask() {
    auto* drawM = GetDrawManagerInstance();
    if (!mesh_.resource || !drawM) {
        return;
    }
    drawM->SubmitOutlineMask(mesh_.resource.get(), nullptr);
}

void Primitive3DObject::Debug(const char* label) {
#ifdef USE_IMGUI
    if (!ui_) {
        return;
    }

    ImGui::Begin(label);

    // --- Mesh Component ---
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* shapeNames[] = {"Triangle", "Plane",  "Cube", "Cylinder", "Sphere",    "Tetra", "Circle",
                                    "Ring",     "Skybox", "Cone", "Torus",    "IcoSphere", "Grid"};
        int currentType = static_cast<int>(mesh_.type);
        if (ImGui::Combo("Shape", &currentType, shapeNames, IM_ARRAYSIZE(shapeNames))) {
            SetShape(static_cast<Irufemi::PrimitiveType>(currentType));
        }
    }

    // --- Irufemi::Transform Component ---
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat3("Position", &transform_.transform.translate.x, 0.01f)) {
            transform_.isDirty = true;
        }
        if (ImGui::DragFloat3("Rotation", &transform_.transform.rotate.x, 0.01f)) {
            transform_.isDirty = true;
        }
        if (ImGui::DragFloat3("Scale", &transform_.transform.scale.x, 0.01f)) {
            transform_.isDirty = true;
        }
        ImGui::Checkbox("Frustum Culling", &isCullingEnabled_);
    }

    // --- Material Component ---
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4("Base Color", &material_.color.x);

        const char* lightingModes[] = {"None", "Lambert", "Half-Lambert", "PBR"};
        ImGui::Combo("Lighting Mode", &material_.lightingMode, lightingModes, IM_ARRAYSIZE(lightingModes));

        ImGui::Checkbox("Enable Lighting", &material_.enableLighting);

        if (material_.lightingMode == 3) { // PBR
            ImGui::SliderFloat("Metallic", &material_.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &material_.roughness, 0.0f, 1.0f);
        }

        // Texture 選択 (DebugUIの機能を利用)
        if (textureManager_) {
            ui_->DebugTexture(mesh_.resource.get(), material_.selectedTextureIndex);
            // 選択されたインデックスから名前を更新
            auto textureNames = textureManager_->GetTextureNamesForDebug();
            if (material_.selectedTextureIndex >= 0 &&
                material_.selectedTextureIndex < static_cast<int>(textureNames.size())) {
                material_.texturePath = textureNames[material_.selectedTextureIndex];
            }
        }
    }

    ImGui::End();
#endif
}

void Primitive3DObject::SyncBeforeDraw() {
    auto* engine = GetEngineInstance();
    if (customSyncCallback_ && engine) {
        uint32_t frameIndex = engine->GetDirectXCommon()->GetFrameIndex();
        customSyncCallback_(frameIndex);
    }
    if (mesh_.resource) {
        mesh_.resource->SyncBeforeDraw();
    }
}

Irufemi::Vector3 Primitive3DObject::GetRight() const {
    Irufemi::Matrix4x4 mat = Irufemi::Math::MakeRotateXYZMatrix(transform_.transform.rotate);
    return {mat.m[0][0], mat.m[0][1], mat.m[0][2]};
}

Irufemi::Vector3 Primitive3DObject::GetUp() const {
    Irufemi::Matrix4x4 mat = Irufemi::Math::MakeRotateXYZMatrix(transform_.transform.rotate);
    return {mat.m[1][0], mat.m[1][1], mat.m[1][2]};
}

Irufemi::Vector3 Primitive3DObject::GetDirection() const {
    Irufemi::Matrix4x4 mat = Irufemi::Math::MakeRotateXYZMatrix(transform_.transform.rotate);
    return {mat.m[2][0], mat.m[2][1], mat.m[2][2]};
}
