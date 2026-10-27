#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <memory>

class PlayerHealthComponent;
class BaseModel;
class CameraShakeComponent;
class ScreenEffectComponent;

/**
 * @class PlayerDamageVisualizerComponent
 * @brief プレイヤーの被弾演出（モデル点滅、カメラシェイク、画面エフェクト）および死亡演出を担うコンポーネント
 */
class PlayerDamageVisualizerComponent : public Component {
public:
    PlayerDamageVisualizerComponent() = default;
    ~PlayerDamageVisualizerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "PlayerDamageVisualizerComponent";
    }

    /**
     * @brief 被弾時の点滅演出を開始する
     */
    void TriggerDamageFlash();

    /**
     * @brief カメラシェイク演出を再生する
     */
    void TriggerCameraShake();

    /**
     * @brief 画面エフェクト（ポストエフェクト）を再生する
     */
    void TriggerScreenEffect();

    /**
     * @brief 死亡時のモデル非表示演出を実行する
     */
    void TriggerDeathVisuals();

    /**
     * @brief 臨界突破した特異点がスーパーノヴァ大爆発を起こす
     */
    void TriggerSupernovaExplosion();

    /**
     * @brief ゲーム中のHUD（レティクル・照準等）を非表示化する
     */
    void HideInGameHUD();

    float GetFlashInterval() const {
        return flashInterval_;
    }
    void SetFlashInterval(float interval) {
        flashInterval_ = interval;
    }

    float GetFlashDuration() const {
        return flashDuration_;
    }
    void SetFlashDuration(float duration) {
        flashDuration_ = duration;
    }

    const Irufemi::Vector4& GetFlashColor() const {
        return flashColor_;
    }
    void SetFlashColor(const Irufemi::Vector4& color) {
        flashColor_ = color;
    }

public:
    /**
     * @enum DeathSequencePhase
     * @brief 自機死亡シーケンスの明示的進行フェーズ
     */
    enum class DeathSequencePhase {
        None,           ///< 通常時（非死亡）
        Implosion,      ///< フェーズ1: 周回ガレキ・全敵弾回収 ＆ 特異点への加速度的収縮 (0.0s〜0.65s)
        VacuumFreeze,   ///< フェーズ2: 極小点での完全静寂・息を呑む真空フリーズ (0.65s〜0.85s)
        SupernovaBurst, ///< フェーズ3: 臨界突破！Voxel大破砕 ＆ カメラ激震・余韻 (0.85s〜2.10s)
        WhiteoutFade,   ///< フェーズ4: 破砕拡散を見届けた後、全画面ホワイトアウト侵食 (2.10s〜2.70s)
        Completed       ///< フェーズ5: 演出完了（ResultSceneへの確定バトンタッチ 2.70s〜）
    };

    /**
     * @brief 死亡演出シーケンス完了時のコールバックを登録する
     */
    void SetOnDeathSequenceCompleted(std::function<void()> callback) {
        onDeathSequenceCompleted_ = std::move(callback);
    }

    /**
     * @brief 現在の死亡シーケンスフェーズを取得する
     */
    DeathSequencePhase GetDeathPhase() const {
        return phase_;
    }

    /**
     * @brief 死亡シーケンスのフェーズを明示的に遷移させる
     */
    void SetDeathPhase(DeathSequencePhase newPhase);

private:
    /**
     * @brief 死亡シーケンスのステートマシンを更新する
     * @param[in] dtReal 実時間デルタタイム
     */
    void UpdateDeathSequence(float dtReal);

    /**
     * @brief 対象のBaseModelポインタを安全に取得する
     * @return 取得できたBaseModelへのポインタ、存在しない場合はnullptr
     */
    BaseModel* GetTargetModel();

private:
    PlayerHealthComponent* healthComp_ = nullptr; ///< 監視対象の体力コンポーネント

    // 点滅演出パラメータ
    float flashTimer_ = 0.0f;
    float flashDuration_ = 1.0f;
    float flashRemaining_ = 0.0f;
    float flashInterval_ = 0.05f;
    bool isFlashing_ = false;

    Irufemi::Vector4 flashColor_ = {1.0f, 0.0f, 0.0f, 1.0f};        ///< 点滅カラー
    Irufemi::Vector4 originalBaseColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 元のモデルカラー
    bool colorCached_ = false;

    // カメラシェイクパラメータ
    float shakeIntensity_ = 1.0f;
    int shakeFrames_ = 30;
    float shakeFrequency_ = 20.0f;

    // キャッシュ参照
    std::weak_ptr<GameObject> mainCameraObj_;
    CameraShakeComponent* cameraShakeComp_ = nullptr;
    ScreenEffectComponent* screenEffectComp_ = nullptr;

    // --- 死亡シーケンス・ステートマシン（SSOT） ---
    DeathSequencePhase phase_ = DeathSequencePhase::None; ///< 現在の進行フェーズ
    float phaseTimer_ = 0.0f;                             ///< 現在フェーズの経過タイマー（秒）
    Irufemi::Vector3 originalScale_{1.0f, 1.0f, 1.0f};    ///< 収縮前の自機スケール
    std::function<void()> onDeathSequenceCompleted_;      ///< 演出完了コールバック
};
