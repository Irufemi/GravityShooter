#include "Panels/SceneViewPanel.h"
#include "Renderer/Camera/CameraManager.h"
#include "Framework/Scene/SceneManager.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Core/EditorManager.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/RenderTexture.h"
// #include "Editor/Utils/EditorCameraController.h" (Removed during refactoring)
#include "Physics/CollisionManager.h"
#include "Commands/EditorActionManager.h"
#include "UI/EditorDragDrop.h"
#include "Commands/EditorCommands.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include "Core/Math/MathFunction.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/TargetFollowComponent.h"
#include "Physics/Collision/Collision.h"
#include "Platform/Input/InputManager.h"
#include "Platform/Input/Mouse.h"
#include "Platform/Input/Keyboard.h"
#include <algorithm>

void SceneViewPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void SceneViewPanel::Draw() {
    if (!editorManager_) {
        return;
    }

    ImGui::Begin(GetName(), &isOpen_);

    auto* engine = editorManager_->GetEngine();
    if (engine && engine->GetMainRenderTexture()) {
        auto mainTexture = engine->GetMainRenderTexture();

        float targetWidth = 1280.0f;
        float targetHeight = 720.0f;

        // パネルの大きさを取得して画像をフィットさせる（16:9を維持する）
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float aspect = targetWidth / targetHeight;
        ImVec2 size;
        if (avail.x / avail.y > aspect) {
            size.y = avail.y;
            size.x = size.y * aspect;
        } else {
            size.x = avail.x;
            size.y = size.x / aspect;
        }

        // 中央揃えにするためのカーソル位置調整
        ImVec2 cursor = ImGui::GetCursorPos();
        cursor.x += (avail.x - size.x) * 0.5f;
        cursor.y += (avail.y - size.y) * 0.5f;
        ImGui::SetCursorPos(cursor);

        ImGui::Image((ImTextureID)mainTexture->GetImGuiSrvHandleGPU().ptr, size);

        ImVec2 minPos = ImGui::GetItemRectMin(); // ImGui::Image() の左上
        ImVec2 maxPos = ImGui::GetItemRectMax(); // ImGui::Image() の右下

        // --- 選択中のSpriteに対するアウトライン（強調枠）描画 ---
        if (auto selectedObj = editorManager_->GetSelectedObject()) {
            // ビューポート領域でクリッピングを行い、アウトラインが画面外（他のパネル等）にはみ出さないようにする
            ImGui::GetWindowDrawList()->PushClipRect(minPos, maxPos, true);

            if (auto spriteComp = selectedObj->GetComponent<SpriteRendererComponent>()) {
                if (auto transform = selectedObj->GetTransform()) {
                    auto sprite = spriteComp->GetSprite();
                    if (sprite) {
                        Irufemi::Vector2 sizeScaled = sprite->GetSize();
                        Irufemi::Vector2 anchor = sprite->GetAnchor();
                        Irufemi::Vector3 pos = transform->GetWorldPosition();

                        float left = pos.x - sizeScaled.x * anchor.x;
                        float top = pos.y - sizeScaled.y * anchor.y;
                        float right = pos.x + sizeScaled.x * (1.0f - anchor.x);
                        float bottom = pos.y + sizeScaled.y * (1.0f - anchor.y);

                        float scaleX = size.x / targetWidth;
                        float scaleY = size.y / targetHeight;

                        ImVec2 pMin = ImVec2(minPos.x + left * scaleX, minPos.y + top * scaleY);
                        ImVec2 pMax = ImVec2(minPos.x + right * scaleX, minPos.y + bottom * scaleY);

                        ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(255, 165, 0, 255), 0.0f, 0, 2.0f);
                    }
                }
            } else if (auto textComp = selectedObj->GetComponent<TextRendererComponent>()) {
                if (auto transform = selectedObj->GetTransform()) {
                    Irufemi::Vector3 pos = transform->GetWorldPosition();
                    Irufemi::Vector2 minBounds = textComp->GetLocalBoundsMin();
                    Irufemi::Vector2 maxBounds = textComp->GetLocalBoundsMax();

                    float left = pos.x + minBounds.x * transform->GetWorldScale().x;
                    float right = pos.x + maxBounds.x * transform->GetWorldScale().x;
                    float top = pos.y + minBounds.y * transform->GetWorldScale().y;
                    float bottom = pos.y + maxBounds.y * transform->GetWorldScale().y;

                    float scaleX = size.x / targetWidth;
                    float scaleY = size.y / targetHeight;

                    ImVec2 pMin = ImVec2(minPos.x + left * scaleX, minPos.y + top * scaleY);
                    ImVec2 pMax = ImVec2(minPos.x + right * scaleX, minPos.y + bottom * scaleY);

                    ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                }
            }

            ImGui::GetWindowDrawList()->PopClipRect();
        }

        DrawImGuizmo(minPos, size);
        HandleDragAndDrop(minPos, size);

        // --- UI用の仮想マウス座標更新 & クリックによる3Dピッキング ---
        if (ImGui::IsWindowHovered()) {
            ImVec2 mousePos = ImGui::GetMousePos(); // 画面全体の座標

            if (mousePos.x >= minPos.x && mousePos.x <= maxPos.x && mousePos.y >= minPos.y && mousePos.y <= maxPos.y) {

                Irufemi::Vector2 localMousePos = {mousePos.x - minPos.x, mousePos.y - minPos.y};
                float scaleX = targetWidth / size.x;
                float scaleY = targetHeight / size.y;
                Irufemi::Vector2 scaledVirtualPos = {localMousePos.x * scaleX, localMousePos.y * scaleY};

                engine->GetInputManager()->SetVirtualMousePosition(scaledVirtualPos, true);

                if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                    cameraController_.UpdateCameraInput(camera, engine->GetInputManager());
                }

                HandlePicking(mousePos, minPos, maxPos, size);

                // Fキーによるフォーカス機能
                if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                    if (auto selectedObj = editorManager_->GetSelectedObject()) {
                        if (auto transform = selectedObj->GetTransform()) {
                            if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                                cameraController_.Focus(camera, transform->GetWorldPosition());
                            }
                        }
                    }
                }

                // Ctrl + Shift + F または P キーによる GameCamera スナップ機能
                if ((ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F)) ||
                    ImGui::IsKeyPressed(ImGuiKey_P)) {
                    SnapToGameCamera();
                }
            } else {
                engine->GetInputManager()->SetVirtualMousePosition({0.0f, 0.0f}, false);
            }
        } else {
            if (engine && engine->GetInputManager()) {
                engine->GetInputManager()->SetVirtualMousePosition({0.0f, 0.0f}, false);
            }
        }

        // --- オーバーレイUI（SceneViewの右上） ---
        DrawToolbar(minPos, maxPos);
    }

    ImGui::End();
}

