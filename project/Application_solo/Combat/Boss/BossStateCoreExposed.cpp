#include "Combat/Boss/BossStateCoreExposed.h"
#include "Combat/Boss/BossStateDestroyed.h"
#include "Combat/Boss/BossComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <memory>
#include <string>

EffectMaskComponent* BossStateCoreExposed::GetCoreEffectMask(BossComponent* boss) {
    if (!boss || !boss->GetGameObject()) {
        return nullptr;
    }
    if (auto scene = boss->GetGameObject()->GetScene()) {
        if (auto coreObj = scene->FindGameObject("BossCore")) {
            return coreObj->GetComponent<EffectMaskComponent>();
        }
    }
    return nullptr;
}

void BossStateCoreExposed::Enter(BossComponent* boss) {
    Log::OutPutLog(std::cout, "Boss entered CoreExposed State (Vulnerable)\n");

    // コア露出（弱点露出）演出：超高輝度ゴールド [1.5, 1.2, 0.2, 1.0] ＆ 太線 (3.0) で強調
    if (auto coreMask = GetCoreEffectMask(boss)) {
        if (!hasCachedOriginal_) {
            originalCoreColor_ = coreMask->GetCustomParams().color1;
            originalThickness_ = coreMask->GetCustomParams().param1;
            hasCachedOriginal_ = true;
        }
        auto params = coreMask->GetCustomParams();
        params.color1 = Irufemi::Vector4{1.5f, 1.2f, 0.2f, 1.0f}; // 超高輝度ゴールド
        params.param1 = 3.0f;                                     // 太線強調
        coreMask->SetCustomParams(params);
    }
}

void BossStateCoreExposed::Update(BossComponent* boss) {
    // 被弾ヒットフラッシュタイマーの減衰
    if (hitFlashTimer_ > 0.0f) {
        float dt = (boss && boss->GetEngine()) ? boss->GetEngine()->GetGameDeltaTime() : (1.0f / 60.0f);
        hitFlashTimer_ -= dt;
        if (hitFlashTimer_ <= 0.0f) {
            hitFlashTimer_ = 0.0f;
            // ゴールドに復帰
            if (auto coreMask = GetCoreEffectMask(boss)) {
                auto params = coreMask->GetCustomParams();
                params.color1 = Irufemi::Vector4{1.5f, 1.2f, 0.2f, 1.0f};
                coreMask->SetCustomParams(params);
            }
        }
    }
}

void BossStateCoreExposed::Exit(BossComponent* boss) {
    // コア露出終了時：通常時の深紅・標準太さに完全復元
    if (auto coreMask = GetCoreEffectMask(boss)) {
        auto params = coreMask->GetCustomParams();
        params.color1 = originalCoreColor_;
        params.param1 = originalThickness_;
        coreMask->SetCustomParams(params);
    }
}

void BossStateCoreExposed::OnTakeDamage(BossComponent* boss, float damage) {
    if (!boss) {
        return;
    }

    // コア被弾時の白熱ヒットフラッシュ演出 (Juice)
    if (auto coreMask = GetCoreEffectMask(boss)) {
        auto params = coreMask->GetCustomParams();
        params.color1 = Irufemi::Vector4{2.5f, 2.5f, 2.5f, 1.0f}; // 白熱閃光
        coreMask->SetCustomParams(params);
        hitFlashTimer_ = 0.08f;
    }

    // ボス装甲の被弾剥離（破片ドロップ連携）：弾丸ヒット時に破片を2個飛散させる
    boss->SpawnDebrisCluster(2, 4.0f);

    // ダメージ適用（HP減算、ログ出力、通知、撃破ステート遷移を内部カプセル化）
    boss->ApplyCoreDamage(damage);
}
