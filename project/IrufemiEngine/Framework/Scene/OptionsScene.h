#pragma once
#include "Framework/Scene/BaseScene.h"
#include "Core/Math/Vector2.h"
#include <cstdint>
#include <memory>
#include <string>

class SliderComponent;
class ButtonComponent;
class TextRendererComponent;
class Primitive2DRendererComponent;
class GameObject;

/**
 * @class OptionsScene
 * @brief ゲーム内設定(Options)を管理・表示するシーン
 * @details SceneManager::PushScene で呼び出され、背景ゲームをポーズしつつBGMやUI音を維持。
 *          Apex Legends風の仮想カーソル（Virtual Cursor）による左スティック操作とマウス操作のハイブリッド制御に対応します。
 */
class OptionsScene : public BaseScene {
public:
    OptionsScene() = default;
    ~OptionsScene() override = default;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Finalize() override;
    void OnEnter() override;
    void OnExit() override;

    // --- スタック管理用フラグ ---

    // オプション画面を開いている間は下のシーンのUpdateを止める（ゲームをポーズする）
    bool IsUpdateBlocking() const override {
        return true;
    }

    // 背景のゲーム画面は描画し続ける（半透明の背景UIの下に表示させるため）
    bool IsDrawBlocking() const override {
        return false;
    }

    // マウスカーソルは表示する
    bool IsCursorVisible() const override {
        return true;
    }

    /// @brief オーディオをポーズせず継続再生する（BGMやUI音を維持）
    bool IsAudioBlocking() const override {
        return false;
    }

private:
    void BindUIComponents();
    void UpdateVirtualCursor(float deltaTime);
    void UpdateSliderDrag();
    void UpdateValueTexts();
    void PlaySE(const std::string& filePath, const std::string& key, float volume = 0.7f);

    // キャッシュしたUIコンポーネント参照
    SliderComponent* sliderBGM_ = nullptr;
    SliderComponent* sliderSE_ = nullptr;
    SliderComponent* sliderSensitivity_ = nullptr;
    ButtonComponent* buttonClose_ = nullptr;
    TextRendererComponent* valueTextBGM_ = nullptr;
    TextRendererComponent* valueTextSE_ = nullptr;
    TextRendererComponent* valueTextSensitivity_ = nullptr;

    // 仮想カーソルGameObject参照
    std::shared_ptr<GameObject> virtualCursorObj_;
    Primitive2DRendererComponent* virtualCursorRenderer_ = nullptr;

    // ドラッグ状態
    bool isDraggingSlider_ = false;
    SliderComponent* draggingSlider_ = nullptr;

    // ホバー検出＆SE用
    void* lastHoveredTarget_ = nullptr;

    // カーソル設定定数
    const float kStickyFriction_ = 0.45f;   // ホバー時の減速倍率

    // 開いた直後の入力ガードタイマー
    float openCooldownTimer_ = 0.2f;

    // UIの初期化が完了したか
    bool uiBound_ = false;
};
