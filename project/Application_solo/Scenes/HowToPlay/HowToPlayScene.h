#pragma once

#include "Framework/Scene/BaseScene.h"

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
};
