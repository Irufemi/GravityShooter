#pragma once

#include "Framework/Scene/BaseScene.h"

class IrufemiEngine;

/**
 * @class ClearScene
 * @brief クリア画面を管理するクラス
 */
class ClearScene : public Irufemi::BaseScene {
public: // メンバ関数(システム)
    ~ClearScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
};
