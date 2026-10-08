#pragma once

#include "Framework/Scene/BaseScene.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

class IrufemiEngine;

/**
 * @class HowToPlayScene
 * @brief 操作説明(How To Play)をスタック重畳表示するシーン
 * @details SceneManager::PushScene で呼び出され、背景のタイトル画面を描画し続けたまま
 *          半透明オーバーレイでキーボード/パッドの操作図解を表示し、B/ESC/クリックで PopScene します。
 */
class HowToPlayScene : public BaseScene {
public: // メンバ関数(システム)
    HowToPlayScene() = default;
    ~HowToPlayScene() override = default;

    /**
     * @brief 初期化処理
     * @param engine IrufemiEngineのポインタ
     */
    void Initialize(IrufemiEngine* engine) override;

    /**
     * @brief 毎フレーム更新処理
     */
    void Update() override;

    /**
     * @brief 描画処理
     */
    void Draw() override;

    // --- スタック管理用フラグ ---

    // 開いている間は下のシーンのUpdateを止める
    bool IsUpdateBlocking() const override {
        return true;
    }

    // 背景のタイトル画面は描画し続ける（半透明オーバーレイの下に美しい3D背景を表示）
    bool IsDrawBlocking() const override {
        return false;
    }

    // マウスカーソルは表示する
    bool IsCursorVisible() const override {
        return true;
    }

    // タイトルBGMは止めない
    bool IsAudioBlocking() const override {
        return false;
    }

    /**
     * @brief シーン開始時処理 (スタック積載時)
     */
    void OnEnter() override;

private:
    enum class TransitionState {
        Opening,
        Open,
        Closing
    };

    struct UIElementState {
        std::shared_ptr<GameObject> obj;
        Irufemi::Vector3 basePos{};
        Irufemi::Vector3 baseScale{};
        Irufemi::Vector4 baseColor{};
        bool isSprite = false;
        bool isBackdrop = false;
    };

    void CacheUIElements();
    void ApplyTransition(float progress);

    TransitionState state_ = TransitionState::Opening;
    float transitionTimer_ = 0.0f;
    static constexpr float kOpenDuration_ = 0.16f;
    static constexpr float kCloseDuration_ = 0.12f;
    float openCooldownTimer_ = 0.0f; //!< 前画面からの入力残存防止用タイマー
    bool uiCached_ = false;
    std::vector<UIElementState> cachedElements_;
};

