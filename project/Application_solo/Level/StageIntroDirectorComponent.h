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

    // 物理的な重力水面膜（3Dメッシュ & EffectMask）管理
    bool EnsureMembraneInitialized();
    void UpdateMembraneVisuals(float progress);
    void DestroyMembraneObject();

    // インスペクター調整可能プロパティ（完全データ駆動化・ハードコード排除）
    float warpInDuration_ =
        1.80f; ///< 重力の壁突破・急減速にかける時間（秒: 1.8秒でしっかりと水面膜の揺らぎと突破を視認可能に）
    float arrivalDuration_ = 0.35f; ///< 到着整流・ノーズダイブ収束にかける時間（秒）
    float bootupDuration_ = 0.65f;  ///< FCSシステムブートにかける時間（秒）
    float startOffsetZ_ = -4.80f;   ///< 重力の壁突入時の初期後方オフセット（メートル:
                                  ///< カメラZ=-6.5mの前方1.7m、手前ドアップから前方の水面膜へ突進）
    float warpInFovBoostDeg_ = 24.0f; ///< 突入時の最大FOV拡張ブースト量（度: 魚眼に近い超広角）
    float maxWarpBlurWidth_ = 0.14f;  ///< 突入時の最大ラジアルブラー強度（強烈な速度線）
    float warpInRailSpeedBoost_ = 2.2f; ///< 突入開始時のレール前進初速ブースト倍率（2.2倍 -> 1.0倍へ滑らかに減衰）
    std::string targetShipName_ = "Player";                              ///< 演出対象の自機オブジェクト名
    std::string warpSePath_ = "resources/audio/SE/se_gravity_warp.mp3";  ///< 空間突破・重力膜通過音
    std::string fcsBootSePath_ = "resources/audio/SE/se_fcs_online.mp3"; ///< FCSシステム起動チャイム音

    // --- 空間物理配置 超高密度重力エネルギー界面（Gravity Membrane: Primitive + EffectMask）突破用プロパティ ---
    float membraneOffsetZ_ = -1.80f; ///< 重力界面の初期ローカルZ位置（自機開始Z=-4.8mと定位置0.0mの中間）
    float membraneScaleX_ = 48.00f; ///< 重力界面の横幅（メートル: 視界一面を覆い尽くす重力レンズ界面）
    float membraneScaleY_ = 36.00f; ///< 重力界面の縦幅（メートル: 視界一面を覆い尽くす重力レンズ界面）
    float membraneDisplacementIntensity_ = 0.038f; ///< 重力界面の上品な局所Simplex Noise屈折歪み強度
    float membraneRgbShiftRatio_ = 0.04f; ///< 重力界面の色収差比率（ギラつきを抑えた自然な空間屈折）
    std::string breachSePath_ = "resources/audio/SE/se_water_breach.mp3"; ///< 重力界面破断・空間突破音
    float breachProgressThreshold_ =
        0.32f; ///< 重力膜を突き破るタイミング（進行割合: 0.32 = 自機が膜Z=-1.8mに接触・貫通する瞬間）
    float breachFlashIntensity_ = 0.30f; ///< 重力膜突破時の白・シアン重力破断閃光強度（白飛びを抑え色収差を鮮明化）
    float breachShakeAmplitude_ = 0.090f; ///< 重力膜突破時の正面カメラ激震振幅（メートル）

    // 内部状態管理
    StageIntroState introState_ = StageIntroState::WarpIn;
    float stateTimer_ = 0.0f;
    bool hasInitializedShip_ = false;
    bool hasTriggeredBreach_ = false; ///< 重力の壁突破インパルス発火済みフラグ
    float breachTimer_ = 0.0f;        ///< 突破フラッシュの経過タイマー

    std::weak_ptr<GameObject> shipObj_;
    std::weak_ptr<GameObject> playerCartObj_; ///< レール追従を行っている親オブジェクト（PlayerCart）
    std::shared_ptr<GameObject> membraneObj_; ///< 空間に物理配置される重力水面膜オブジェクト
    Irufemi::Vector3 initialShipLocalPos_{0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 initialShipLocalRot_{0.0f, 0.0f, 0.0f};

    // カメラFOV & ポストプロセス管理
    CameraComponent* GetCameraComponent() const;
    std::weak_ptr<GameObject> cameraObj_;
    float baseCameraFov_ = 0.785398f;
    bool isRadialBlurActive_ = false;
    bool isChromaticActive_ = false;
    bool isFadeActive_ = false;
};
