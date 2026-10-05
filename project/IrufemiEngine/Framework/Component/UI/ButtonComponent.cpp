#include "Framework/Component/UI/ButtonComponent.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Framework/Scene/SceneTransition.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include <algorithm>

void ButtonComponent::OnRegisterProperties() {
    RegisterProperty("Normal Color", &normalColor_);
    RegisterProperty("Hover Color", &hoverColor_);
    RegisterProperty("Click Color", &clickColor_);
    RegisterProperty("Enable Hover Pulse", &enableHoverPulse_);
    RegisterProperty("Enable Idle Pulse", &enableIdlePulse_);
    RegisterProperty("Hitbox Scale", &hitboxScale_);
}

void ButtonComponent::Initialize() {
    OnAwake();
}

void ButtonComponent::OnAwake() {
    if (gameObject_) {
        sprite_ = gameObject_->GetComponent<SpriteRendererComponent>();
        text_ = gameObject_->GetComponent<TextRendererComponent>();
    }
}

bool ButtonComponent::CheckBounds(const Irufemi::Vector2& cursorPos) {
    if (!GetTransform()) {
        return false;
    }

    Irufemi::Vector3 pos = GetTransform()->GetWorldPosition();
    Irufemi::Vector3 scale = GetTransform()->GetWorldScale();

    // 1. TextRendererComponent が存在する場合：文字幾何バウンディングボックスから判定
    if (text_) {
        const auto& minB = text_->GetLocalBoundsMin();
        const auto& maxB = text_->GetLocalBoundsMax();
        if (minB.x != maxB.x && minB.y != maxB.y) {
            float width = (maxB.x - minB.x) * hitboxScale_.x;
            float height = (maxB.y - minB.y) * hitboxScale_.y;
            float centerX = pos.x + (minB.x + maxB.x) * 0.5f * scale.x;
            float centerY = pos.y + (minB.y + maxB.y) * 0.5f * scale.y;
            float halfW = width * 0.5f * scale.x;
            float halfH = height * 0.5f * scale.y;

            return (cursorPos.x >= centerX - halfW && cursorPos.x <= centerX + halfW &&
                    cursorPos.y >= centerY - halfH && cursorPos.y <= centerY + halfH);
        }
    }

    // 2. SpriteRendererComponent が存在する場合：スプライト矩形から判定
    if (sprite_ && sprite_->GetSprite()) {
        auto* s = sprite_->GetSprite();
        Irufemi::Vector2 anchor = s->GetAnchor();
        Irufemi::Vector2 baseSize = s->GetSize();

        float width = baseSize.x * hitboxScale_.x;
        float height = baseSize.y * hitboxScale_.y;

        float left = pos.x - width * anchor.x;
        float right = pos.x + width * (1.0f - anchor.x);
        float top = pos.y - height * anchor.y;
        float bottom = pos.y + height * (1.0f - anchor.y);

        return (cursorPos.x >= left && cursorPos.x <= right && cursorPos.y >= top && cursorPos.y <= bottom);
    }

    // 3. 描画コンポーネントが無い場合：Transformのスケールを透明ヒットボックスとして判定
    float halfW = scale.x * 0.5f * hitboxScale_.x;
    float halfH = scale.y * 0.5f * hitboxScale_.y;
    return (cursorPos.x >= pos.x - halfW && cursorPos.x <= pos.x + halfW && cursorPos.y >= pos.y - halfH &&
            cursorPos.y <= pos.y + halfH);
}

void ButtonComponent::Update() {
    if (!gameObject_) {
        return;
    }

    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }
    auto engine = scene->GetEngine();
    if (!engine) {
        return;
    }

    auto input = engine->GetInputManager();
    if (!input) {
        return;
    }

    // 統合仮想カーソル（マウス/ゲームパッド自動調停）のUI座標を取得
    const Irufemi::Vector2& cursorPos = input->GetVirtualCursorPosition();

    isHovered_ = CheckBounds(cursorPos);
    isClicked_ = false;

    // アニメーターの更新
    animator_.Update(engine->GetDeltaTime());

    // 色適用用ラムダ
    auto applyColor = [this](const Irufemi::Vector4& col) {
        if (sprite_ && sprite_->GetSprite()) {
            sprite_->GetSprite()->SetColor(col);
        }
        if (text_) {
            text_->SetColor(col);
        }
    };

    if (isHovered_) {
        // ホバーした瞬間に押下されたらフラグを立てる
        if (input->IsCursorActionPressed()) {
            isPressedOnButton_ = true;
        }

        if (input->IsCursorActionDown()) {
            // 押下中（ボタン上で押下開始した場合のみ色を変える）
            if (isPressedOnButton_) {
                applyColor(clickColor_);
            } else {
                applyColor(normalColor_);
            }
        } else {
            // ホバー中
            Irufemi::Vector4 color = hoverColor_;
            if (enableHoverPulse_) {
                float animAlpha = animator_.GetPulseAlpha(0.7f, 0.3f, 5.0f);
                color.w *= animAlpha;
            }
            applyColor(color);

            // 離された瞬間（同一ボタン上でのクリック完了）
            if (input->IsCursorActionReleased() && isPressedOnButton_) {
                isClicked_ = true;
                if (onClickCallback_) {
                    onClickCallback_();
                }
            }
        }
    } else {
        // 通常状態（待機中）
        Irufemi::Vector4 color = normalColor_;
        if (enableIdlePulse_) {
            float animAlpha = animator_.GetPulseAlpha(0.6f, 0.4f, 3.0f);
            color.w *= animAlpha;
        } else {
            animator_.Reset();
        }
        applyColor(color);
    }

    // どこかでカーソルアクションが離されたら押下フラグを安全にリセット
    if (input->IsCursorActionReleased()) {
        isPressedOnButton_ = false;
    }
}
