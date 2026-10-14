#include "Framework/Component/UI/SliderComponent.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include <algorithm>

void SliderComponent::OnRegisterProperties() {
    RegisterProperty("Hitbox Scale", &hitboxScale_);
    RegisterProperty("Value", &value_);
    RegisterGameObjectRef("Handle Object ID", &handleObjectID_);
}

void SliderComponent::Initialize() {
    OnAwake();
}

void SliderComponent::OnAwake() {
    if (gameObject_) {
        backgroundSprite_ = gameObject_->GetComponent<SpriteRendererComponent>();
    }
}

void SliderComponent::Start() {
    ResolveHandleObject();
}

void SliderComponent::SetHandleObjectID(uint64_t id) {
    handleObjectID_ = id;
    ResolveHandleObject();
}

void SliderComponent::ResolveHandleObject() {
    if (handleObjectID_ == 0 || !gameObject_) {
        handleObject_.reset();
        return;
    }

    // シーンの高速ハッシュ検索を試みる
    if (auto scene = gameObject_->GetScene()) {
        if (auto obj = scene->FindGameObjectByID(handleObjectID_)) {
            handleObject_ = obj;
            return;
        }
    }

    // フォールバックとして子オブジェクトから検索
    for (auto& child : gameObject_->GetChildren()) {
        if (child && child->GetInstanceID() == handleObjectID_) {
            handleObject_ = child;
            return;
        }
    }
}

bool SliderComponent::CheckBounds(const Irufemi::Vector2& mousePos) {
    if (!GetTransform() || !backgroundSprite_) {
        return false;
    }

    Irufemi::Vector3 pos = GetTransform()->GetWorldPosition();

    auto* s = backgroundSprite_->GetSprite();
    if (!s) {
        return false;
    }

    Irufemi::Vector2 anchor = s->GetAnchor();
    Irufemi::Vector2 baseSize = s->GetSize();

    float width = baseSize.x * hitboxScale_.x;
    float height = baseSize.y * hitboxScale_.y;

    float left = pos.x - width * anchor.x;
    float right = pos.x + width * (1.0f - anchor.x);
    float top = pos.y - height * anchor.y;
    float bottom = pos.y + height * (1.0f - anchor.y);

    return (mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom);
}

void SliderComponent::Update() {
    if (!backgroundSprite_ || !gameObject_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto input = engine->GetInputManager();
    auto cameraManager = engine->GetCameraManager();
    if (!input || !cameraManager || !cameraManager->GetActiveCamera()) {
        return;
    }

    Irufemi::Vector2 mousePos = input->GetMousePosition();
    Irufemi::Vector2 uiPos = cameraManager->GetActiveCamera()->ScreenToUIPosition(mousePos);

    bool isHovered = CheckBounds(uiPos);

    if (input->IsMouseButtonPressed(Mouse::Button::Left)) {
        if (isHovered) {
            isDragging_ = true;
        }
    }

    if (input->IsMouseButtonReleased(Mouse::Button::Left)) {
        isDragging_ = false;
    }

    if (isDragging_) {
        // マウス位置からValueを計算
        Irufemi::Vector3 pos = GetTransform()->GetWorldPosition();
        auto* s = backgroundSprite_->GetSprite();

        if (s) {
            Irufemi::Vector2 anchor = s->GetAnchor();
            float width = s->GetSize().x;

            float left = pos.x - width * anchor.x;
            float right = pos.x + width * (1.0f - anchor.x);

            // X座標の割合を計算し、0.0〜1.0にクランプ
            if (width > 0) {
                float newValue = (uiPos.x - left) / width;
                newValue = std::clamp(newValue, 0.0f, 1.0f);

                if (newValue != value_) {
                    value_ = newValue;
                    if (onValueChangedCallback_) {
                        onValueChangedCallback_(value_);
                    }
                }
            }
        }
    }

    // ツマミ（ハンドル）の位置を更新
    UpdateHandlePosition();
}

void SliderComponent::SetValue(float value) {
    value_ = std::clamp(value, 0.0f, 1.0f);
    UpdateHandlePosition();
}

void SliderComponent::UpdateHandlePosition() {
    auto handle = handleObject_.lock();
    if (!handle || !backgroundSprite_ || !GetTransform()) {
        return;
    }

    auto handleTransform = handle->GetTransform();
    if (!handleTransform) {
        return;
    }

    auto* s = backgroundSprite_->GetSprite();
    if (!s) {
        return;
    }

    // スライダー背景のワールド幅を計算
    float width = s->GetSize().x;
    Irufemi::Vector2 anchor = s->GetAnchor();

    // 背景の中心からの相対X座標を計算する
    // value=0 なら左端、value=1 なら右端
    float leftLocal = -width * anchor.x;
    float rightLocal = width * (1.0f - anchor.x);

    float targetLocalX = leftLocal + width * value_;

    // ハンドルが背景の子オブジェクトである場合、ローカル位置のXを更新する
    // (UI構成として、Backgroundを親、Handleを子にすることを想定)
    Irufemi::Vector3 localPos = handleTransform->GetPosition();
    localPos.x = targetLocalX;
    handleTransform->SetPosition(localPos);
}

void SliderComponent::OnDisable() {
    isDragging_ = false;
}
