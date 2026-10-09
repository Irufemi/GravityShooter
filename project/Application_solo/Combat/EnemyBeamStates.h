#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstdint>
#include <memory>

class EnemyBeamComponent;

/**
 * @struct BeamConfig
 * @brief ビーム諸元および演出パラメータを集約するデータ構造体
 */
struct BeamConfig {
    float beamLength = 200.0f;          //!< ビームの最大長
    float beamMaxRadius = 1.0f;         //!< ビームの最大半径
    float chargeDuration = 1.5f;        //!< 溜め時間（秒）
    float fireDuration = 0.8f;          //!< 発射時間（秒）
    float lockLeadTime = 0.4f;          //!< 発射前何秒で射線を固定するか
    int beamDamage = 30;                //!< 直撃ダメージ
    float hitCheckRadiusMargin = 0.5f;  //!< 当たり判定マージン

    Irufemi::Vector4 telegraphColor = {1.0f, 0.1f, 0.1f, 0.7f}; //!< 予兆円柱の基本色
    Irufemi::Vector4 chargeColor = {0.7f, 0.0f, 0.9f, 1.0f};    //!< チャージ球の色

    Irufemi::Vector4 beamColor = {0.8f, 0.0f, 1.0f, 1.0f};        //!< ビーム主色
    Irufemi::Vector4 beamCoreColor = {0.0f, 1.0f, 1.0f, 1.0f};    //!< ビームコア色
    float beamIntensity = 6.0f;
    float beamCoreIntensity = 40.0f;
    float beamSpeed = 3.0f;

    Irufemi::Vector4 auraColor = {0.1f, 0.0f, 0.2f, 1.0f};        //!< 外側オーラ色
    Irufemi::Vector4 auraCoreColor = {0.8f, 0.0f, 1.0f, 1.0f};    //!< オーラコア色
    float auraIntensity = 12.0f;
    float auraSpeed = 0.8f;
};

/**
 * @class IBeamState
 * @brief ビームの状態遷移と個別フェーズの振る舞いを司るインターフェース (State Pattern)
 */
class IBeamState {
public:
    virtual ~IBeamState() = default;

    virtual void OnEnter(EnemyBeamComponent& owner) {}
    virtual void OnUpdate(EnemyBeamComponent& owner, float deltaTime) = 0;
    virtual void OnDraw(EnemyBeamComponent& owner, uint32_t frameIndex) = 0;
    virtual void OnExit(EnemyBeamComponent& owner) {}
};

/**
 * @class BeamChargingState
 * @brief 溜め・AOE予兆追尾および射線ロックオン制御を行うステート
 */
class BeamChargingState : public IBeamState {
public:
    void OnEnter(EnemyBeamComponent& owner) override;
    void OnUpdate(EnemyBeamComponent& owner, float deltaTime) override;
    void OnDraw(EnemyBeamComponent& owner, uint32_t frameIndex) override;
    void OnExit(EnemyBeamComponent& owner) override;

private:
    float stateTimer_ = 0.0f;
    bool isAimLocked_ = false;
};

/**
 * @class BeamFiringState
 * @brief レーザー照射・当たり判定・カメラシェイク制御を行うステート
 */
class BeamFiringState : public IBeamState {
public:
    void OnEnter(EnemyBeamComponent& owner) override;
    void OnUpdate(EnemyBeamComponent& owner, float deltaTime) override;
    void OnDraw(EnemyBeamComponent& owner, uint32_t frameIndex) override;
    void OnExit(EnemyBeamComponent& owner) override;

private:
    float stateTimer_ = 0.0f;
};
