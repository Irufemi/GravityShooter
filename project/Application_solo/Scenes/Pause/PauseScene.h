#pragma once

#include "Framework/Scene/BaseScene.h"
#include "Core/Math/Vector2.h"
#include <string>
#include <vector>
#include <memory>

class TextRendererComponent;
class SpriteRendererComponent;
class GameObject;

/**
 * @class PauseScene
 * @brief ゲーム本編の一時停止画面を管理するシーン
 * @details SceneManager::PushScene で呼び出され、背景ゲームを静止・描画継続しつつ、
 *          半透明暗幕オーバーレイ・各種メニュー（Resume/Retry/Options/Title）を提供します。
 *          キーボード、マウス、ゲームパッドのハイブリッド入力に対応します。
 */
class PauseScene : public BaseScene {
public:
    enum class MenuItem { Resume = 0, Retry, Options, Title, Quit, Count };

    PauseScene();
    ~PauseScene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;
    void OnSuspend() override;
    void OnResume() override;

    // --- スタック管理用設定 ---
    bool IsUpdateBlocking() const override {
        return true;
    }
    bool IsDrawBlocking() const override {
        return false;
    }
    bool IsCursorVisible() const override {
        return true;
    }
    bool IsAudioBlocking() const override {
        return false;
    }
    bool ShouldClearParticlesOnPop() const override {
        return false;
    }

private:
    void CreateUIElements();
    void UpdateInput(float deltaTime);
    void UpdateSelectionVisuals(float deltaTime);
    void ExecuteAction(MenuItem item);
    void SetUIVisible(bool visible);

    /**
     * @brief カーソル座標が指定インデックスのメニュー項目の幾何領域内（動的スケール・バウンディングボックス反映）にあるか判定する
     * @param[in] index 項目インデックス
     * @param[in] cursorPos 判定するカーソル座標
     * @return 領域内にある場合は true
     */
    bool IsCursorOverItem(int index, const Irufemi::Vector2& cursorPos) const;

    bool isSuspended_ = false;

    struct ItemData {
        std::wstring text;
        MenuItem itemType;
        float yPos = 0.0f;
        std::shared_ptr<GameObject> gameObject;
        TextRendererComponent* textComp = nullptr;
        float currentScale = 1.0f;
    };

    std::vector<ItemData> menuItems_;
    int selectedIndex_ = 0;
    int lastHoveredIndex_ = -1;
    int pressedItemIndex_ = -1;

    float openCooldownTimer_ = 0.2f;
    float originalBGMVolume_ = 1.0f;

    std::shared_ptr<GameObject> darkOverlayObj_;
    std::shared_ptr<GameObject> titleObj_;
};
