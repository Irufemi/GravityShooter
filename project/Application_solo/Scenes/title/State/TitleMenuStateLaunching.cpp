#include "Scenes/title/State/TitleMenuStateLaunching.h"
#include "Scenes/title/TitleMenuControllerComponent.h"

void TitleMenuStateLaunching::Enter(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    // 白飛びフラッシュを停止し、暗黒宇宙とスラスター噴流・流体のコントラストを死守
    // menu->TriggerScreenFlash();
    menu->StartDismissAnimation();
    menu->TriggerLaunchSequence();
}

void TitleMenuStateLaunching::Update(TitleMenuControllerComponent* menu, float dt) {
    if (!menu) {
        return;
    }
    menu->UpdateDismissAnimation(dt);
}

void TitleMenuStateLaunching::Exit(TitleMenuControllerComponent* /*menu*/) {}
