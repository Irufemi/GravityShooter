#include "Framework/Scene/OptionsScene.h"
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
#include <algorithm>
#include <cmath>

namespace {
// 感度マッピング定数（250.0f 〜 1250.0f、基準 650.0f = 0.4x）
constexpr float kMinCursorSpeed = 250.0f;
constexpr float kMaxCursorSpeed = 1250.0f;
} // namespace

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
}

void OptionsScene::OnEnter() {
    BaseScene::OnEnter();
    openCooldownTimer_ = 0.15f;
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
    if (openCooldownTimer_ > 0.0f) {
        openCooldownTimer_ -= dt;
        return; // 開いた直後のクリック・ボタン入力残存による即時クローズをガード
    }

    // =========================================================================
    // 1. 即時脱出判定 (Bボタン / ESC / BackSpace)
    // =========================================================================
    if (input->IsCancelPressed()) {
        PlaySE(seCancelPath_, "se_menu_cancel", 0.7f);
        if (auto sm = engine->GetSceneManager()) {
            sm->PopScene();
        }
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

    bool isDecidePressed =
        input->IsCursorActionPressed() || input->IsKeyPressed(VK_SPACE) || input->IsKeyPressed(VK_RETURN);

    if (isOverCloseButton && isDecidePressed && !isDraggingSlider_) {
        PlaySE(seDecidePath_, "se_menu_decide", 0.9f);
        if (auto sm = engine->GetSceneManager()) {
            sm->PopScene();
        }
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

    // BGM スライダー
    if (auto obj = FindGameObject("Slider_BGM")) {
        sliderBGM_ = obj->GetComponent<SliderComponent>();
        if (auto t = obj->GetTransform()) {
            rectSliderBGM_.x = t->GetPosition().x;
            rectSliderBGM_.y = t->GetPosition().y;
        }
        if (sliderBGM_) {
            float bgmVol = Irufemi::CVarSystem::GetFloat("a.BGMVolume");
            sliderBGM_->SetValue(bgmVol);
        }
    }

    // SE スライダー
    if (auto obj = FindGameObject("Slider_SE")) {
        sliderSE_ = obj->GetComponent<SliderComponent>();
        if (auto t = obj->GetTransform()) {
            rectSliderSE_.x = t->GetPosition().x;
            rectSliderSE_.y = t->GetPosition().y;
        }
        if (sliderSE_) {
            float seVol = Irufemi::CVarSystem::GetFloat("a.SEVolume");
            sliderSE_->SetValue(seVol);
        }
    }

    // SENSITIVITY スライダー
    if (auto obj = FindGameObject("Slider_Sensitivity")) {
        sliderSensitivity_ = obj->GetComponent<SliderComponent>();
        if (auto t = obj->GetTransform()) {
            rectSliderSensitivity_.x = t->GetPosition().x;
            rectSliderSensitivity_.y = t->GetPosition().y;
        }
        if (sliderSensitivity_) {
            float speed = Irufemi::CVarSystem::GetFloat("i.CursorSpeed");
            if (speed <= 0.0f) {
                speed = 650.0f;
            }
            float val = std::clamp((speed - kMinCursorSpeed) / (kMaxCursorSpeed - kMinCursorSpeed), 0.0f, 1.0f);
            sliderSensitivity_->SetValue(val);
        }
    }

    // CLOSE ボタン
    if (auto obj = FindGameObject("Button_Close")) {
        buttonClose_ = obj->GetComponent<ButtonComponent>();
        if (auto t = obj->GetTransform()) {
            rectButtonClose_.x = t->GetPosition().x;
            rectButtonClose_.y = t->GetPosition().y;
        }
    }

    // 数値テキスト表示
    if (auto obj = FindGameObject("ValueText_BGM")) {
        valueTextBGM_ = obj->GetComponent<TextRendererComponent>();
    }
    if (auto obj = FindGameObject("ValueText_SE")) {
        valueTextSE_ = obj->GetComponent<TextRendererComponent>();
    }
    if (auto obj = FindGameObject("ValueText_Sensitivity")) {
        valueTextSensitivity_ = obj->GetComponent<TextRendererComponent>();
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

    // 1. ホバー判定
    void* currentHovered = nullptr;
    bool isOverClose = rectButtonClose_.Contains(cursorPos.x, cursorPos.y);
    bool isOverBgm = rectSliderBGM_.Contains(cursorPos.x, cursorPos.y);
    bool isOverSe = rectSliderSE_.Contains(cursorPos.x, cursorPos.y);
    bool isOverSens = rectSliderSensitivity_.Contains(cursorPos.x, cursorPos.y);

    if (isOverBgm) {
        currentHovered = sliderBGM_;
    } else if (isOverSe) {
        currentHovered = sliderSE_;
    } else if (isOverSens) {
        currentHovered = sliderSensitivity_;
    } else if (isOverClose) {
        currentHovered = buttonClose_;
    }

    // ホバー対象が変わった瞬間にカーソルSE再生
    if (currentHovered != lastHoveredTarget_) {
        if (currentHovered != nullptr) {
            PlaySE(seCursorPath_, "se_menu_cursor", 0.5f);
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

    bool isOverBgm = rectSliderBGM_.Contains(cursorPos.x, cursorPos.y);
    bool isOverSe = rectSliderSE_.Contains(cursorPos.x, cursorPos.y);
    bool isOverSens = rectSliderSensitivity_.Contains(cursorPos.x, cursorPos.y);

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
        if (isOverBgm && sliderBGM_) {
            float val = std::clamp(sliderBGM_->GetValue() + directAdjust, 0.0f, 1.0f);
            sliderBGM_->SetValue(val);
            Irufemi::CVarSystem::SetFloat("a.BGMVolume", val);
            Irufemi::CVarSystem::SetFloat("a.MasterVolume", val);
            UpdateValueTexts();
        } else if (isOverSe && sliderSE_) {
            float val = std::clamp(sliderSE_->GetValue() + directAdjust, 0.0f, 1.0f);
            sliderSE_->SetValue(val);
            Irufemi::CVarSystem::SetFloat("a.SEVolume", val);
            UpdateValueTexts();
        } else if (isOverSens && sliderSensitivity_) {
            float val = std::clamp(sliderSensitivity_->GetValue() + directAdjust, 0.0f, 1.0f);
            sliderSensitivity_->SetValue(val);
            float newSpeed = kMinCursorSpeed + val * (kMaxCursorSpeed - kMinCursorSpeed);
            Irufemi::CVarSystem::SetFloat("i.CursorSpeed", newSpeed);
            input->SetVirtualCursorBaseSpeed(newSpeed);
            UpdateValueTexts();
        }
    }

    // =========================================================================
    // B. マウス左クリック / Aボタンによるドラッグ操作 (InputManager統合API)
    // =========================================================================
    bool isActionDown = input->IsCursorActionDown();
    bool isActionReleased = input->IsCursorActionReleased();

    if (isActionDown && !isDraggingSlider_) {
        if (isOverBgm && sliderBGM_) {
            isDraggingSlider_ = true;
            draggingSlider_ = sliderBGM_;
        } else if (isOverSe && sliderSE_) {
            isDraggingSlider_ = true;
            draggingSlider_ = sliderSE_;
        } else if (isOverSens && sliderSensitivity_) {
            isDraggingSlider_ = true;
            draggingSlider_ = sliderSensitivity_;
        }
    }

    // ドラッグ中処理
    if (isActionDown && isDraggingSlider_ && draggingSlider_) {
        float left = rectSliderBGM_.x - rectSliderBGM_.halfW;
        float width = rectSliderBGM_.halfW * 2.0f;
        if (draggingSlider_ == sliderBGM_) {
            left = rectSliderBGM_.x - rectSliderBGM_.halfW;
            width = rectSliderBGM_.halfW * 2.0f;
        } else if (draggingSlider_ == sliderSE_) {
            left = rectSliderSE_.x - rectSliderSE_.halfW;
            width = rectSliderSE_.halfW * 2.0f;
        } else if (draggingSlider_ == sliderSensitivity_) {
            left = rectSliderSensitivity_.x - rectSliderSensitivity_.halfW;
            width = rectSliderSensitivity_.halfW * 2.0f;
        }

        float newValue = (cursorPos.x - left) / width;
        newValue = std::clamp(newValue, 0.0f, 1.0f);
        draggingSlider_->SetValue(newValue);

        if (draggingSlider_ == sliderBGM_) {
            Irufemi::CVarSystem::SetFloat("a.BGMVolume", newValue);
            Irufemi::CVarSystem::SetFloat("a.MasterVolume", newValue);
        } else if (draggingSlider_ == sliderSE_) {
            Irufemi::CVarSystem::SetFloat("a.SEVolume", newValue);
        } else if (draggingSlider_ == sliderSensitivity_) {
            float newSpeed = kMinCursorSpeed + newValue * (kMaxCursorSpeed - kMinCursorSpeed);
            Irufemi::CVarSystem::SetFloat("i.CursorSpeed", newSpeed);
            input->SetVirtualCursorBaseSpeed(newSpeed);
        }
        UpdateValueTexts();
    }

    // ドラッグ終了
    if (isActionReleased && isDraggingSlider_) {
        if (draggingSlider_ == sliderSE_) {
            PlaySE(seCursorPath_, "se_menu_cursor", 0.7f);
        }
        isDraggingSlider_ = false;
        draggingSlider_ = nullptr;
    }
}

void OptionsScene::UpdateValueTexts() {
    if (valueTextBGM_ && sliderBGM_) {
        int percent = static_cast<int>(std::round(sliderBGM_->GetValue() * 100.0f));
        valueTextBGM_->SetText(std::to_wstring(percent) + L"%");
    }

    if (valueTextSE_ && sliderSE_) {
        int percent = static_cast<int>(std::round(sliderSE_->GetValue() * 100.0f));
        valueTextSE_->SetText(std::to_wstring(percent) + L"%");
    }

    if (valueTextSensitivity_ && sliderSensitivity_) {
        int percent = static_cast<int>(std::round(sliderSensitivity_->GetValue() * 100.0f));
        valueTextSensitivity_->SetText(std::to_wstring(percent) + L"%");
    }
}

void OptionsScene::PlaySE(const std::string& filePath, const std::string& key, float volume) {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto audioManager = engine->GetAudioManager();
    if (!audioManager) {
        return;
    }

    auto sound = audioManager->GetOrLoadSoundByFile(filePath, key);
    if (!sound) {
        return;
    }

    float seVolumeMultiplier = Irufemi::CVarSystem::GetFloat("a.SEVolume");
    float finalVolume = std::clamp(volume * seVolumeMultiplier, 0.0f, 1.0f);

    audioManager->Play(sound, false, finalVolume, AudioCategory::UI);
}
