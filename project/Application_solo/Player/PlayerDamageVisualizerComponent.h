#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/PostProcess/PostProcessHandle.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <memory>

class PlayerHealthComponent;
class IPlayerDeathState;
namespace Irufemi {
class BaseModel;
}
namespace Irufemi {
class CameraShakeComponent;
}
namespace Irufemi {
class ScreenEffectComponent;
}

/**
 * @class PlayerDamageVisualizerComponent
 * @brief プレイヤーの被弾演出（モデル点滅、カメラシェイク、画面エフェクト）および死亡演出を担うコンポーネント
 */
class PlayerDamageVisualizerComponent : public Irufemi::Component {
public:
    PlayerDamageVisualizerComponent();
    ~PlayerDamageVisualizerComponent() override;

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
        Meltdown,       ///< 第1幕: 装甲破断・Voxel剥離・重力過熱暴走 (0.00s〜0.40s)
        BlackHole,      ///< 第2幕: 漆黒の事象の地平面出現・求心スワール超吸引 (0.40s〜1.00s)
        VacuumFreeze,   ///< 幕間: 臨界極小点での完全静寂 (1.00s〜1.06s)
        SupernovaBurst, ///< 第3幕: ガンマ線バースト閃光 & 超巨大GRAVITY DAZE色収差衝撃波 & Voxel結晶爆散 (1.06s〜1.80s)
        PlasmaAfterglow, ///< 第4幕: 宇宙の冷気と青白いプラズマ余韻を見届ける (1.80s〜2.50s)
        WhiteoutFade,    ///< リザルト接続フェード (2.50s〜3.00s)
        Completed        ///< シーケンス完了 (3.00s〜)
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
    DeathSequencePhase GetDeathPhase() const;

    /**
     * @brief 死亡シーケンスのステートを遷移させる (State Pattern)
     */
    void ChangeDeathState(std::unique_ptr<IPlayerDeathState> newState);

    /**
     * @brief 死亡シーケンスのフェーズを明示的に遷移させる（互換用ファクトリ）
     */
    void SetDeathPhase(DeathSequencePhase newPhase);

    // --- State クラス向け各種アクセサ・操作メソッド ---
    const Irufemi::Vector3& GetDeathWorldPos() const {
        return deathWorldPos_;
    }
    const Irufemi::Vector3& GetDeathWorldRot() const {
        return deathWorldRot_;
    }
    const Irufemi::Vector3& GetOriginalScale() const {
        return originalScale_;
    }
    const Irufemi::Vector3& GetOriginalLocalPos() const {
        return originalLocalPos_;
    }
    const Irufemi::Vector2& GetDeathScreenUV() const {
        return deathScreenUV_;
    }

    Irufemi::ScopedEffectHandle& GetBlackHoleHandle() {
        return blackHoleHandle_;
    }
    Irufemi::ScopedEffectHandle& GetDeathFadeHandle() {
        return deathFadeHandle_;
    }

    void SetIsFlashing(bool flashing) {
        isFlashing_ = flashing;
    }
    bool IsColorCached() const {
        return colorCached_;
    }
    void SetColorCached(bool cached) {
        colorCached_ = cached;
    }
    void SetOriginalBaseColor(const Irufemi::Vector4& color) {
        originalBaseColor_ = color;
    }

    Irufemi::BaseModel* GetTargetModel();
    void InvokeDeathCompletedCallback() {
        if (onDeathSequenceCompleted_) {
            onDeathSequenceCompleted_();
        }
    }

private:
    /**
     * @brief 死亡シーケンスのステートマシンを更新する
     * @param[in] dtReal 実時間デルタタイム
     */
    void UpdateDeathSequence(float dtReal);

    /**
     * @brief ワールド座標を現在のカメラからスクリーンUV座標 (0.0〜1.0) へ動的投影する
     * @param[in] worldPos 投影対象のワールド座標
     * @return スクリーンUV座標
     */
    Irufemi::Vector2 CalculateWorldScreenUV(const Irufemi::Vector3& worldPos);

    /**
     * @brief 画面内の敵オブジェクトのアウトライン（Irufemi::EffectMaskComponent）を消去する
     */
    void ClearEnemyOutlines();

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
    std::weak_ptr<Irufemi::GameObject> mainCameraObj_;
    Irufemi::CameraShakeComponent* cameraShakeComp_ = nullptr;
    Irufemi::ScreenEffectComponent* screenEffectComp_ = nullptr;
    Irufemi::BaseModel* cachedTargetModel_ = nullptr;

    // --- 死亡シーケンス・ステートマシン（SSOT & State Pattern） ---
    std::unique_ptr<IPlayerDeathState> currentDeathState_ = nullptr; ///< 現在の具象状態インスタンス
    DeathSequencePhase phase_ = DeathSequencePhase::None;            ///< 現在の進行フェーズ (互換用キャッシュ)
    float phaseTimer_ = 0.0f;                             ///< 現在フェーズの経過タイマー（秒）
    Irufemi::Vector3 originalScale_{1.0f, 1.0f, 1.0f};    ///< 収縮前の自機スケール
    Irufemi::Vector3 originalLocalPos_{0.0f, 0.0f, 0.0f}; ///< 撃墜前の自機ローカル座標
    Irufemi::Vector2 deathScreenUV_{0.5f, 0.5f};          ///< 自機のスクリーンUV
    Irufemi::Vector3 deathWorldPos_{0.0f, 0.0f, 0.0f};    ///< 自機の撃墜ワールド座標
    Irufemi::Vector3 deathWorldRot_{0.0f, 0.0f, 0.0f};    ///< 自機の撃墜ワールド回転
    std::function<void()> onDeathSequenceCompleted_;      ///< 演出完了コールバック

    // --- 死亡シーケンス・RAIIエフェクト寿命管理 ---
    Irufemi::ScopedEffectHandle blackHoleHandle_;
    Irufemi::ScopedEffectHandle deathFadeHandle_;
};
