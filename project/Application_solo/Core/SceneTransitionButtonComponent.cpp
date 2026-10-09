#include "Core/SceneTransitionButtonComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/UI/ButtonComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneTransition.h"
#include <array>

void SceneTransitionButtonComponent::OnRegisterProperties() {
    RegisterProperty("Load Scene Name", &onClickLoadScene_);
    // 0:Fade, 1:Dissolve, 2:Slide, 3:RadialBlur
    RegisterProperty("Transition Type(0-3)", &transitionType_);
    RegisterProperty("Transition Duration", &transitionDuration_);
    RegisterProperty("Transition Delay", &transitionDelay_);
    RegisterProperty("Click Anim Duration", &clickAnimDuration_);
}

void SceneTransitionButtonComponent::OnAwake() {
    if (gameObject_) {
        button_ = gameObject_->GetComponent<ButtonComponent>();
        sprite_ = gameObject_->GetComponent<SpriteRendererComponent>();
    }
}

void SceneTransitionButtonComponent::Initialize() {
    OnAwake();
}

void SceneTransitionButtonComponent::Update() {
    if (!button_ || !gameObject_ || !sprite_) {
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

    float dt = engine->GetDeltaTime();
    animator_.Update(dt);

    if (isTransitionPending_) {
        transitionTimer_ -= dt;

        if (GetTransform()) {
            float timePassed = transitionDelay_ - transitionTimer_;
            if (timePassed <= clickAnimDuration_ && clickAnimDuration_ > 0.0f) {
                // アニメーション中は少し縮小する（0.9倍）
                GetTransform()->SetScale(originalScale_ * 0.9f);

                // 高速フラッシュ演出
                bool isVisible = animator_.GetFlashVisibility(40.0f);
                if (isVisible) {
                    sprite_->GetSprite()->SetColor({0.5f, 0.5f, 0.5f, 1.0f}); // clickColor (fallback)
                } else {
                    // 非表示状態（アルファ0）
                    sprite_->GetSprite()->SetColor({0.5f, 0.5f, 0.5f, 0.0f});
                }
            } else {
                // アニメーションが終わったら元のスケールと色に戻す
                GetTransform()->SetScale(originalScale_);
                sprite_->GetSprite()->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
            }
        }

        // 待機時間が終了したらシーン遷移を実行
        if (transitionTimer_ <= 0.0f) {
            isTransitionPending_ = false;

            if (!onClickLoadScene_.empty()) {
                static constexpr std::array<SceneTransition::Type, 4> kTransitionTypes = {
                    SceneTransition::Type::Fade,       // 0
                    SceneTransition::Type::Dissolve,   // 1
                    SceneTransition::Type::Slide,      // 2
                    SceneTransition::Type::RadialBlur  // 3
                };
                SceneTransition::Type type = SceneTransition::Type::Fade;
                if (transitionType_ >= 0 && static_cast<size_t>(transitionType_) < kTransitionTypes.size()) {
                    type = kTransitionTypes[static_cast<size_t>(transitionType_)];
                }
                engine->GetSceneManager()->LoadScene(onClickLoadScene_, type, transitionDuration_);
            }
        }
        return;
    }

    if (button_->IsClicked() && !onClickLoadScene_.empty()) {
        isTransitionPending_ = true;
        transitionTimer_ = transitionDelay_;
        if (GetTransform()) {
            originalScale_ = GetTransform()->GetScale();
        }
    }
}
