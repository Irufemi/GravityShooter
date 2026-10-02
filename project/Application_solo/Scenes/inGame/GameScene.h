#pragma once

#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include <memory>

class IrufemiEngine;

/**
 * @class GameScene
 * @brief ゲーム本編を管理するクラス
 */
class GameScene : public BaseScene {
public: // メンバ関数(システム)
    ~GameScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
    void OnSuspend() override;
    void OnResume() override;

private:
    void SetHUDActive(bool active);
};