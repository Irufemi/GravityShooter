#pragma once

#include "ITitleLaunchState.h"

/**
 * @class TitleLaunchStateBreak
 * @brief [Phase 3: 突破] 超光速離脱・消滅フェーズ
 * @details 1.60s〜2.20s の間に自機が超高速で画面奥へ離脱し、
 *          スケールを光の点へ縮小させながらFOVを通常値へ復帰させます。
 */
class TitleLaunchStateBreak : public ITitleLaunchState {
public:
    TitleLaunchStateBreak() = default;
    ~TitleLaunchStateBreak() override = default;

    void Enter(TitleSceneDirectorComponent* director) override {
        (void)director;
    }
    void Update(TitleSceneDirectorComponent* director, float dt) override;
    TitleLaunchStateType GetStateType() const override {
        return TitleLaunchStateType::Break;
    }
};
