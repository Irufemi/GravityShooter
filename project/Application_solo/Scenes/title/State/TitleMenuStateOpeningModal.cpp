#include "Scenes/title/State/TitleMenuStateOpeningModal.h"
#include "Scenes/title/TitleMenuControllerComponent.h"
#include "UI/UISound.h"

void TitleMenuStateOpeningModal::Enter(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    menu->ResetStateTimer();
    UISound::PlayDecide();
}

void TitleMenuStateOpeningModal::Update(TitleMenuControllerComponent* menu, float dt) {
    if (!menu) {
        return;
    }
    menu->UpdateOpeningModalAnimation(dt);
}

void TitleMenuStateOpeningModal::Exit(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    // モーダル決定アニメーション（Click Punch + フェード）完全完了の瞬間に PushScene を発火
    menu->PushPendingModalScene();
}
