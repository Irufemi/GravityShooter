#include "Combat/Boss/BossStateIdle.h"
#include "Combat/Boss/BossStateCoreExposed.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/EnemyBeamComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <memory>

void BossStateIdle::Enter(BossComponent* boss) {
    Log::OutPutLog(std::cout, "Boss entered Idle State (Shield Active)\n");
}

void BossStateIdle::Update(BossComponent* boss) {
    if (!boss->gameObject_) {
        return;
    }

    // --- ビーム攻撃のタイマー処理 ---
    if (boss->beamComponent_) {
        float deltaTime = 1.0f / 60.0f;
        if (auto engine = boss->GetEngine()) {
            float dt = engine->GetGameDeltaTime();
            if (dt > 0.0f) {
                deltaTime = dt;
            }
        }

        if (!boss->beamComponent_->IsActive()) {
            boss->beamTimer_ += deltaTime;
            if (boss->beamTimer_ >= boss->beamInterval_) {
                boss->beamTimer_ = 0.0f;

                if (auto myTrans = boss->GetTransform()) {
                    // ボスの前面（プレイヤー側＝ローカル -Z方向）および口元（ローカルY）のオフセット
                    Irufemi::Vector3 localMuzzle = {0.0f, boss->beamOffsetY_, -boss->beamOffsetZ_};
                    Irufemi::Vector3 startPos = Irufemi::Math::Transform(localMuzzle, myTrans->GetWorldMatrix());

                    Irufemi::Vector3 forward = -myTrans->GetWorldForward();
                    Irufemi::Vector3 targetPos =
                        Irufemi::Math::Add(startPos, Irufemi::Math::Multiply(boss->beamRange_, forward));
                    boss->beamComponent_->Fire(startPos, targetPos);
                }
            }
        }
    }

    // CoreExposed への遷移チェック
    if (boss->isShieldsInitialized_ && boss->initialShieldsSpawned_ > 0 && boss->shields_.empty()) {
        boss->ChangeState(std::make_unique<BossStateCoreExposed>());
    }
}

void BossStateIdle::Exit(BossComponent* boss) {}

void BossStateIdle::OnTakeDamage(BossComponent* boss, float damage) {
    Log::OutPutLog(std::cout, "Boss blocked damage with shield!\n");
}
