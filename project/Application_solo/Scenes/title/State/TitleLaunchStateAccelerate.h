#pragma once

#include "ITitleLaunchState.h"

/**
 * @class TitleLaunchStateAccelerate
 * @brief [Phase 2: 咆哮] アフターバーナー点火・急加速・動的FOVフェーズ
 * @details 0.50s〜1.60s の間に臨界蓄圧（マイクロフリーズ・身震い）、
 *          臨界解放マイルストーン発火、自機の4次急加速突進および動的FOVを制御します。
 */
class TitleLaunchStateAccelerate : public ITitleLaunchState {
public:
    TitleLaunchStateAccelerate() = default;
    ~TitleLaunchStateAccelerate() override = default;

    void Enter(TitleSceneDirectorComponent* director) override;
    void Update(TitleSceneDirectorComponent* director, float dt) override;
    TitleLaunchStateType GetStateType() const override {
        return TitleLaunchStateType::Accelerate;
    }
};