void SceneViewPanel::DrawToolbar(ImVec2 minPos, ImVec2 maxPos) {
    auto* engine = editorManager_->GetEngine();

    ImVec2 overlayPos = ImVec2(maxPos.x - 365.0f, minPos.y + 10.0f);
    ImGui::SetCursorScreenPos(overlayPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    if (ImGui::BeginChild("SceneToolbar", ImVec2(355.0f, 40.0f), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

        // Tools
        bool isTranslate = currentGizmoOperation_ == ImGuizmo::TRANSLATE;
        bool isRotate = currentGizmoOperation_ == ImGuizmo::ROTATE;
        bool isScale = currentGizmoOperation_ == ImGuizmo::SCALE;
        bool isBounds = currentGizmoOperation_ == ImGuizmo::BOUNDS;

        auto DrawToolBtn = [](const char* icon, bool selected, ImGuizmo::OPERATION op, ImGuizmo::OPERATION& currentOp,
                              const char* tooltip) {
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }
            if (ImGui::Button(icon, ImVec2(24, 24))) {
                currentOp = op;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
        };

        DrawToolBtn(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, isTranslate, ImGuizmo::TRANSLATE, currentGizmoOperation_,
                    "Translate");
        ImGui::SameLine();
        DrawToolBtn(ICON_FA_ROTATE, isRotate, ImGuizmo::ROTATE, currentGizmoOperation_, "Rotate");
        ImGui::SameLine();
        DrawToolBtn(ICON_FA_EXPAND, isScale, ImGuizmo::SCALE, currentGizmoOperation_, "Scale");
        ImGui::SameLine();
        DrawToolBtn(ICON_FA_VECTOR_SQUARE, isBounds, ImGuizmo::BOUNDS, currentGizmoOperation_, "Bounds");

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Local / World
        const char* modeText = currentGizmoMode_ == ImGuizmo::LOCAL ? "Local" : "World";
        if (ImGui::Button(modeText, ImVec2(50, 24))) {
            currentGizmoMode_ = (currentGizmoMode_ == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Collider Draw
        bool* drawCollider = engine->GetCollisionManager()->GetIsDrawDebugLinePtr();
        if (drawCollider) {
            bool isActive = *drawCollider;
            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }

            if (ImGui::Button("Col", ImVec2(35, 24))) {
                *drawCollider = !isActive;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle Collider Debug Draw");
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Game Camera スナップボタン
        if (ImGui::Button(ICON_FA_VIDEO " Cam", ImVec2(54, 24))) {
            SnapToGameCamera();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Align SceneView to Game Camera\n(Snap to Play start position & rotation) [P]");
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void SceneViewPanel::DrawImGuizmo(ImVec2 minPos, ImVec2 size) {
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(minPos.x, minPos.y, size.x, size.y);

    auto* engine = editorManager_->GetEngine();
    if (auto selectedObj = editorManager_->GetSelectedObject()) {
        // ロックされている、またはフォルダの場合はギズモを非表示・操作不可にする
        if (selectedObj->GetIsLocked() || selectedObj->GetIsFolder()) {
            return;
        }

        if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
            Irufemi::Matrix4x4 view = camera->GetViewMatrix();
            Irufemi::Matrix4x4 proj = camera->GetPerspectiveFovMatrix();

            if (auto transform = selectedObj->GetComponent<TransformComponent>()) {
                Irufemi::Matrix4x4 world = transform->GetWorldMatrix();
                bool manipulated = false;

                bool isUsingGizmo = ImGuizmo::IsUsing();

                if (isUsingGizmo && !wasUsingGizmo_) {
                    gizmoStartPos_ = transform->GetPosition();
                    gizmoStartRot_ = transform->GetRotation();
                    gizmoStartScale_ = transform->GetScale();

                    if (currentGizmoOperation_ == ImGuizmo::BOUNDS) {
                        if (auto aabbCol = selectedObj->GetComponent<AABBColliderComponent>()) {
                            gizmoStartColliderOffset_ = aabbCol->GetLocalOffset();
                            gizmoStartColliderSize_ = aabbCol->GetLocalSize();
                        } else if (auto obbCol = selectedObj->GetComponent<OBBColliderComponent>()) {
                            gizmoStartColliderOffset_ = obbCol->GetLocalOffset();
                            gizmoStartColliderSize_ = obbCol->GetLocalSize();
                        }
                    }
                }

                if (currentGizmoOperation_ == ImGuizmo::BOUNDS) {
                    // コライダーのリサイズ操作
                    if (auto aabbCol = selectedObj->GetComponent<AABBColliderComponent>()) {
                        Irufemi::Vector3 offset = aabbCol->GetLocalOffset();
                        Irufemi::Vector3 csize = aabbCol->GetLocalSize();
                        float bounds[6] = {offset.x - csize.x, offset.y - csize.y, offset.z - csize.z,
                                           offset.x + csize.x, offset.y + csize.y, offset.z + csize.z};
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_,
                                                 currentGizmoMode_, &world.m[0][0], nullptr, nullptr, bounds)) {
                            aabbCol->SetLocalOffset({(bounds[0] + bounds[3]) * 0.5f, (bounds[1] + bounds[4]) * 0.5f,
                                                     (bounds[2] + bounds[5]) * 0.5f});
                            aabbCol->SetLocalSize({(bounds[3] - bounds[0]) * 0.5f, (bounds[4] - bounds[1]) * 0.5f,
                                                   (bounds[5] - bounds[2]) * 0.5f});
                        }
                    } else if (auto obbCol = selectedObj->GetComponent<OBBColliderComponent>()) {
                        Irufemi::Vector3 offset = obbCol->GetLocalOffset();
                        Irufemi::Vector3 csize = obbCol->GetLocalSize();
                        float bounds[6] = {offset.x - csize.x, offset.y - csize.y, offset.z - csize.z,
                                           offset.x + csize.x, offset.y + csize.y, offset.z + csize.z};
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_,
                                                 currentGizmoMode_, &world.m[0][0], nullptr, nullptr, bounds)) {
                            obbCol->SetLocalOffset({(bounds[0] + bounds[3]) * 0.5f, (bounds[1] + bounds[4]) * 0.5f,
                                                    (bounds[2] + bounds[5]) * 0.5f});
                            obbCol->SetLocalSize({(bounds[3] - bounds[0]) * 0.5f, (bounds[4] - bounds[1]) * 0.5f,
                                                  (bounds[5] - bounds[2]) * 0.5f});
                        }
                    } else if (auto sphereCol = selectedObj->GetComponent<SphereColliderComponent>()) {
                        ImGuizmo::OPERATION op = ImGuizmo::SCALE;
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], op, currentGizmoMode_, &world.m[0][0])) {
                            manipulated = true;
                        }
                    } else {
                        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], op, currentGizmoMode_, &world.m[0][0])) {
                            manipulated = true;
                        }
                    }
                } else {
                    if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_, currentGizmoMode_,
                                             &world.m[0][0])) {
                        manipulated = true;
                    }
                }

                if (!isUsingGizmo && wasUsingGizmo_) {
                    auto IsNotEqual = [](const Irufemi::Vector3& a, const Irufemi::Vector3& b) {
                        return a.x != b.x || a.y != b.y || a.z != b.z;
                    };
                    auto actionManager = editorManager_->GetActionManager();
                    if (actionManager) {
                        if (currentGizmoOperation_ == ImGuizmo::BOUNDS) {
                            if (auto aabbCol = selectedObj->GetComponent<AABBColliderComponent>()) {
                                Irufemi::Vector3 endOffset = aabbCol->GetLocalOffset();
                                Irufemi::Vector3 endSize = aabbCol->GetLocalSize();
                                using BoundsPair = std::pair<Irufemi::Vector3, Irufemi::Vector3>;
                                if (IsNotEqual(endOffset, gizmoStartColliderOffset_) ||
                                    IsNotEqual(endSize, gizmoStartColliderSize_)) {
                                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<BoundsPair>>(
                                        BoundsPair(gizmoStartColliderOffset_, gizmoStartColliderSize_),
                                        BoundsPair(endOffset, endSize), [aabbCol](const BoundsPair& v) {
                                            aabbCol->SetLocalOffset(v.first);
                                            aabbCol->SetLocalSize(v.second);
                                        }));
                                }
                            } else if (auto obbCol = selectedObj->GetComponent<OBBColliderComponent>()) {
                                Irufemi::Vector3 endOffset = obbCol->GetLocalOffset();
                                Irufemi::Vector3 endSize = obbCol->GetLocalSize();
                                using BoundsPair = std::pair<Irufemi::Vector3, Irufemi::Vector3>;
                                if (IsNotEqual(endOffset, gizmoStartColliderOffset_) ||
                                    IsNotEqual(endSize, gizmoStartColliderSize_)) {
                                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<BoundsPair>>(
                                        BoundsPair(gizmoStartColliderOffset_, gizmoStartColliderSize_),
                                        BoundsPair(endOffset, endSize), [obbCol](const BoundsPair& v) {
                                            obbCol->SetLocalOffset(v.first);
                                            obbCol->SetLocalSize(v.second);
                                        }));
                                }
                            } else {
                                Irufemi::Vector3 endScale = transform->GetScale();
                                if (IsNotEqual(endScale, gizmoStartScale_)) {
                                    actionManager->PushAndExecute(
                                        std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                                            gizmoStartScale_, endScale,
                                            [transform](const Irufemi::Vector3& v) { transform->SetScale(v); }));
                                }
                            }
                        } else {
                            Irufemi::Vector3 endPos = transform->GetPosition();
                            Irufemi::Vector3 endRot = transform->GetRotation();
                            Irufemi::Vector3 endScale = transform->GetScale();
                            if (IsNotEqual(endPos, gizmoStartPos_)) {
                                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                                    gizmoStartPos_, endPos,
                                    [transform](const Irufemi::Vector3& v) { transform->SetPosition(v); }));
                            }
                            if (IsNotEqual(endRot, gizmoStartRot_)) {
                                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                                    gizmoStartRot_, endRot,
                                    [transform](const Irufemi::Vector3& v) { transform->SetRotation(v); }));
                            }
                            if (IsNotEqual(endScale, gizmoStartScale_)) {
                                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                                    gizmoStartScale_, endScale,
                                    [transform](const Irufemi::Vector3& v) { transform->SetScale(v); }));
                            }
                        }
                    }
                }
                wasUsingGizmo_ = isUsingGizmo;

                if (manipulated) {
                    Irufemi::Vector3 pos, rot, mscale;
                    ImGuizmo::DecomposeMatrixToComponents(&world.m[0][0], &pos.x, &rot.x, &mscale.x);

                    rot.x = rot.x * Irufemi::Math::PI / 180.0f;
                    rot.y = rot.y * Irufemi::Math::PI / 180.0f;
                    rot.z = rot.z * Irufemi::Math::PI / 180.0f;

                    transform->SetPosition(pos);
                    transform->SetRotation(rot);
                    transform->SetScale(mscale);
                }
            }
        }
    }
}

