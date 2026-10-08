#include "Scenes/HowToPlay/HowToPlayScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Audio/AudioManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "UI/UISound.h"
#include <cmath>
#include <algorithm>

void HowToPlayScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);
    openCooldownTimer_ = 0.15f;
    state_ = TransitionState::Opening;
    transitionTimer_ = 0.0f;
    uiCached_ = false;
}

void HowToPlayScene::OnEnter() {
    BaseScene::OnEnter();
    openCooldownTimer_ = 0.15f;
    state_ = TransitionState::Opening;
    transitionTimer_ = 0.0f;
    uiCached_ = false;
}

void HowToPlayScene::CacheUIElements() {
    cachedElements_.clear();
    const auto& objs = GetGameObjects();
    for (const auto& obj : objs) {
        if (!obj) {
            continue;
        }
        auto transform = obj->GetComponent<TransformComponent>();
        if (!transform) {
            continue;
        }

        UIElementState state{};
        state.obj = obj;
        state.basePos = transform->GetPosition();
        state.baseScale = transform->GetScale();
        state.isBackdrop = (obj->GetName() == "DarkBackdrop");

        if (auto sprite = obj->GetComponent<SpriteRendererComponent>()) {
            state.baseColor = sprite->GetColor();
            state.isSprite = true;
            cachedElements_.push_back(state);
        } else if (auto text = obj->GetComponent<TextRendererComponent>()) {
            state.baseColor = text->GetColor();
            state.isSprite = false;
            cachedElements_.push_back(state);
        }
    }
    uiCached_ = true;
}

void HowToPlayScene::ApplyTransition(float progress) {
    if (!uiCached_) {
        CacheUIElements();
    }

    const Irufemi::Vector3 center{640.0f, 360.0f, 0.0f};

    for (auto& elem : cachedElements_) {
        if (!elem.obj) {
            continue;
        }
        auto transform = elem.obj->GetComponent<TransformComponent>();
        if (!transform) {
            continue;
        }

        if (elem.isBackdrop) {
            // 暗幕は位置・スケール固定、アルファのみフェード
            if (auto sprite = elem.obj->GetComponent<SpriteRendererComponent>()) {
                sprite->SetColor({elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
            }
        } else {
            // UI要素群を中心からスケール補間＆アルファフェード (0.95 -> 1.0)
            float scaleMul = std::lerp(0.95f, 1.0f, progress);
            Irufemi::Vector3 offset = elem.basePos - center;
            transform->SetPosition(center + offset * scaleMul);
            transform->SetScale({elem.baseScale.x * scaleMul, elem.baseScale.y * scaleMul, elem.baseScale.z});

            if (elem.isSprite) {
                if (auto sprite = elem.obj->GetComponent<SpriteRendererComponent>()) {
                    sprite->SetColor(
                        {elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
                }
            } else {
                if (auto text = elem.obj->GetComponent<TextRendererComponent>()) {
                    text->SetColor({elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
                }
            }
        }
    }
}

void HowToPlayScene::Update() {
    BaseScene::Update();

    if (!engine_) {
        return;
    }

    float dt = engine_->GetDeltaTime();

    if (!uiCached_) {
        CacheUIElements();
        ApplyTransition(0.0f); // 初回フレームで非表示から開始
    }

    // --- オープニング演出 (Fade & Scale In) ---
    if (state_ == TransitionState::Opening) {
        transitionTimer_ += dt;
        float progress = std::clamp(transitionTimer_ / kOpenDuration_, 0.0f, 1.0f);
        // EaseOutCubic
        float ease = 1.0f - std::pow(1.0f - progress, 3.0f);
        ApplyTransition(ease);

        if (progress >= 1.0f) {
            state_ = TransitionState::Open;
            ApplyTransition(1.0f);
        }
        return; // オープン途中は閉じる入力をガード
    }

    // --- クロージング演出 (Fade & Scale Out) ---
    if (state_ == TransitionState::Closing) {
        transitionTimer_ += dt;
        float progress = std::clamp(transitionTimer_ / kCloseDuration_, 0.0f, 1.0f);
        // EaseInQuad
        float ease = 1.0f - (progress * progress);
        ApplyTransition(ease);

        if (progress >= 1.0f) {
            if (auto sm = engine_->GetSceneManager()) {
                sm->PopScene();
            }
        }
        return;
    }

    // --- 通常表示状態 (Open) 入力判定 ---
    auto inputManager = engine_->GetInputManager();
    if (!inputManager) {
        return;
    }

    if (openCooldownTimer_ > 0.0f) {
        openCooldownTimer_ -= dt;
        return; // 開いた直後のクリック・ボタン入力残存による即時クローズをガード
    }

    // 閉じる入力判定 (Bボタン / ESC / BackSpace / マウス左クリック)
    bool closeTrigger = inputManager->IsButtonPressed(XINPUT_GAMEPAD_B) || inputManager->IsKeyPressed(VK_ESCAPE) ||
                        inputManager->IsKeyPressed(VK_BACK);

    if (!closeTrigger && inputManager->GetMouse()) {
        closeTrigger =
            inputManager->GetMouse()->IsButtonPressed(Mouse::Button::Left) || inputManager->IsKeyPressed(VK_LBUTTON);
    }

    if (closeTrigger) {
        UISound::PlayCancel();
        state_ = TransitionState::Closing;
        transitionTimer_ = 0.0f;
    }
}

void HowToPlayScene::Draw() {
    BaseScene::Draw();
}
