#include "Framework/Scene/OptionsScene.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/UI/ButtonComponent.h"
#include "Framework/Component/UI/SliderComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Utility/CVar.h"
#include "Audio/AudioManager.h"
#include "Audio/Sound.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Utility/Log.h"

void OptionsScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // Prefab（実際はシーン形式）からUI要素をロード
    if (!SceneSerializer::Load(this, "OptionsUI.prefab")) {
        Log::OutPutLog(std::cerr, "[OptionsScene] Warning: Failed to load 'OptionsUI.prefab'\n");
    }

    uiBound_ = false;
}

void OptionsScene::OnEnter() {
    BaseScene::OnEnter();
}

void OptionsScene::Update() {
    BaseScene::Update();

    if (!uiBound_) {
        BindUIComponents();
        uiBound_ = true;
    }
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

    // UIを閉じるボタン用コールバック
    auto closeScene = [engine]() {
        if (auto sm = engine->GetSceneManager()) {
            sm->PopScene();
        }
    };

    // Apply ボタン
    if (auto obj = FindGameObject("Button_Apply")) {
        if (auto btn = obj->GetComponent<ButtonComponent>()) {
            btn->SetOnClickCallback([this, closeScene]() {
                ApplyPendingSettings();
                closeScene();
            });
        }
    }

    // Close / Cancel ボタン
    if (auto obj = FindGameObject("Button_Cancel")) {
        if (auto btn = obj->GetComponent<ButtonComponent>()) {
            btn->SetOnClickCallback(closeScene);
        }
    } else if (auto objClose = FindGameObject("Button_Close")) {
        if (auto btn = objClose->GetComponent<ButtonComponent>()) {
            btn->SetOnClickCallback(closeScene);
        }
    }

    // BGM スライダー
    if (auto obj = FindGameObject("Slider_BGM")) {
        if (auto slider = obj->GetComponent<SliderComponent>()) {
            slider->SetValue(Irufemi::CVarSystem::GetFloat("a.MasterVolume")); // 代替としてMasterVolumeを使用
            slider->SetOnValueChangedCallback([](float val) { Irufemi::CVarSystem::SetFloat("a.MasterVolume", val); });
        }
    }

    // SE スライダー
    if (auto obj = FindGameObject("Slider_SE")) {
        if (auto slider = obj->GetComponent<SliderComponent>()) {
            slider->SetValue(1.0f); // ダミー
            slider->SetOnValueChangedCallback([](float val) {
                // SE用音量の保存先があれば設定
            });
        }
    }
}

void OptionsScene::ApplyPendingSettings() {
    // 保留設定を適用
}

void OptionsScene::RevertSettings() {
    // キャンセル時
}