void SceneViewPanel::HandleDragAndDrop(ImVec2 minPos, ImVec2 size) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
            std::string droppedPathStr = static_cast<const char*>(payload->Data);

            Irufemi::Vector3 dropPos = {0.0f, 0.0f, 0.0f};
            auto* engine = editorManager_->GetEngine();
            if (engine) {
                if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    Irufemi::Vector2 localMousePos = {mousePos.x - minPos.x, mousePos.y - minPos.y};
                    float targetWidth = camera->GetViewportWidth();
                    float targetHeight = camera->GetViewportHeight();
                    float scaleX = targetWidth / size.x;
                    float scaleY = targetHeight / size.y;
                    Irufemi::Vector2 scaledVirtualPos = {localMousePos.x * scaleX, localMousePos.y * scaleY};

                    Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
                    Irufemi::Matrix4x4 invViewProj = Irufemi::Math::Inverse(viewProj);
                    Irufemi::Ray ray =
                        Irufemi::Math::ScreenPointToRay(scaledVirtualPos, targetWidth, targetHeight, invViewProj);

                    if (std::abs(ray.diff.y) > 0.001f) {
                        float t = -ray.origin.y / ray.diff.y;
                        if (t > 0.0f) {
                            dropPos = ray.origin + ray.diff * t;
                        } else {
                            dropPos = ray.origin + ray.diff * 10.0f;
                        }
                    } else {
                        dropPos = ray.origin + ray.diff * 10.0f;
                    }
                }
            }

            if (auto am = editorManager_->GetActionManager()) {
                am->CreateObjectFromAsset(droppedPathStr, dropPos);
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneViewPanel::HandlePicking(ImVec2 mousePos, ImVec2 minPos, ImVec2 maxPos, ImVec2 size) {
    // プレイモード中（ゲーム進行中）のインゲームのクリック操作（射撃など）と競合するため、
    // 明示的に許可されていない場合はピッキングを無効にする
    if (editorManager_->IsPlayMode() && !editorManager_->IsPickingAllowedInPlayMode()) {
        return;
    }

    bool isGizmoUsing = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isGizmoUsing) {
        auto* engine = editorManager_->GetEngine();
        bool isHit = false;
        GameObject* closestObj = nullptr;
        float closestDist = 1000.0f;

        float targetWidth = 1280.0f;
        float targetHeight = 720.0f;
        if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
            targetWidth = camera->GetViewportWidth();
            targetHeight = camera->GetViewportHeight();
        }
        Irufemi::Vector2 localMousePos = {mousePos.x - minPos.x, mousePos.y - minPos.y};
        float scaleX = targetWidth / size.x;
        float scaleY = targetHeight / size.y;
        Irufemi::Vector2 scaledVirtualPos = {localMousePos.x * scaleX, localMousePos.y * scaleY};

        // --- 1. まず 2D (Sprite) のピッキング判定を行う ---
        // --- 1. Sprite や Text などの 2D UI 要素を先に判定 ---
        if (auto scene = engine->GetSceneManager()->GetCurrentScene()) {
            auto gameObjects = scene->GetGameObjects();

            std::function<void(const std::shared_ptr<GameObject>&)> PickUI =
                [&](const std::shared_ptr<GameObject>& obj) {
                    if (!obj || obj->IsDestroyed() || !obj->GetIsActive()) {
                        return;
                    }

                    if (auto spriteComp = obj->GetComponent<SpriteRendererComponent>()) {
                        if (auto transform = obj->GetComponent<TransformComponent>()) {
                            auto sprite = spriteComp->GetSprite();
                            if (sprite) {
                                Irufemi::Vector2 sizeScaled = sprite->GetSize();
                                Irufemi::Vector2 anchor = sprite->GetAnchor();
                                Irufemi::Vector3 pos = transform->GetWorldPosition();

                                float left = pos.x - sizeScaled.x * anchor.x;
                                float top = pos.y - sizeScaled.y * anchor.y;
                                float right = pos.x + sizeScaled.x * (1.0f - anchor.x);
                                float bottom = pos.y + sizeScaled.y * (1.0f - anchor.y);

                                if (scaledVirtualPos.x >= left && scaledVirtualPos.x <= right &&
                                    scaledVirtualPos.y >= top && scaledVirtualPos.y <= bottom) {
                                    closestObj = obj.get();
                                    isHit = true;
                                    return;
                                }
                            }
                        }
                    }

                    if (!isHit) {
                        if (auto textComp = obj->GetComponent<TextRendererComponent>()) {
                            if (auto transform = obj->GetComponent<TransformComponent>()) {
                                Irufemi::Vector3 pos = transform->GetWorldPosition();
                                Irufemi::Vector2 minBounds = textComp->GetLocalBoundsMin();
                                Irufemi::Vector2 maxBounds = textComp->GetLocalBoundsMax();

                                float left = pos.x + minBounds.x * transform->GetWorldScale().x;
                                float right = pos.x + maxBounds.x * transform->GetWorldScale().x;
                                float top = pos.y + minBounds.y * transform->GetWorldScale().y;
                                float bottom = pos.y + maxBounds.y * transform->GetWorldScale().y;

                                if (scaledVirtualPos.x >= left && scaledVirtualPos.x <= right &&
                                    scaledVirtualPos.y >= top && scaledVirtualPos.y <= bottom) {
                                    closestObj = obj.get();
                                    isHit = true;
                                    return;
                                }
                            }
                        }
                    }

                    for (auto it = obj->GetChildren().rbegin(); it != obj->GetChildren().rend(); ++it) {
                        PickUI(*it);
                        if (isHit) {
                            return;
                        }
                    }
                };

            for (auto it = gameObjects.rbegin(); it != gameObjects.rend(); ++it) {
                PickUI(*it);
                if (isHit) {
                    break;
                }
            }
        }

        // --- 2. Sprite に当たらなかった場合のみ 3D のピッキングを行う ---
        if (!isHit) {
            if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
                Irufemi::Matrix4x4 viewProjInverse = Irufemi::Math::Inverse(viewProj);
                Irufemi::Ray ray = Irufemi::Math::ScreenPointToRay(localMousePos, size.x, size.y, viewProjInverse);

                RaycastHit hit;
                if (engine && engine->GetCollisionManager()->Raycast(ray, hit, 1000.0f)) {
                    closestDist = hit.distance;
                    closestObj = hit.hitObject;
                    isHit = true;
                }

                if (auto scene = engine->GetSceneManager()->GetCurrentScene()) {
                    std::function<void(const std::shared_ptr<GameObject>&)> Pick3D =
                        [&](const std::shared_ptr<GameObject>& obj) {
                            if (!obj || obj->IsDestroyed() || !obj->GetIsActive() || obj.get() == closestObj) {
                                return;
                            }

                            float dist = 0.0f;
                            for (auto& comp : obj->GetComponents()) {
                                if (comp->Raycast(ray, dist)) {
                                    if (dist < closestDist) {
                                        closestDist = dist;
                                        closestObj = obj.get();
                                        isHit = true;
                                    }
                                }
                            }
                            for (auto& child : obj->GetChildren()) {
                                Pick3D(child);
                            }
                        };

                    for (auto& obj : scene->GetGameObjects()) {
                        Pick3D(obj);
                    }
                }
            }
        }

        if (isHit && closestObj) {
            editorManager_->SetSelectedObject(closestObj->shared_from_this());
        } else {
            editorManager_->ClearSelectedObject();
        }
    }
}

