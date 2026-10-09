#include "Scenes/title/State/TitleMenuStateSuspended.h"
#include "Scenes/title/TitleMenuControllerComponent.h"

void TitleMenuStateSuspended::Enter(TitleMenuControllerComponent* menu) {
    if (!menu) {
        return;
    }
    menu->SetMenuVisible(false);
    menu->HideVirtualCursor();
}

void TitleMenuStateSuspended::Update(TitleMenuControllerComponent* /*menu*/, float /*dt*/) {
    // 背面待機中は完全休眠（Update処理を行わない）
}

void TitleMenuStateSuspended::Exit(TitleMenuControllerComponent* /*menu*/) {}
