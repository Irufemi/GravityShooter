#pragma once
#include "Scenes/title/State/ITitleMenuState.h"

/**
 * @class TitleMenuStateIdle
 * @brief 通常待機ステート（ホバー・ナビゲーション入力受付中）
 */
class TitleMenuStateIdle : public ITitleMenuState {
public:
    TitleMenuStateIdle() = default;
    ~TitleMenuStateIdle() override = default;

    void Enter(TitleMenuControllerComponent* menu) override;
    void Update(TitleMenuControllerComponent* menu, float dt) override;
    void Exit(TitleMenuControllerComponent* menu) override;

    TitleMenuStateType GetStateType() const override {
        return TitleMenuStateType::Idle;
    }
};
