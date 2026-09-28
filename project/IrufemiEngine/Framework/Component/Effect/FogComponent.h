#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"

/**
 * @class FogComponent
 * @brief シーンの大気・距離フォグを制御するコンポーネント
 */
class FogComponent : public Component {
public:
    FogComponent() = default;
    ~FogComponent() override = default;

    void Start() override;
    void Update() override;
    void OnDisable() override;
    void OnDestroy() override;
    void OnRegisterProperties() override;

    bool CanUpdateInEditMode() const override {
        return true;
    }

    std::string GetComponentName() const override {
        return "FogComponent";
    }

    std::shared_ptr<Component> Clone() override;

    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    // ゲッター・セッター
    const Irufemi::Vector3& GetFogColor() const {
        return fogColor_;
    }
    void SetFogColor(const Irufemi::Vector3& color) {
        fogColor_ = color;
    }

    float GetFogStart() const {
        return fogStart_;
    }
    void SetFogStart(float start) {
        fogStart_ = start;
    }

    float GetFogEnd() const {
        return fogEnd_;
    }
    void SetFogEnd(float end) {
        fogEnd_ = end;
    }

    float GetFogDensity() const {
        return fogDensity_;
    }
    void SetFogDensity(float density) {
        fogDensity_ = density;
    }

    int GetFogType() const {
        return fogType_;
    }
    void SetFogType(int type) {
        fogType_ = type;
    }

    bool IsFogEnabled() const {
        return enabled_;
    }
    void SetFogEnabled(bool enabled) {
        enabled_ = enabled;
    }

private:
    void SyncToEngine();

private:
    Irufemi::Vector3 fogColor_ = {0.65f, 0.70f, 0.85f}; ///< フォグ色 (淡いラベンダーブルー)
    float fogStart_ = 200.0f;                            ///< 開始距離 (m)
    float fogEnd_ = 2000.0f;                             ///< 終了距離 (m)
    float fogDensity_ = 1.0f;                            ///< 密度
    int fogType_ = 0;                                    ///< 0: Linear, 1: Exponential
    bool enabled_ = true;                                ///< 有効/無効フラグ
};
