#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <vector>

namespace Irufemi {

class CameraComponent;
class PerlinNoise;

/**
 * @brief カメラシェイクの再生イベント情報
 */
struct ShakeEvent {
    float maxIntensity = 0.0f;                        ///< 最大振幅
    float duration = 0.0f;                            ///< 継続時間（秒）
    float currentTime = 0.0f;                         ///< 経過時間（秒）
    float frequency = 10.0f;                          ///< 揺れの周波数
    Irufemi::Vector3 axisIntensity{1.0f, 1.0f, 0.5f}; ///< 軸ごとの揺れ比率
    unsigned int seed = 0;                            ///< ノイズシード値
};

/**
 * @class CameraShakeComponent
 * @brief AAA品質のカメラシェイク（多重加算、パーリンノイズ、イージング減衰）を実現するコンポーネント
 */
class CameraShakeComponent : public Component {
public:
    CameraShakeComponent();
    ~CameraShakeComponent() override;

    void Initialize() override;
    void OnAwake() override;
    void Update() override;

    std::string GetComponentName() const override {
        return "CameraShakeComponent";
    }

    /**
     * @brief カメラシェイクを再生します
     * @param intensity 揺れの強さ (振幅)
     * @param durationFrames 揺れの長さ (フレーム数。内部で秒に変換)
     * @param frequency 揺れの速さ (周波数)
     */
    void PlayShake(float intensity, int durationFrames, float frequency = 10.0f);

    /**
     * @brief カメラシェイクを再生します (秒指定)
     */
    void PlayShakeSeconds(float intensity, float durationSeconds, float frequency = 10.0f);

    /**
     * @brief 現在シェイクが再生中かどうかを判定する
     * @return 再生中ならtrue
     */
    bool IsPlaying() const {
        return !activeShakes_.empty();
    }

protected:
    void OnRegisterProperties() override;

private:
    std::vector<ShakeEvent> activeShakes_;
    CameraComponent* cameraComp_ = nullptr;
    std::shared_ptr<Irufemi::PerlinNoise> perlinNoise_;

    float globalIntensityMultiplier_ = 1.0f;
};

} // namespace Irufemi
