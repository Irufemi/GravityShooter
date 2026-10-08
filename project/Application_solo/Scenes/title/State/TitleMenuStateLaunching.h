#pragma once
#include "Scenes/title/State/ITitleMenuState.h"

/**
 * @class TitleMenuStateLaunching
 * @brief 出撃演出ステート（白光フラッシュ＆ディスミス拡散 ➔ InGame遷移）
 */
class TitleMenuStateLaunching : public ITitleMenuState {
public:
    TitleMenuStateLaunching() = default;
    ~TitleMenuStateLaunching() override = default;

    void Enter(TitleMenuControllerComponent* menu) override;
    void Update(TitleMenuControllerComponent* menu, float dt) override;
    void Exit(TitleMenuControllerComponent* menu) override;

    TitleMenuStateType GetStateType() const override {
        return TitleMenuStateType::Launching;
    }
};
