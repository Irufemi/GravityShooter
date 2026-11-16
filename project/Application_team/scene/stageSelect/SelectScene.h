#pragma once

#include "Framework/Scene/BaseScene.h"

class IrufemiEngine;

/**
 * @class SelectScene
 * @brief セレクト画面を管理するクラス
 */
class SelectScene : public Irufemi::BaseScene {
public: // メンバ関数(システム)
    ~SelectScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
};
