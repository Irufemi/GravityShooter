#pragma once

#include "ITitleLaunchState.h"

/**
 * @class TitleLaunchStateAfterglow
 * @brief [Phase 4: 余韻] 残光・静寂・暗転フェーズ
 * @details 2.20s〜3.20s の間に自機消失後の静寂の中でガレキを整流し、
 *          InGame シーンへのフェードアウト遷移を発火します。
 */
class TitleLaunchStateAfterglow : public ITitleLaunchState {
public:
    TitleLaunchStateAfterglow() = default;
    ~TitleLaunchStateAfterglow() override = default;

    void Enter(TitleSceneDirectorComponent* director) override;
    void Update(TitleSceneDirectorComponent* director, float dt) override;
    TitleLaunchStateType GetStateType() const override {
        return TitleLaunchStateType::Afterglow;
    }
};
