#include "Scenes/inGame/ParticleShowcaseDirectorComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Utility/SplineComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "Player/GravityPlayerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/PlayerTargetingComponent.h"
#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Combat/EnemySpawnerComponent.h"
#include "Level/EnemyBuilder.h"
#include "Combat/Boss/BossComponent.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Core/AttractDemoManager.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include <algorithm>

void ParticleShowcaseDirectorComponent::Initialize() {
    isClearShowcase_ = AttractDemoManager::IsParticleShowcaseTargetClear();
    timeline_ = 0.0f;
    phaseTimer_ = 0.0f;
    bossEncounterTimer_ = 0.0f;
    currentPhase_ = Phase::IntroBoost;
    hasTriggeredClimax_ = false;
    hasSpawnedCurrentPhase_ = false;
    hasEnemyDied_ = false;
    enemyDiedTimer_ = 0.0f;
    hasTriggeredCounterThrow_ = false;
    hasPulledBossDebris_ = false;
    hasThrownBossDebris_ = false;
    demoEnemies_.clear();
}

void ParticleShowcaseDirectorComponent::Start() {
    CacheReferences();

    // ボス本体およびドローン群をシーン開始時に完全休眠（雑魚敵演出の邪魔を一切排除）
    if (auto boss = bossObj_.lock()) {
        if (bossComp_) {
            bossComp_->CleanupSubEntities();
        }
        boss->SetActive(false);
    }

    // クリア周の場合は道中での不慮の撃墜を避けるためゴッドモード保護
    if (healthComp_) {
        healthComp_->SetGodMode(isClearShowcase_);
    }

    // デモ開始時に自機スラスターを全開にしてダイナミックな推進を演出
    if (auto player = playerObj_.lock()) {
        if (auto thruster = player->GetComponent<PlayerThrusterVisualizerComponent>()) {
            thruster->SetThrusterActive(true);
            thruster->SetTargetScaleZ(1.5f);
        }
    }
}

void ParticleShowcaseDirectorComponent::CacheReferences() {
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (!scene) {
        return;
    }

    if (auto player = scene->FindGameObject("Player")) {
        playerObj_ = player;
        gravityComp_ = player->GetComponent<GravityPlayerComponent>();
        healthComp_ = player->GetComponent<PlayerHealthComponent>();
        targetingComp_ = player->GetComponent<PlayerTargetingComponent>();
        damageVisComp_ = player->GetComponent<PlayerDamageVisualizerComponent>();
    }

    if (auto pathObj = scene->FindGameObject("PathManager")) {
        splineComp_ = pathObj->GetComponent<Irufemi::SplineComponent>();
    }

    if (auto cartObj = scene->FindGameObject("PlayerCart")) {
        playerFollowerComp_ = cartObj->GetComponent<SplineFollowerComponent>();
    }

    if (auto debrisObj = scene->FindGameObject("DebrisManager")) {
        debrisMgr_ = debrisObj->GetComponent<DebrisManagerComponent>();
    }

    if (auto spawnerObj = scene->FindGameObject("EnemySpawner")) {
        enemySpawner_ = spawnerObj->GetComponent<EnemySpawnerComponent>();
    }

    if (auto bossObj = scene->FindGameObject("Boss")) {
        bossObj_ = bossObj;
        bossComp_ = bossObj->GetComponent<BossComponent>();
    }
}

void ParticleShowcaseDirectorComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    float dt = engine->GetGameDeltaTime();
    timeline_ += dt;
    phaseTimer_ += dt;

    if (!gravityComp_ || !healthComp_ || !targetingComp_) {
        CacheReferences();
    }

    switch (currentPhase_) {
    // === [Phase 0] スラスター推進実演 ===
    case Phase::IntroBoost:
        if (phaseTimer_ >= Timing::kIntroBoostDuration) {
            currentPhase_ = Phase::PullDebris;
            phaseTimer_ = 0.0f;
            hasSpawnedCurrentPhase_ = false;
        }
        break;

    // === [Phase 1] ガレキ引き寄せ＆エレクトリックシアンオーラ点灯 ===
    case Phase::PullDebris:
        if (!hasSpawnedCurrentPhase_) {
            hasSpawnedCurrentPhase_ = true;
            if (debrisMgr_) {
                debrisMgr_->SpawnDebrisInFrontOfPlayer(4);
            }
            PullDebrisToCount(2); // 敵2機分（2個）確実に引き寄せてオービット保持
        }
        if (phaseTimer_ >= Timing::kPullDebrisDuration) {
            currentPhase_ = Phase::LockonEnemy;
            phaseTimer_ = 0.0f;
            hasSpawnedCurrentPhase_ = false;
        }
        break;

    // === [Phase 2] Wave 1: 通常雑魚敵（重力ゴーレム）2機編隊アプローチ＆マルチロックオン ===
    case Phase::LockonEnemy:
        if (!hasSpawnedCurrentPhase_) {
            hasSpawnedCurrentPhase_ = true;
            demoEnemies_.clear();
            // 左右にオフセットした重力ゴーレム2機をスポーン（本編標準雑魚敵・迫力ある大型ボクセル破砕実演）
            SpawnDemonstrationEnemy("resources/prefabs/Enemy_GravityGolem.json", 30.0f, {-6.5f, 1.5f}, false);
            SpawnDemonstrationEnemy("resources/prefabs/Enemy_GravityGolem.json", 30.0f, {6.5f, 1.5f}, false);

            if (targetingComp_) {
                for (auto& w : demoEnemies_) {
                    if (auto enemy = w.lock()) {
                        targetingComp_->AddExplicitTarget(enemy);
                    }
                }
            }
        }
        if (phaseTimer_ >= Timing::kLockonDuration) {
            currentPhase_ = Phase::ThrowAndShatter;
            phaseTimer_ = 0.0f;
            hasSpawnedCurrentPhase_ = false;
        }
        break;

    // === [Phase 3] ガレキ連続投擲・重力光条トレイル・2機連続3D Voxel破砕 ===
    case Phase::ThrowAndShatter:
        if (!hasSpawnedCurrentPhase_) {
            hasSpawnedCurrentPhase_ = true;
            if (gravityComp_) {
                gravityComp_->ExecuteThrowAction();
            }
        }
        // 敵全滅を検知後、Voxel破砕・電磁紫煙・残骸ガレキ飛翔を邪魔なく鑑賞させるアフターグロー待機時間を計測
        if (phaseTimer_ >= Timing::kMinShatterDetectTime && AreAllDemoEnemiesDestroyed() && !hasEnemyDied_) {
            hasEnemyDied_ = true;
            enemyDiedTimer_ = 0.0f;
        }
        if (hasEnemyDied_) {
            enemyDiedTimer_ += dt;
        }

        // 撃破からじっくり演出を見せてから、次のシールド防御フェーズへ遷移（フェイルセーフ付き）
        if ((hasEnemyDied_ && enemyDiedTimer_ >= Timing::kShatterAfterglowDuration) ||
            phaseTimer_ >= Timing::kShatterFailsafeTimeout) {
            currentPhase_ = Phase::ShieldDefense;
            phaseTimer_ = 0.0f;
            hasSpawnedCurrentPhase_ = false;
            hasEnemyDied_ = false;
            enemyDiedTimer_ = 0.0f;
            hasTriggeredCounterThrow_ = false;
        }
        break;

    // === [Phase 4] Wave 2: 狙撃メカ出現・敵弾ガレキシールド受け止め防御実演 ===
    case Phase::ShieldDefense:
        if (!hasSpawnedCurrentPhase_) {
            hasSpawnedCurrentPhase_ = true;
            // ガレキを再補充してシールド展開
            if (debrisMgr_) {
                debrisMgr_->SpawnDebrisInFrontOfPlayer(4);
            }
            PullDebrisToCount(2); // シールド防御用ガレキ補充
            demoEnemies_.clear();
            // 正面に射撃スナイパーメカをスポーン
            SpawnDemonstrationEnemy("resources/prefabs/Enemy_SniperArtillery.json", 28.0f, {0.0f, 0.0f}, true);
        }
        // 敵弾を受け止めさせた後、反撃投擲
        if (phaseTimer_ >= Timing::kShieldDefenseCounterTime && !hasTriggeredCounterThrow_) {
            hasTriggeredCounterThrow_ = true;
            if (!demoEnemies_.empty()) {
                if (auto sniper = demoEnemies_[0].lock()) {
                    if (targetingComp_) {
                        targetingComp_->AddExplicitTarget(sniper);
                    }
                    if (gravityComp_) {
                        gravityComp_->ExecuteThrowAction();
                    }
                }
            }
        }
        // 反撃投擲後、スナイパー撃破を検知してアフターグロー待機時間を計測
        if (hasTriggeredCounterThrow_ && AreAllDemoEnemiesDestroyed() && !hasEnemyDied_) {
            hasEnemyDied_ = true;
            enemyDiedTimer_ = 0.0f;
        }
        if (hasEnemyDied_) {
            enemyDiedTimer_ += dt;
        }

        // 撃破からじっくり演出を見せてから、次のクライマックス（ボス出現）へ遷移（フェイルセーフ付き）
        if ((hasEnemyDied_ && enemyDiedTimer_ >= Timing::kShieldAfterglowDuration) ||
            phaseTimer_ >= Timing::kShieldFailsafeTimeout) {
            currentPhase_ = Phase::ClimaxSequence;
            phaseTimer_ = 0.0f;
            hasSpawnedCurrentPhase_ = false;
            hasEnemyDied_ = false;
            enemyDiedTimer_ = 0.0f;
            hasTriggeredClimax_ = false;
            hasPulledBossDebris_ = false;
            hasThrownBossDebris_ = false;
            bossEncounterTimer_ = 0.0f;
        }
        break;

    // === [Phase 5] クライマックス（クリア周: ボス出現・対峙・連鎖大爆散 / 敗北周: 自機特異点大爆散） ===
    case Phase::ClimaxSequence:
        if (isClearShowcase_) {
            bossEncounterTimer_ += dt;

            // 1. ボス威風堂々たる出現・対峙 (ガレキ回収はまだ行わずボスの巨大感をしっかり見せる)
            if (!hasTriggeredClimax_) {
                hasTriggeredClimax_ = true;
                if (auto boss = bossObj_.lock()) {
                    boss->SetActive(true);
                }
            }

            // 2. 対峙後、ボスに対抗するため自機がガレキを一気に引き寄せ！
            if (bossEncounterTimer_ >= Timing::kBossPullDebrisTime && !hasPulledBossDebris_) {
                hasPulledBossDebris_ = true;
                if (debrisMgr_) {
                    debrisMgr_->SpawnDebrisInFrontOfPlayer(4);
                }
                PullDebrisToCount(2);
            }

            // 3. ボスコアへ向けて反撃投擲！
            if (bossEncounterTimer_ >= Timing::kBossThrowDebrisTime && !hasThrownBossDebris_) {
                hasThrownBossDebris_ = true;
                if (auto boss = bossObj_.lock()) {
                    if (targetingComp_) {
                        targetingComp_->AddExplicitTarget(boss);
                    }
                    if (gravityComp_) {
                        gravityComp_->ExecuteThrowAction();
                    }
                }
            }

            // 4. ボスコア直撃！4段連鎖爆散＆4,000ボクセル全方位大爆散発火！
            if (bossEncounterTimer_ >= Timing::kBossCoreDamageTime && bossComp_ && bossComp_->GetHp() > 0.0f) {
                bossComp_->ApplyCoreDamage(99999.0f);
            }
        } else {
            // ゲームオーバー敗北実演：被弾フィードバック ➔ 自機特異点大爆散！
            if (!hasTriggeredClimax_) {
                hasTriggeredClimax_ = true;
                if (damageVisComp_) {
                    damageVisComp_->TriggerDamageFlash();
                    damageVisComp_->TriggerCameraShake();
                    damageVisComp_->TriggerScreenEffect();
                }
                if (healthComp_) {
                    healthComp_->SetGodMode(false);
                    healthComp_->TakeDamage(99999);
                }
            }
        }
        break;

    case Phase::Finished:
    default:
        break;
    }
}

