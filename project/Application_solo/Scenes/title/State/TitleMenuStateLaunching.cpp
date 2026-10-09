#include "Scenes/title/State/TitleMenuStateLaunching.h"
#include "Scenes/title/TitleMenuControllerComponent.h"

void TitleMenuStateLaunching::Enter(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    menu->TriggerScreenFlash();
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
