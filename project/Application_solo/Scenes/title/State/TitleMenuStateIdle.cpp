#include "Scenes/title/State/TitleMenuStateIdle.h"
#include "Scenes/title/TitleMenuControllerComponent.h"

void TitleMenuStateIdle::Enter(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    menu->SetMenuVisible(true);
    menu->ResetTargetScalesForCurrentIndex();
}

void TitleMenuStateIdle::Update(TitleMenuControllerComponent* menu, float dt) {
    if (!menu) {
        return;
    }
    // スティッククールダウン減衰
    menu->UpdateStickCooldown(dt);
    // ナビゲーション入力処理
    menu->HandleNavigationInput();
    // 決定入力処理
    menu->HandleSelectionInput();
    // ボタンの視覚フィードバック（拡縮・発光アニメーション）
    menu->UpdateButtonVisuals(dt);
}

void TitleMenuStateIdle::Exit(TitleMenuControllerComponent* /*menu*/) {}