void ParticleShowcaseDirectorComponent::SpawnDemonstrationEnemy(const std::string& prefabPath, float initialDistOffset,
                                                               const Irufemi::Vector2& formationOffset,
                                                               bool shootAtPlayer) {
    if (!enemySpawner_) {
        return;
    }

    EnemyBuilder builder(enemySpawner_);
    builder.WithPrefab(prefabPath)
        .WithTargetDistance(initialDistOffset)
        .WithCombatDuration(12.0f)
        .WithShootInterval(shootAtPlayer ? 0.9f : 999.0f)
        .WithBulletSpeed(35.0f)
        .WithSpeed(5.0f);

    // 自機との相対レール追従を設定し、指定オフセット位置に対峙・同期追従させる
    if (splineComp_ && playerFollowerComp_) {
        builder.WithRailTrackingParams(splineComp_, playerFollowerComp_, initialDistOffset, initialDistOffset,
                                       formationOffset);
    }

    Irufemi::GameObject* enemyObj = builder.Build({0.0f, 0.0f, 0.0f}, {0.0f, 3.14159f, 0.0f}, 1.0f);
    if (enemyObj) {
        demoEnemies_.push_back(enemyObj->shared_from_this());
    }
}

void ParticleShowcaseDirectorComponent::PullDebrisToCount(int desiredCount) {
    if (!gravityComp_) {
        return;
    }
    int current = gravityComp_->GetOrbitingDebrisCount();
    for (int i = current; i < desiredCount; ++i) {
        gravityComp_->ExecutePullAction();
    }
}

bool ParticleShowcaseDirectorComponent::AreAllDemoEnemiesDestroyed() const {
    if (demoEnemies_.empty()) {
        return true;
    }
    for (const auto& w : demoEnemies_) {
        if (auto enemy = w.lock()) {
            if (enemy->GetIsActive()) {
                if (auto enemyComp = enemy->GetComponent<RailShooterEnemyComponent>()) {
                    if (enemyComp->IsAlive()) {
                        return false; // 生存中の敵が存在する
                    }
                }
            }
        }
    }
    return true; // 全滅
}
