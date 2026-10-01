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
// UI各要素の画面上レイアウト定数 (1280x720 空間)
constexpr float kButtonCloseX = 640.0f;
constexpr float kButtonCloseY = 510.0f;
constexpr float kButtonCloseHalfW = 130.0f;
constexpr float kButtonCloseHalfH = 30.0f;

constexpr float kSliderBgmX = 640.0f;
constexpr float kSliderBgmY = 280.0f;
constexpr float kSliderSeX = 640.0f;
constexpr float kSliderSeY = 390.0f;
constexpr float kSliderHalfW = 210.0f;
constexpr float kSliderHalfH = 35.0f; // 当たり判定を縦幅70pxに拡大して操作性を向上
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
    }

    // =========================================================================
    // 1. 即時脱出判定 (Bボタン / ESC / BackSpace)
    // =========================================================================
    if (input->IsCancelPressed()) {
        PlaySE("resources/audio/se_menu_cancel.wav", "se_menu_cancel", 0.7f);
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
    if (openCooldownTimer_ <= 0.0f) {
        bool isOverCloseButton = (std::abs(cursorPos.x - kButtonCloseX) <= kButtonCloseHalfW &&
                                  std::abs(cursorPos.y - kButtonCloseY) <= kButtonCloseHalfH);

        bool isDecidePressed = input->IsCursorActionPressed() || input->IsKeyPressed(VK_SPACE) ||
                               input->IsKeyPressed(VK_RETURN);

        if (isOverCloseButton && isDecidePressed && !isDraggingSlider_) {
            PlaySE("resources/audio/se_menu_decide.wav", "se_menu_decide", 0.9f);
            if (auto sm = engine->GetSceneManager()) {
                sm->PopScene();
            }
            return;
        }
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
        if (sliderBGM_) {
            float bgmVol = Irufemi::CVarSystem::GetFloat("a.BGMVolume");
            sliderBGM_->SetValue(bgmVol);
        }
    }

    // SE スライダー
    if (auto obj = FindGameObject("Slider_SE")) {
        sliderSE_ = obj->GetComponent<SliderComponent>();
        if (sliderSE_) {
            float seVol = Irufemi::CVarSystem::GetFloat("a.SEVolume");
            sliderSE_->SetValue(seVol);
        }
    }

    // CLOSE ボタン
    if (auto obj = FindGameObject("Button_Close")) {
        buttonClose_ = obj->GetComponent<ButtonComponent>();
    }

    // 数値テキスト表示
    if (auto obj = FindGameObject("ValueText_BGM")) {
        valueTextBGM_ = obj->GetComponent<TextRendererComponent>();
    }
    if (auto obj = FindGameObject("ValueText_SE")) {
        valueTextSE_ = obj->GetComponent<TextRendererComponent>();
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
    bool isOverClose = (std::abs(cursorPos.x - kButtonCloseX) <= kButtonCloseHalfW &&
                        std::abs(cursorPos.y - kButtonCloseY) <= kButtonCloseHalfH);
    bool isOverBgm = (std::abs(cursorPos.x - kSliderBgmX) <= kSliderHalfW &&
                      std::abs(cursorPos.y - kSliderBgmY) <= kSliderHalfH);
    bool isOverSe = (std::abs(cursorPos.x - kSliderSeX) <= kSliderHalfW &&
                     std::abs(cursorPos.y - kSliderSeY) <= kSliderHalfH);

    if (isOverBgm) {
        currentHovered = sliderBGM_;
    } else if (isOverSe) {
        currentHovered = sliderSE_;
    } else if (isOverClose) {
        currentHovered = buttonClose_;
    }

    // ホバー対象が変わった瞬間にカーソルSE再生
    if (currentHovered != lastHoveredTarget_) {
        if (currentHovered != nullptr) {
            PlaySE("resources/audio/se_menu_cursor.wav", "se_menu_cursor", 0.5f);
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
                virtualCursorRenderer_->SetColor((currentHovered != nullptr) ? Irufemi::Vector4{0.2f, 1.0f, 0.95f, 1.0f}
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

    bool isOverBgm = (std::abs(cursorPos.x - kSliderBgmX) <= kSliderHalfW &&
                      std::abs(cursorPos.y - kSliderBgmY) <= kSliderHalfH);
    bool isOverSe = (std::abs(cursorPos.x - kSliderSeX) <= kSliderHalfW &&
                     std::abs(cursorPos.y - kSliderSeY) <= kSliderHalfH);

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
        }
    }

    // =========================================================================
    // B. マウス左クリック / Aボタンによるドラッグ操作 (InputManager統合API)
    // =========================================================================
    bool isActionDown = input->IsCursorActionDown();
    bool isActionReleased = input->IsCursorActionReleased();

    // ガード期間終了後にドラッグ開始を受け付ける
    if (openCooldownTimer_ <= 0.0f) {
        if (isActionDown && !isDraggingSlider_) {
            if (isOverBgm && sliderBGM_) {
                isDraggingSlider_ = true;
                draggingSlider_ = sliderBGM_;
            } else if (isOverSe && sliderSE_) {
                isDraggingSlider_ = true;
                draggingSlider_ = sliderSE_;
            }
        }
    }

    // ドラッグ中処理
    if (isActionDown && isDraggingSlider_ && draggingSlider_) {
        float left = (draggingSlider_ == sliderBGM_) ? (kSliderBgmX - kSliderHalfW) : (kSliderSeX - kSliderHalfW);
        float width = kSliderHalfW * 2.0f;

        float newValue = (cursorPos.x - left) / width;
        newValue = std::clamp(newValue, 0.0f, 1.0f);

        draggingSlider_->SetValue(newValue);

        if (draggingSlider_ == sliderBGM_) {
            Irufemi::CVarSystem::SetFloat("a.BGMVolume", newValue);
            Irufemi::CVarSystem::SetFloat("a.MasterVolume", newValue);
        } else if (draggingSlider_ == sliderSE_) {
            Irufemi::CVarSystem::SetFloat("a.SEVolume", newValue);
        }
        UpdateValueTexts();
    }

    // ドラッグ終了
    if (isActionReleased && isDraggingSlider_) {
        if (draggingSlider_ == sliderSE_) {
            PlaySE("resources/audio/se_menu_cursor.wav", "se_menu_cursor", 0.7f);
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
