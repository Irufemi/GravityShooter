#pragma once

#include "ITitleLaunchState.h"

/**
 * @class TitleLaunchStateCharge
 * @brief [Phase 1: 蓄勢] 出撃エネルギー充填・重力収束フェーズ
 * @details 0.00s〜0.50s の間に自機の沈み込みと水平整流、ガレキの収束、
 *          BGMフェードアウトおよびチャージSE再生を制御します。
 */
class TitleLaunchStateCharge : public ITitleLaunchState {
public:
    TitleLaunchStateCharge() = default;
    ~TitleLaunchStateCharge() override = default;

    void Enter(TitleSceneDirectorComponent* director) override;
    void Update(TitleSceneDirectorComponent* director, float dt) override;
    TitleLaunchStateType GetStateType() const override {
        return TitleLaunchStateType::Charge;
    }
};
