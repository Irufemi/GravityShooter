#pragma once

#include "Framework/Scene/BaseScene.h"

class IrufemiEngine;

/**
 * @class TitleScene
 * @brief タイトル画面を管理するクラス
 */
class TitleScene : public Irufemi::BaseScene {
public: // メンバ関数(システム)
    ~TitleScene() override;

    /**
     * @brief 初期化処理
     * @param engine IrufemiEngineのポインタ
     */
    void Initialize(IrufemiEngine* engine) override;

    /**
     * @brief 毎フレームの更新処理
     */
    void Update() override;

    /**
     * @brief 描画処理
     */
    void Draw() override;
};