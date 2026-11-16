#pragma once

#include "Framework/Scene/BaseScene.h"

class IrufemiEngine;

/**
 * @class GameOverScene
 * @brief ゲームオーバー画面を管理するクラス
 */
class GameOverScene : public Irufemi::BaseScene {
public: // メンバ関数(システム)
    ~GameOverScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
};
