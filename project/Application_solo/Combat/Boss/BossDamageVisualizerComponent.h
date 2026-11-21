#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include <memory>

class BossComponent;
namespace Irufemi {
class GameObject;
}
namespace Irufemi {
class CameraShakeComponent;
class CameraComponent;
} // namespace Irufemi

/**
 * @class BossDamageVisualizerComponent
 * @brief ボスの被弾演出（カメラシェイク）および撃破演出を管理するコンポーネント
 */
class BossDamageVisualizerComponent : public Irufemi::Component {
public:
    BossDamageVisualizerComponent() = default;
    ~BossDamageVisualizerComponent() override;

    void Initialize() override;
    void Start() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "BossDamageVisualizerComponent";
    }

    /**
     * @brief メインカメラのゲームオブジェクトを取得する（弱参照キャッシュ付き）
     */
    std::shared_ptr<Irufemi::GameObject> GetMainCamera() const;

    /**
     * @brief メインカメラの CameraComponent を取得する
     */
    Irufemi::CameraComponent* GetMainCameraComponent() const;

    /**
     * @brief メインカメラの Irufemi::CameraShakeComponent を取得する
     */
    Irufemi::CameraShakeComponent* GetCameraShake() const;

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
     * @brief 被弾シェイクの持続時間（秒）を取得する
     */
    float GetDamageShakeDuration() const {
        return damageShakeDuration_;
    }
    /**
     * @brief 被弾シェイクの持続時間（秒）を設定する
     */
    void SetDamageShakeDuration(float val) {
        damageShakeDuration_ = val;
    }

    /**
     * @brief 被弾シェイクのフレーム数を取得する（互換用）
     */
    int GetDamageShakeFrames() const {
        return static_cast<int>(damageShakeDuration_ * 60.0f);
    }
    /**
     * @brief 被弾シェイクのフレーム数を設定する（互換用）
     */
    void SetDamageShakeFrames(int val) {
        damageShakeDuration_ = static_cast<float>(val) / 60.0f;
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
     * @brief 撃破シェイクの持続時間（秒）を取得する
     */
    float GetDeathShakeDuration() const {
        return deathShakeDuration_;
    }
    /**
     * @brief 撃破シェイクの持続時間（秒）を設定する
     */
    void SetDeathShakeDuration(float val) {
        deathShakeDuration_ = val;
    }

    /**
     * @brief 撃破シェイクのフレーム数を取得する（互換用）
     */
    int GetDeathShakeFrames() const {
        return static_cast<int>(deathShakeDuration_ * 60.0f);
    }
    /**
     * @brief 撃破シェイクのフレーム数を設定する（互換用）
     */
    void SetDeathShakeFrames(int val) {
        deathShakeDuration_ = static_cast<float>(val) / 60.0f;
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

    /**
     * @brief 撃破時の稲妻放電カラー（ハロ発光色）を取得する
     */
    const Irufemi::Vector4& GetDeathLightningColor() const {
        return deathLightningColor_;
    }
    void SetDeathLightningColor(const Irufemi::Vector4& color) {
        deathLightningColor_ = color;
    }

    /**
     * @brief 撃破時のEMPドーナツ爆炎カラーを取得する
     */
    const Irufemi::Vector4& GetDeathBlastColor() const {
        return deathBlastColor_;
    }
    void SetDeathBlastColor(const Irufemi::Vector4& color) {
        deathBlastColor_ = color;
    }

    /**
     * @brief 撃破時のVoxel爆縮・爆散プラズマカラーを取得する
     */
    const Irufemi::Vector4& GetDeathVoxelColor() const {
        return deathVoxelColor_;
    }
    void SetDeathVoxelColor(const Irufemi::Vector4& color) {
        deathVoxelColor_ = color;
    }

private:
    /**
     * @brief 登録済みのイベントリスナーを解除する
     */
    void UnregisterListeners();

private:
    BossComponent* bossComp_ = nullptr;
    mutable std::weak_ptr<Irufemi::GameObject> mainCameraObj_;

    uint32_t damageListenerId_ = 0;
    uint32_t deathListenerId_ = 0;

    // 被弾シェイク
    float damageShakeIntensity_ = 0.4f;
    float damageShakeDuration_ = 0.16f;
    float damageShakeFrequency_ = 15.0f;

    // 撃破シェイク
    float deathShakeIntensity_ = 2.0f;
    float deathShakeDuration_ = 1.0f;
    float deathShakeFrequency_ = 10.0f;

    // 撃破演出カラーパレット（ボス固有色：ネオンマゼンタ＆クリムゾン）
    Irufemi::Vector4 deathLightningColor_ = {2.2f, 0.2f, 2.8f, 1.0f}; ///< 撃破時放電カラー（高圧マゼンタ紫電）
    Irufemi::Vector4 deathBlastColor_     = {3.8f, 0.3f, 4.5f, 1.0f}; ///< 撃破時爆炎波面カラー（ヴォイドマゼンタ）
    Irufemi::Vector4 deathVoxelColor_     = {9.5f, 2.0f, 8.0f, 1.0f}; ///< 撃破時Voxelプラズマカラー（暴走マゼンタ）
};
