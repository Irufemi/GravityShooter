#pragma once
#include "Scenes/title/State/ITitleMenuState.h"

/**
 * @class TitleMenuStateOpeningModal
 * @brief モーダル決定演出ステート（Click Punch・フェード・PushScene発火）
 */
class TitleMenuStateOpeningModal : public ITitleMenuState {
public:
    TitleMenuStateOpeningModal() = default;
    ~TitleMenuStateOpeningModal() override = default;

    void Enter(TitleMenuControllerComponent* menu) override;
    void Update(TitleMenuControllerComponent* menu, float dt) override;
    void Exit(TitleMenuControllerComponent* menu) override;

    TitleMenuStateType GetStateType() const override {
        return TitleMenuStateType::OpeningModal;
    }
};
