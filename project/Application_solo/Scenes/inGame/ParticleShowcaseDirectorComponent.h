#pragma once

#include "Framework/Component/Component.h"
#include <memory>
#include <vector>

namespace Irufemi {
class GameObject;
class TransformComponent;
class SplineComponent;
} // namespace Irufemi

class SplineFollowerComponent;
class GravityPlayerComponent;
class PlayerHealthComponent;
class PlayerTargetingComponent;
class PlayerDamageVisualizerComponent;
class DebrisManagerComponent;
class EnemySpawnerComponent;
class BossComponent;

/**
 * @class ParticleShowcaseDirectorComponent
 * @brief パーティクル実演専用テストステージの演出シーケンスを完全統括するディレクターコンポーネント
 * @details ゲーム本来の仕様（ガレキ引き寄せ・敵ロックオン・投擲飛翔・直撃3D
 * Voxel破砕・敵弾ガレキシールド防御・ボス連鎖爆散・自機特異点爆散）
 *          を一切の不発なく100%確実に実演し、クリア時とゲームオーバー時の双方を交互にループ展開します。
 */
class ParticleShowcaseDirectorComponent : public Irufemi::Component {
public:
    enum class Phase {
        IntroBoost,      ///< [Phase 0] スラスター推進実演 (0.0s ~ 2.5s)
        PullDebris,      ///< [Phase 1] ガレキ引き寄せ＆エレクトリックシアンオーラ点灯 (2.5s ~ 5.0s)
        LockonEnemy,     ///< [Phase 2] 敵飛来＆ロックオン・サテライトUI展開 (5.0s ~ 7.5s)
        ThrowAndShatter, ///< [Phase 3] ガレキ投擲・重力光条トレイル・直撃3D Voxel破砕 (7.5s ~ 10.5s)
        ShieldDefense,   ///< [Phase 4] 敵弾のガレキシールド受け止め防御実演 (10.5s ~ 14.5s)
        ClimaxSequence,  ///< [Phase 5] クライマックス（クリア周: ボス連鎖爆散 / 敗北周: 自機特異点大爆散）
        Finished         ///< シーケンス完了・リザルト遷移待機
    };

    /**
     * @struct Timing
     * @brief ショーケース各フェーズの演出タイミング設定パラメータ
     */
    struct Timing {
        static constexpr float kIntroBoostDuration = 2.5f;       ///< [Phase 0] スラスター推進時間 (秒)
        static constexpr float kPullDebrisDuration = 2.5f;       ///< [Phase 1] ガレキ引き寄せ時間 (秒)
        static constexpr float kLockonDuration = 2.5f;           ///< [Phase 2] ロックオン展開時間 (秒)
        static constexpr float kMinShatterDetectTime = 0.8f;     ///< [Phase 3] 撃破検知開始最短時間 (秒)
        static constexpr float kShatterAfterglowDuration = 1.8f; ///< [Phase 3] 撃破後アフターグロー鑑賞時間 (秒)
        static constexpr float kShatterFailsafeTimeout = 4.5f;   ///< [Phase 3] フェイルセーフタイムアウト (秒)
        static constexpr float kShieldDefenseCounterTime = 2.0f; ///< [Phase 4] 敵弾受け止め後の反撃投擲タイミング (秒)
        static constexpr float kShieldAfterglowDuration =
            2.0f;                                             ///< [Phase 4] スナイパー撃破後アフターグロー鑑賞時間 (秒)
        static constexpr float kShieldFailsafeTimeout = 5.0f; ///< [Phase 4] フェイルセーフタイムアウト (秒)
        static constexpr float kBossPullDebrisTime = 1.5f;    ///< [Phase 5] ボス対峙後のガレキ引き寄せタイミング (秒)
        static constexpr float kBossThrowDebrisTime = 2.8f;   ///< [Phase 5] ボスコアへの反撃投擲タイミング (秒)
        static constexpr float kBossCoreDamageTime = 3.8f;    ///< [Phase 5] ボスコア直撃・連鎖大爆散発火タイミング (秒)
    };

    ParticleShowcaseDirectorComponent() = default;
    ~ParticleShowcaseDirectorComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;

    std::string GetComponentName() const override {
        return "ParticleShowcaseDirectorComponent";
    }

private:
    void CacheReferences();
    void SpawnDemonstrationEnemy(const std::string& prefabPath, float initialDistOffset,
                                 const Irufemi::Vector2& formationOffset, bool shootAtPlayer);
    void PullDebrisToCount(int desiredCount);
    bool AreAllDemoEnemiesDestroyed() const;

private:
    Phase currentPhase_ = Phase::IntroBoost;
    float timeline_ = 0.0f;
    float phaseTimer_ = 0.0f;
    bool isClearShowcase_ = true; ///< 今回の周回がボス撃破クリア周か自機撃墜敗北周か
    bool hasTriggeredClimax_ = false;
    bool hasSpawnedCurrentPhase_ = false;   ///< 各フェーズでの1回限りスポーン保証フラグ
    bool hasTriggeredCounterThrow_ = false; ///< 反撃投擲実行フラグ
    bool hasEnemyDied_ = false;             ///< 敵撃破検知フラグ
    float enemyDiedTimer_ = 0.0f;           ///< 敵撃破後の余韻・破壊鑑賞タイマー
    bool hasPulledBossDebris_ = false;      ///< ボス対峙後のガレキ引き寄せフラグ
    bool hasThrownBossDebris_ = false;      ///< ボスコアへの投擲実行フラグ
    float bossEncounterTimer_ = 0.0f;       ///< ボス対峙・攻防演出タイマー

    // キャッシュされた各種参照
    std::weak_ptr<Irufemi::GameObject> playerObj_;
    std::weak_ptr<Irufemi::GameObject> bossObj_;
    Irufemi::SplineComponent* splineComp_ = nullptr;
    SplineFollowerComponent* playerFollowerComp_ = nullptr;
    GravityPlayerComponent* gravityComp_ = nullptr;
    PlayerHealthComponent* healthComp_ = nullptr;
    PlayerTargetingComponent* targetingComp_ = nullptr;
    PlayerDamageVisualizerComponent* damageVisComp_ = nullptr;
    DebrisManagerComponent* debrisMgr_ = nullptr;
    EnemySpawnerComponent* enemySpawner_ = nullptr;
    BossComponent* bossComp_ = nullptr;

    std::vector<std::weak_ptr<Irufemi::GameObject>> demoEnemies_;
};
