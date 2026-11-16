#pragma once

#include "Framework/Scene/BaseScene.h"

class IrufemiEngine;

/**
 * @class PauseScene
 * @brief ポーズ画面を管理するクラス
 */
class PauseScene : public Irufemi::BaseScene {
public: // メンバ関数(システム)
    ~PauseScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
};
