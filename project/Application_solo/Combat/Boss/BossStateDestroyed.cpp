#include "Combat/Boss/BossStateDestroyed.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Utility/Log.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/System/IrufemiEngine.h"
#include <iostream>

namespace {

/**
 * @class DeathStepExplosion
 * @brief 大爆発演出およびカメラシェイクの監視を行うステップ
 */
class DeathStepExplosion : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        if (boss && boss->GetGameObject()) {
            // 撃破イベントの通知（BossDamageVisualizerComponent が特大カメラシェイクを発火）
            boss->NotifyBossDied();

            // 演出エフェクトが始まったタイミングでボスのモデル描画およびアウトラインをすべて切る
            auto renderers = boss->GetGameObject()->GetComponentsInChildren<MeshRendererComponent>();
            for (auto* r : renderers) {
                if (r) {
                    r->SetVisible(false);
                }
            }
            auto skinnedRenderers = boss->GetGameObject()->GetComponentsInChildren<SkinnedMeshRendererComponent>();
            for (auto* r : skinnedRenderers) {
                if (r) {
                    r->SetVisible(false);
                }
            }
            auto effectMasks = boss->GetGameObject()->GetComponentsInChildren<EffectMaskComponent>();
            for (auto* em : effectMasks) {
                if (em) {
                    em->SetEnableEffectMask(false);
                }
            }
        }
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)boss;
        (void)visualizer;
        elapsedTime_ += dt;

        // スローモーションに影響されない実時間（1.20秒）で爆発余韻を待って完了
        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 1.20f; ///< 撃破爆発の余韻実時間（秒）
    bool isCompleted_ = false;
};

/**
 * @class DeathStepFinish
 * @brief ボス非アクティブ化と撃破シーケンス完了通知を行う最終ステップ
 */
class DeathStepFinish : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        if (boss && boss->GetGameObject()) {
            boss->GetGameObject()->SetIsActive(false);
        }
        if (boss) {
            boss->NotifyDeathSequenceFinished();
        }
        isCompleted_ = true;
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)boss;
        (void)visualizer;
        (void)dt;
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    bool isCompleted_ = false;
};

} // namespace

void BossStateDestroyed::Enter(BossComponent* boss) {
    Log::OutPutLog(std::cout, "Boss Destroyed! Starting death sequence steps...\n");
    visualizer_ = nullptr;
    if (boss && boss->GetGameObject()) {
        visualizer_ = boss->GetGameObject()->GetComponent<BossDamageVisualizerComponent>();
    }

    steps_.clear();
    currentStepIndex_ = 0;

    // 演出シーケンスステップの登録（将来の演出拡張時はここに追加可能）
    steps_.push_back(std::make_unique<DeathStepExplosion>());
    steps_.push_back(std::make_unique<DeathStepFinish>());

    if (!steps_.empty()) {
        steps_[currentStepIndex_]->Enter(boss, visualizer_);
    }
}

void BossStateDestroyed::Update(BossComponent* boss) {
    if (currentStepIndex_ >= steps_.size()) {
        return;
    }

    float dt = (boss && boss->GetEngine()) ? boss->GetEngine()->GetRealDeltaTime() : (1.0f / 60.0f);
    auto& currentStep = steps_[currentStepIndex_];
    currentStep->Update(boss, visualizer_, dt);

    if (currentStep->IsCompleted()) {
        currentStep->Exit(boss, visualizer_);
        currentStepIndex_++;
        if (currentStepIndex_ < steps_.size()) {
            steps_[currentStepIndex_]->Enter(boss, visualizer_);
        }
    }
}

void BossStateDestroyed::Exit(BossComponent* boss) {
    (void)boss;
}

void BossStateDestroyed::OnTakeDamage(BossComponent* boss, float damage) {
    (void)boss;
    (void)damage;
}