void SceneViewPanel::SnapToGameCamera() {
    auto* engine = editorManager_->GetEngine();
    if (!engine) {
        return;
    }
    auto* scene = engine->GetSceneManager()->GetCurrentScene();
    if (!scene) {
        return;
    }

    // 1. シーン内から MainCamera または CameraComponent を持つ GameObject を再帰探索
    std::shared_ptr<GameObject> camObj = nullptr;
    std::function<void(const std::shared_ptr<GameObject>&)> findCam = [&](const std::shared_ptr<GameObject>& obj) {
        if (!obj || camObj) {
            return;
        }
        if (obj->GetName() == "MainCamera" && obj->GetComponent<CameraComponent>()) {
            camObj = obj;
            return;
        }
        for (const auto& child : obj->GetChildren()) {
            findCam(child);
        }
    };

    for (const auto& obj : scene->GetGameObjects()) {
        findCam(obj);
        if (camObj) {
            break;
        }
    }

    if (!camObj) {
        std::function<void(const std::shared_ptr<GameObject>&)> findAnyCam =
            [&](const std::shared_ptr<GameObject>& obj) {
                if (!obj || camObj) {
                    return;
                }
                if (obj->GetComponent<CameraComponent>()) {
                    camObj = obj;
                    return;
                }
                for (const auto& child : obj->GetChildren()) {
                    findAnyCam(child);
                }
            };
        for (const auto& obj : scene->GetGameObjects()) {
            findAnyCam(obj);
            if (camObj) {
                break;
            }
        }
    }

    if (!camObj) {
        return;
    }

    // 2. TargetFollowComponent がある場合は静的に追従位置を解決 (Play開始時と同等)
    if (auto follow = camObj->GetComponent<TargetFollowComponent>()) {
        follow->SnapToTarget();
    }

    auto transform = camObj->GetTransform();
    if (!transform) {
        return;
    }

    Irufemi::Vector3 pos = transform->GetWorldPosition();
    Irufemi::Vector3 rot = transform->GetWorldRotation();

    // 3. SceneView のアクティブカメラに位置・回転・FOV・FarZ を適用
    if (auto activeCam = engine->GetCameraManager()->GetActiveCamera()) {
        activeCam->SetTranslate(pos);
        activeCam->SetRotate(rot);
        if (auto camComp = camObj->GetComponent<CameraComponent>()) {
            activeCam->SetFovY(camComp->GetFovAngleY());
            activeCam->SetFarClip(camComp->GetFarZ());
        }

        // 4. OrbitCameraController の注視点・距離を前方10mに再同期
        cameraController_.SyncTargetFromCamera(activeCam, 10.0f);
    }
}

#endif // EditorMode
