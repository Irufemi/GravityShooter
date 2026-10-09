#include "Scenes/Options/OptionsScene.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/UI/ButtonComponent.h"
#include "Framework/Component/UI/SliderComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Utility/CVar.h"
#include "Audio/AudioManager.h"
#include "Audio/Sound.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Utility/Log.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Font/FontManager.h"
#include "Input/GameAction.h"
#include "UI/UISound.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
// 感度マッピング定数（250.0f 〜 1250.0f、基準 650.0f = 0.4x）
constexpr float kMinCursorSpeed = 250.0f;
constexpr float kMaxCursorSpeed = 1250.0f;
} // namespace

std::vector<OptionsScene::SliderBinding> OptionsScene::GetSliderBindings() {
    return {
        {"Slider_BGM", "ValueText_BGM", sliderBGM_, valueTextBGM_, rectSliderBGM_,
         []() { return Irufemi::CVarSystem::GetFloat("a.BGMVolume"); },
         [](float val, InputManager*) {
             Irufemi::CVarSystem::SetFloat("a.BGMVolume", val);
             Irufemi::CVarSystem::SetFloat("a.MasterVolume", val);
         }},
        {"Slider_SE", "ValueText_SE", sliderSE_, valueTextSE_, rectSliderSE_,
         []() { return Irufemi::CVarSystem::GetFloat("a.SEVolume"); },
         [](float val, InputManager*) {
             Irufemi::CVarSystem::SetFloat("a.SEVolume", val);
         }},
        {"Slider_Sensitivity", "ValueText_Sensitivity", sliderSensitivity_, valueTextSensitivity_,
         rectSliderSensitivity_,
         []() {
             float speed = Irufemi::CVarSystem::GetFloat("i.CursorSpeed");
             if (speed <= 0.0f) {
                 speed = 650.0f;
             }
             return std::clamp((speed - kMinCursorSpeed) / (kMaxCursorSpeed - kMinCursorSpeed), 0.0f, 1.0f);
         },
         [](float val, InputManager* input) {
             float newSpeed = kMinCursorSpeed + val * (kMaxCursorSpeed - kMinCursorSpeed);
             Irufemi::CVarSystem::SetFloat("i.CursorSpeed", newSpeed);
             if (input) {
                 input->SetVirtualCursorBaseSpeed(newSpeed);
             }
         }}};
}



void OptionsScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // スライダー数値で使用する文字を非同期（スレッドプール）でバックグラウンド生成
    // メインスレッドの初期ロードを阻害せず、操作時の文字飛びを防止
    if (engine) {
        if (auto fm = engine->GetFontManager()) {
            fm->PrecacheText("toro_glitch", L"0123456789%");
        }
        if (auto input = engine->GetInputManager()) {
            input->SetVirtualCursorPosition({640.0f, 360.0f});
        }
    }

    isDraggingSlider_ = false;
    draggingSlider_ = nullptr;
    lastHoveredTarget_ = nullptr;
    openCooldownTimer_ = 0.15f; // 前画面からのクリック残存をガード
    uiBound_ = false;
    transitionState_ = TransitionState::Opening;
    transitionTimer_ = 0.0f;
    uiCached_ = false;
}

void OptionsScene::OnEnter() {
    BaseScene::OnEnter();
    openCooldownTimer_ = 0.15f;
    transitionState_ = TransitionState::Opening;
    transitionTimer_ = 0.0f;
    uiCached_ = false;
}

