#pragma once
#include "Framework/Component/Component.h"
#include <memory>

class BossComponent;
class GameObject;
class CameraShakeComponent;

/**
 * @class BossDamageVisualizerComponent
 * @brief ボスの被弾演出（カメラシェイク）および撃破演出を管理するコンポーネント
 */
class BossDamageVisualizerComponent : public Component {
public:
    BossDamageVisualizerComponent() = default;
    ~BossDamageVisualizerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "BossDamageVisualizerComponent";
    }

    /**
     * @brief 被弾時のカメラシェイクを再生する
     * @param damage 受けたダメージ量
     */
    void TriggerDamageShake(float damage);

    /**
     * @brief 撃破時の特大カメラシェイクを再生する
     */
    void TriggerDeathShake();

    /**
     * @brief 撃破シェイクが現在再生中かどうかを取得する
     * @return 再生中の場合は true
     */
    bool IsDeathShakePlaying() const;

    /**
     * @brief 被弾シェイクの強度を取得する
     */
    float GetDamageShakeIntensity() const {
        return damageShakeIntensity_;
    }
    /**
     * @brief 被弾シェイクの強度を設定する
     */
    void SetDamageShakeIntensity(float val) {
        damageShakeIntensity_ = val;
    }

    /**
     * @brief 被弾シェイクのフレーム数を取得する
     */
    int GetDamageShakeFrames() const {
        return damageShakeFrames_;
    }
    /**
     * @brief 被弾シェイクのフレーム数を設定する
     */
    void SetDamageShakeFrames(int val) {
        damageShakeFrames_ = val;
    }

    /**
     * @brief 被弾シェイクの周波数を取得する
     */
    float GetDamageShakeFrequency() const {
        return damageShakeFrequency_;
    }
    /**
     * @brief 被弾シェイクの周波数を設定する
     */
    void SetDamageShakeFrequency(float val) {
        damageShakeFrequency_ = val;
    }

    /**
     * @brief 撃破シェイクの強度を取得する
     */
    float GetDeathShakeIntensity() const {
        return deathShakeIntensity_;
    }
    /**
     * @brief 撃破シェイクの強度を設定する
     */
    void SetDeathShakeIntensity(float val) {
        deathShakeIntensity_ = val;
    }

    /**
     * @brief 撃破シェイクのフレーム数を取得する
     */
    int GetDeathShakeFrames() const {
        return deathShakeFrames_;
    }
    /**
     * @brief 撃破シェイクのフレーム数を設定する
     */
    void SetDeathShakeFrames(int val) {
        deathShakeFrames_ = val;
    }

    /**
     * @brief 撃破シェイクの周波数を取得する
     */
    float GetDeathShakeFrequency() const {
        return deathShakeFrequency_;
    }
    /**
     * @brief 撃破シェイクの周波数を設定する
     */
    void SetDeathShakeFrequency(float val) {
        deathShakeFrequency_ = val;
    }

private:
    /**
     * @brief メインカメラのゲームオブジェクトを取得する（弱参照キャッシュ付き）
     */
    std::shared_ptr<GameObject> GetMainCamera() const;

    /**
     * @brief メインカメラの CameraShakeComponent を取得する
     */
    CameraShakeComponent* GetCameraShake() const;

private:
    BossComponent* bossComp_ = nullptr;
    mutable std::weak_ptr<GameObject> mainCameraObj_;
    mutable CameraShakeComponent* cachedCameraShakeComp_ = nullptr;

    // 被弾シェイク
    float damageShakeIntensity_ = 0.4f;
    int damageShakeFrames_ = 10;
    float damageShakeFrequency_ = 15.0f;

    // 撃破シェイク
    float deathShakeIntensity_ = 2.0f;
    int deathShakeFrames_ = 60;
    float deathShakeFrequency_ = 10.0f;
};
