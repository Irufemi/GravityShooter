#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>

class TransformComponent;
class RailShooterPlayerComponent;
class CameraComponent;

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
        Bootup,  ///< [Phase 3: システムブート] FCS起動チャイム再生・HUD展開・操作待機
        Active   ///< [Phase 4: 通常戦闘] 起動完了・プレイヤー操作権委譲
    };

    StageIntroDirectorComponent() = default;
    ~StageIntroDirectorComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnDestroy() override;
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

    /**
     * @brief イントロ演出を即座にスキップして通常戦闘へ移行する (Bail-out)
     */
    void SkipIntroSequence();

private:
    bool EnsureShipInitialized();
    void SetHUDActive(bool active);
    void SetShipThrusterActive(bool active);
    void SetShipThrusterScale(float targetScaleZ, bool snap = false);
    void SetIntroState(StageIntroState newState);

    void ApplyArrivalInertia(float progress);
    void TriggerFCSBootupSequence();
    void TriggerBreachImpulse();

    void UpdateWarpVisuals(float progress);
    void CleanupWarpVisuals();

    // インスペクター調整可能プロパティ
    float warpInDuration_ = 0.70f;    ///< 重力の壁突破・急減速にかける時間（秒）
    float arrivalDuration_ = 0.35f;   ///< 到着整流・ノーズダイブ収束にかける時間（秒）
    float bootupDuration_ = 0.65f;    ///< FCSシステムブートにかける時間（秒）
    float startOffsetZ_ = -35.00f;    ///< 重力の壁の向こう側（初期後方オフセット、メートル）
    float warpInFovBoostDeg_ = 14.0f; ///< 突入時の最大FOV拡張ブースト量（度）
    float maxWarpBlurWidth_ = 0.085f; ///< 突入時の最大ラジアルブラー強度
    std::string targetShipName_ = "Player"; ///< 演出対象の自機オブジェクト名

    // 内部状態管理
    StageIntroState introState_ = StageIntroState::WarpIn;
    float stateTimer_ = 0.0f;
    bool hasInitializedShip_ = false;
    bool hasTriggeredBreach_ = false; ///< 重力の壁突破インパルス発火済みフラグ

    std::weak_ptr<GameObject> shipObj_;
    Irufemi::Vector3 initialShipLocalPos_{0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 initialShipLocalRot_{0.0f, 0.0f, 0.0f};

    // カメラFOV & ポストプロセス管理
    CameraComponent* GetCameraComponent() const;
    std::weak_ptr<GameObject> cameraObj_;
    float baseCameraFov_ = 0.785398f;
    bool isRadialBlurActive_ = false;
    bool isChromaticActive_ = false;
};