void OptionsScene::CacheUIElements() {
    cachedElements_.clear();
    const auto& objs = GetGameObjects();
    for (const auto& obj : objs) {
        if (!obj || obj == virtualCursorObj_) {
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
        state.isBackdrop = (obj->GetName() == "OptionsUI_Root");

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

void OptionsScene::ApplyTransition(float progress) {
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
            // 暗幕はアルファのみフェード
            if (auto sprite = elem.obj->GetComponent<SpriteRendererComponent>()) {
                sprite->SetColor({elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
            }
        } else {
            // UIパネル群を中心からスケール補間＆アルファフェード (0.95 -> 1.0)
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

void OptionsScene::Update() {
    BaseScene::Update();

    if (!uiBound_) {
        BindUIComponents();
        uiBound_ = true;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto input = engine->GetInputManager();
    if (!input) {
        return;
    }

    float dt = engine->GetDeltaTime();

    if (!uiCached_) {
        CacheUIElements();
        ApplyTransition(0.0f);
    }

    // --- オープニング演出 (Fade & Scale In) ---
    if (transitionState_ == TransitionState::Opening) {
        transitionTimer_ += dt;
        float progress = std::clamp(transitionTimer_ / kOpenDuration_, 0.0f, 1.0f);
        // EaseOutCubic
        float ease = 1.0f - std::pow(1.0f - progress, 3.0f);
        ApplyTransition(ease);

        if (progress >= 1.0f) {
            transitionState_ = TransitionState::Open;
            ApplyTransition(1.0f);
        }
        return; // オープン途中は操作入力をガード
    }

    // --- クロージング演出 (Fade & Scale Out) ---
    if (transitionState_ == TransitionState::Closing) {
        transitionTimer_ += dt;
        float progress = std::clamp(transitionTimer_ / kCloseDuration_, 0.0f, 1.0f);
        // EaseInQuad
        float ease = 1.0f - (progress * progress);
        ApplyTransition(ease);

        if (progress >= 1.0f) {
            if (auto sm = engine->GetSceneManager()) {
                sm->PopScene();
            }
        }
        return;
    }

    if (openCooldownTimer_ > 0.0f) {
        openCooldownTimer_ -= dt;
        return; // 開いた直後のクリック・ボタン入力残存による即時クローズをガード
    }

    // =========================================================================
    // 1. 即時脱出判定 (Bボタン / ESC / BackSpace)
    // =========================================================================
    if (input->IsCancelPressed()) {
        UISound::PlayCancel();
        transitionState_ = TransitionState::Closing;
        transitionTimer_ = 0.0f;
        return;
    }

    // =========================================================================
    // 2. 仮想カーソル更新 (エンジン側InputManagerに委譲)
    // =========================================================================
    UpdateVirtualCursor(dt);

    // =========================================================================
    // 3. BACK / CLOSE ボタン判定 (確実な矩形判定 ＆ 決定入力)
    // =========================================================================
    const auto& cursorPos = input->GetVirtualCursorPosition();
    bool isOverCloseButton = rectButtonClose_.Contains(cursorPos.x, cursorPos.y);

    bool isDecidePressed = input->IsCursorActionPressed() || InputHelper::IsActionPressed(input, GameAction::UI_Submit);

    if (isOverCloseButton && isDecidePressed && !isDraggingSlider_) {
        UISound::PlayDecide();
        transitionState_ = TransitionState::Closing;
        transitionTimer_ = 0.0f;
        return;
    }

    // =========================================================================
    // 4. スライダー操作更新 (マウスドラッグ ＆ スティック左右直感操作)
    // =========================================================================
    UpdateSliderDrag();
}

void OptionsScene::Finalize() {
    BaseScene::Finalize();
}

void OptionsScene::OnExit() {
    BaseScene::OnExit();
}

void OptionsScene::BindUIComponents() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    // スライダー群のバインド（データ駆動テーブルループ）
    for (auto& binding : GetSliderBindings()) {
        if (auto obj = FindGameObject(binding.objectName)) {
            binding.sliderRef = obj->GetComponent<SliderComponent>();
            if (auto t = obj->GetTransform()) {
                binding.rectRef.x = t->GetPosition().x;
                binding.rectRef.y = t->GetPosition().y;
                if (t->GetScale().x > 1.0f) {
                    binding.rectRef.halfW = t->GetScale().x * 0.5f;
                }
            }
            if (binding.sliderRef) {
                binding.sliderRef->SetValue(binding.initialValueGetter());
            }
        }
        if (auto textObj = FindGameObject(binding.textName)) {
            binding.textRef = textObj->GetComponent<TextRendererComponent>();
        }
    }

    // CLOSE ボタン
    if (auto obj = FindGameObject("Button_Close")) {
        buttonClose_ = obj->GetComponent<ButtonComponent>();
        if (auto t = obj->GetTransform()) {
            rectButtonClose_.x = t->GetPosition().x;
            rectButtonClose_.y = t->GetPosition().y;
            if (t->GetScale().x > 1.0f) {
                rectButtonClose_.halfW = t->GetScale().x * 0.5f;
            }
        }
    }

    // 仮想カーソルオブジェクト
    virtualCursorObj_ = FindGameObject("VirtualCursor");
    if (virtualCursorObj_) {
        virtualCursorRenderer_ = virtualCursorObj_->GetComponent<Primitive2DRendererComponent>();
        if (auto trans = virtualCursorObj_->GetTransform()) {
            if (auto input = engine->GetInputManager()) {
                const auto& pos = input->GetVirtualCursorPosition();
                trans->SetPosition({pos.x, pos.y, 0.0f});
            }
        }
    }

    // 初期テキスト更新
    UpdateValueTexts();
}

void OptionsScene::UpdateVirtualCursor(float deltaTime) {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto input = engine->GetInputManager();
    if (!input) {
        return;
    }

    const auto& cursorPos = input->GetVirtualCursorPosition();

    // 1. ホバー判定（データ駆動判定）
    void* currentHovered = nullptr;
    for (const auto& binding : GetSliderBindings()) {
        if (binding.sliderRef && binding.rectRef.Contains(cursorPos.x, cursorPos.y)) {
            currentHovered = binding.sliderRef;
            break;
        }
    }
    bool isOverClose = rectButtonClose_.Contains(cursorPos.x, cursorPos.y);
    if (!currentHovered && isOverClose) {
        currentHovered = buttonClose_;
    }

    // ホバー対象が変わった瞬間にカーソルSE再生
    if (currentHovered != lastHoveredTarget_) {
        if (currentHovered != nullptr) {
            UISound::PlayCursor();
        }
        lastHoveredTarget_ = currentHovered;
    }

    // CLOSEボタンのハイライト更新
    if (buttonClose_) {
        if (auto obj = buttonClose_->GetGameObject()) {
            if (auto sprite = obj->GetComponent<SpriteRendererComponent>()) {
                sprite->SetColor(isOverClose ? Irufemi::Vector4{0.15f, 0.55f, 0.75f, 1.0f}
                                             : Irufemi::Vector4{0.08f, 0.25f, 0.35f, 0.9f});
            }
        }
    }

    // 2. エンジンのInputManagerに仮想カーソル更新を委譲（ホバー摩擦を適用）
    float speedMultiplier = (currentHovered != nullptr) ? kStickyFriction_ : 1.0f;
    input->UpdateVirtualCursor(deltaTime, speedMultiplier);

    // 3. 仮想カーソルGameObjectのTransform位置および見た目を同期
    const auto& newCursorPos = input->GetVirtualCursorPosition();
    if (virtualCursorObj_) {
        if (auto trans = virtualCursorObj_->GetTransform()) {
            trans->SetPosition({newCursorPos.x, newCursorPos.y, 0.0f});

            float targetScale = (currentHovered != nullptr) ? 1.2f : 1.0f;
            trans->SetScale({targetScale, targetScale, 1.0f});
        }

        if (virtualCursorRenderer_) {
            // ゲームパッド操作中のみリングカーソルを表示し、物理マウス操作時はマウスカーソルに委ねる（非表示）
            if (input->IsUsingGamepadCursor()) {
                virtualCursorRenderer_->SetColor((currentHovered != nullptr)
                                                     ? Irufemi::Vector4{0.2f, 1.0f, 0.95f, 1.0f}
                                                     : Irufemi::Vector4{0.1f, 0.95f, 1.0f, 0.85f});
            } else {
                virtualCursorRenderer_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
    }
}

void OptionsScene::UpdateSliderDrag() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto input = engine->GetInputManager();
    if (!input) {
        return;
    }

    float dt = engine->GetDeltaTime();
    const auto& cursorPos = input->GetVirtualCursorPosition();

    auto bindings = GetSliderBindings();

    // =========================================================================
    // A. 十字キー左右による微調整 (カーソル通過時の誤動作を防ぐためスティック増減は撤廃)
    // =========================================================================
    float directAdjust = 0.0f;
    if (input->DPadRight() || input->IsKeyDown(VK_RIGHT) || input->IsKeyDown('D')) {
        directAdjust = 0.5f * dt;
    } else if (input->DPadLeft() || input->IsKeyDown(VK_LEFT) || input->IsKeyDown('A')) {
        directAdjust = -0.5f * dt;
    }

    if (directAdjust != 0.0f) {
        for (const auto& binding : bindings) {
            if (binding.sliderRef && binding.rectRef.Contains(cursorPos.x, cursorPos.y)) {
                float val = std::clamp(binding.sliderRef->GetValue() + directAdjust, 0.0f, 1.0f);
                binding.sliderRef->SetValue(val);
                binding.applyValue(val, input);
                UpdateValueTexts();
                break;
            }
        }
    }

    // =========================================================================
    // B. マウス左クリック / Aボタンによるドラッグ操作 (InputManager統合API)
    // =========================================================================
    bool isActionDown = input->IsCursorActionDown();
    bool isActionReleased = input->IsCursorActionReleased();

    if (isActionDown && !isDraggingSlider_) {
        for (const auto& binding : bindings) {
            if (binding.sliderRef && binding.rectRef.Contains(cursorPos.x, cursorPos.y)) {
                isDraggingSlider_ = true;
                draggingSlider_ = binding.sliderRef;
                break;
            }
        }
    }

    // ドラッグ中処理（データ駆動による座標計算・値適用）
    if (isActionDown && isDraggingSlider_ && draggingSlider_) {
        for (const auto& binding : bindings) {
            if (binding.sliderRef == draggingSlider_) {
                float left = binding.rectRef.x - binding.rectRef.halfW;
                float width = binding.rectRef.halfW * 2.0f;
                float newValue = std::clamp((cursorPos.x - left) / width, 0.0f, 1.0f);
                draggingSlider_->SetValue(newValue);
                binding.applyValue(newValue, input);
                UpdateValueTexts();
                break;
            }
        }
    }

    // ドラッグ終了
    if (isActionReleased && isDraggingSlider_) {
        if (draggingSlider_ == sliderSE_) {
            UISound::PlayCursor();
        }
        isDraggingSlider_ = false;
        draggingSlider_ = nullptr;
    }
}

void OptionsScene::UpdateValueTexts() {
    for (const auto& binding : GetSliderBindings()) {
        if (binding.textRef && binding.sliderRef) {
            int percent = static_cast<int>(std::round(binding.sliderRef->GetValue() * 100.0f));
            binding.textRef->SetText(std::to_wstring(percent) + L"%");
        }
    }
}
