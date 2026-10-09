#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>

class TransformComponent;
class RailShooterPlayerComponent;

/**
 * @class StageIntroDirectorComponent
 * @brief ゲームステージ開始時の自機突入（Warp-In）および到着シークエンスを司るディレクターコンポーネント
 * @details 出撃シーンからの余韻を受け継ぎ、カメラ後方から自機が超高速で飛来・滑らかに急減速して
 *          定位置へスナップし、HUD点灯と同時にプレイヤーへ操作権を委譲するFSM演出をデータ駆動で管理します。
 */
class StageIntroDirectorComponent : public Component {
public:
    /**
     * @enum StageIntroState
     * @brief ステージ開始イントロの有限状態機械（FSM）
     */
    enum class StageIntroState {
        WarpIn,  ///< [Phase 1: 飛来急減速] 自機がカメラ後方から飛来・3次急減速
        Arrival, ///< [Phase 2: 到着整流] 定位置到着スナップ・スラスター炎収縮
        Active   ///< [Phase 3: 通常戦闘] HUD点灯・プレイヤー操作権委譲
    };

    StageIntroDirectorComponent() = default;
    ~StageIntroDirectorComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "StageIntroDirectorComponent";
    }

    /**
     * @brief 現在のイントロ演出ステートを取得する
     */
    StageIntroState GetIntroState() const {
        return introState_;
    }

    /**
     * @brief イントロ演出が完了して通常プレイ中かどうか
     */
    bool IsActive() const {
        return introState_ == StageIntroState::Active;
    }

private:
    bool EnsureShipInitialized();
    void SetHUDActive(bool active);
    void SetShipThrusterActive(bool active);
    void SetShipThrusterScale(float targetScaleZ, bool snap = false);
    void SetIntroState(StageIntroState newState);

    // 将来の演出拡張用フック（B案: 整流バウンス & FCSブートアップ）
    void ApplyArrivalInertia(float progress);
    void TriggerFCSBootupSequence();

    // インスペクター調整可能プロパティ
    float warpInDuration_ = 1.00f;          ///< 飛来急減速にかける時間（秒）
    float arrivalDuration_ = 0.50f;         ///< 到着整流にかける時間（秒）
    float startOffsetZ_ = -25.00f;          ///< 飛来開始時の初期後方オフセット（メートル）
    std::string targetShipName_ = "Player"; ///< 演出対象の自機オブジェクト名

    // 内部状態管理
    StageIntroState introState_ = StageIntroState::WarpIn;
    float stateTimer_ = 0.0f;
    bool hasInitializedShip_ = false;

    std::weak_ptr<GameObject> shipObj_;
    Irufemi::Vector3 initialShipLocalPos_{0.0f, 0.0f, 0.0f};
};
