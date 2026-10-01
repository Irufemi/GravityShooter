#pragma once
#include "Framework/Scene/BaseScene.h"
#include <cstdint>

/**
 * @class OptionsScene
 * @brief ゲーム内設定(Options)を管理・表示するシーン
 * @details SceneManager::PushScene で呼び出されることを想定し、
 *          背景ゲームをポーズしつつ、BGMやUI音は再生し続けるUXを提供します。
 */
class OptionsScene : public BaseScene {
public:
    OptionsScene() = default;
    ~OptionsScene() override = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(IrufemiEngine* engine) override;

    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief Finalize を実行する。
     */
    void Finalize() override;

    /**
     * @brief OnEnter を実行する。
     */
    void OnEnter() override;

    /**
     * @brief OnExit を実行する。
     */
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
    void ApplyPendingSettings();
    void RevertSettings();

    // 内部状態（保留反映用）
    int pendingResolutionIndex_ = -1;
    bool pendingFullscreen_ = false;

    // UIの初期化が完了したか
    bool uiBound_ = false;
};
