#pragma once

#include "Player/PlayerDamageVisualizerComponent.h"
#include <memory>

/**
 * @class IPlayerDeathState
 * @brief 自機死亡シーケンスの各幕（フェーズ）の振る舞いを司る基底インターフェース (State Pattern)
 */
class IPlayerDeathState {
public:
    virtual ~IPlayerDeathState() = default;

    /**
     * @brief 状態突入時の初期化処理
     * @param visualizer コンポーネント本体
     */
    virtual void Enter(PlayerDamageVisualizerComponent* visualizer) = 0;

    /**
     * @brief 毎フレームの更新処理と次状態への遷移判断
     * @param visualizer コンポーネント本体
     * @param dt 実時間デルタタイム
     */
    virtual void Update(PlayerDamageVisualizerComponent* visualizer, float dt) = 0;

    /**
     * @brief 状態退出時の後処理
     * @param visualizer コンポーネント本体
     */
    virtual void Exit(PlayerDamageVisualizerComponent* /*visualizer*/) {}

    /**
     * @brief 現在の状態に対応するフェーズ識別子を取得
     */
    virtual PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const = 0;
};

/**
 * @class PlayerDeathStateMeltdown
 * @brief 第1幕: 装甲破断・Voxel剥離・重力過熱暴走 (0.00s〜0.40s)
 */
class PlayerDeathStateMeltdown : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::Meltdown;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStateBlackHole
 * @brief 第2幕: 漆黒の事象の地平面出現・求心スワール超吸引・インプロージョン (0.40s〜1.20s)
 */
class PlayerDeathStateBlackHole : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::BlackHole;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStateVacuumFreeze
 * @brief 幕間: 臨界極小点での完全静寂・一瞬のタメ (1.20s〜1.35s)
 */
class PlayerDeathStateVacuumFreeze : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::VacuumFreeze;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStateSupernovaBurst
 * @brief 第3幕: ガンマ線バースト閃光 & 超巨大GRAVITY色収差衝撃波 & Voxel結晶爆散 (1.35s〜2.10s)
 */
class PlayerDeathStateSupernovaBurst : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::SupernovaBurst;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStatePlasmaAfterglow
 * @brief 第4幕: 白熱流体が画面を飲み込みホワイトアウトへ加速 (2.10s〜2.90s)
 */
class PlayerDeathStatePlasmaAfterglow : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::PlasmaAfterglow;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStateWhiteoutFade
 * @brief 第5幕: 眩い純白（白1.0）を維持し、リザルト画面へシームレス接続 (2.90s〜3.20s)
 */
class PlayerDeathStateWhiteoutFade : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::WhiteoutFade;
    }

private:
    float stateTimer_ = 0.0f;
};

/**
 * @class PlayerDeathStateCompleted
 * @brief 最終幕: 死亡演出完了・ゲームオーバー遷移通知
 */
class PlayerDeathStateCompleted : public IPlayerDeathState {
public:
    void Enter(PlayerDamageVisualizerComponent* visualizer) override;
    void Update(PlayerDamageVisualizerComponent* visualizer, float dt) override;
    PlayerDamageVisualizerComponent::DeathSequencePhase GetPhase() const override {
        return PlayerDamageVisualizerComponent::DeathSequencePhase::Completed;
    }
};
