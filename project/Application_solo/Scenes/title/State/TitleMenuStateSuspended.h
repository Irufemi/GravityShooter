#pragma once
#include "Scenes/title/State/ITitleMenuState.h"

/**
 * @class TitleMenuStateSuspended
 * @brief 背面休眠ステート（モーダル表示中のメニュー非表示＆入力停止）
 */
class TitleMenuStateSuspended : public ITitleMenuState {
public:
    TitleMenuStateSuspended() = default;
    ~TitleMenuStateSuspended() override = default;

    void Enter(TitleMenuControllerComponent* menu) override;
    void Update(TitleMenuControllerComponent* menu, float dt) override;
    void Exit(TitleMenuControllerComponent* menu) override;

    TitleMenuStateType GetStateType() const override {
        return TitleMenuStateType::Suspended;
    }
};
