#include "UI/PromptController.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include "Engine/Irufemi.h"

PromptController::PromptController() {
    animator_.Reset();
}

void PromptController::SetTarget(Irufemi::StaticModelObject* targetObj) {
    targetObj_ = targetObj;
}

void PromptController::SetTarget(Irufemi::Sprite* targetSprite) {
    targetSprite_ = targetSprite;
}

void PromptController::Update(Irufemi::InputManager* input, float dt) {
    animator_.Update(dt);

    if (!isDecided_) {
        // --- 待機中 ---
        isVisible_ = true;
        float alpha = animator_.GetPulseAlpha(0.6f, 0.4f, 3.0f);

        if (targetObj_) {
            targetObj_->SetAlpha(alpha);
        }
        if (targetSprite_) {
            Irufemi::Vector4 color = targetSprite_->GetColor();
            color.w = alpha;
            targetSprite_->SetColor(color);
        }

        // キー入力判定
        if (input && input->IsKeyPressed(triggerKey_)) {
            isDecided_ = true;
            animator_.Reset(); // 決定時にアニメーションをリセット
            transitionDelayTimer_ = 0.0f;
        }
    } else {
        // --- 決定後 ---
        transitionDelayTimer_ += dt;

        // 高速フラッシュ
        isVisible_ = animator_.GetFlashVisibility(40.0f);

        // アルファ値は最大にしておく
        if (targetObj_) {
            targetObj_->SetAlpha(1.0f);
        }
        if (targetSprite_) {
            Irufemi::Vector4 color = targetSprite_->GetColor();
            color.w = 1.0f;
            targetSprite_->SetColor(color);
        }
    }
}

void PromptController::Draw() {
    if (!isVisible_) {
        return;
    }

    if (targetObj_) {
        targetObj_->Draw();
    }
    if (targetSprite_) {
        targetSprite_->Draw();
    }
}

bool PromptController::ShouldTransition() const {
    return isDecided_ && (transitionDelayTimer_ >= kTransitionDelayLimit);
}
