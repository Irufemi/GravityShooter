#pragma once
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "Renderer/PostProcess/PostProcessManager.h"

struct IPostProcessSettings {
    virtual ~IPostProcessSettings() = default;

    // エフェクトの識別子
    virtual PostProcessMode GetMode() const = 0;

    // UI用の表示名
    virtual const char* GetName() const = 0;

    // PostProcessManagerへの適用
    virtual void ApplyToManager(class PostProcessManager* manager) = 0;

    // シリアライズ / デシリアライズ
    virtual void Serialize(nlohmann::json& j) const = 0;
    virtual void Deserialize(const nlohmann::json& j) = 0;

    // クローン
    virtual std::shared_ptr<IPostProcessSettings> Clone() const = 0;

    bool enabled = true;
};

// ---------------------------------------------------------
// Bloom Settings
// ---------------------------------------------------------
struct BloomSettings : public IPostProcessSettings {
    PostProcessMode GetMode() const override {
        return PostProcessMode::Bloom;
    }
    const char* GetName() const override {
        return "Bloom";
    }

    void ApplyToManager(PostProcessManager* manager) override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    std::shared_ptr<IPostProcessSettings> Clone() const override;

    float intensity = 1.8f;
    float threshold = 0.6f;
    float sigma = 4.0f;
};

// ---------------------------------------------------------
// Color Grading Settings (ToneMapping + HSV)
// ---------------------------------------------------------
struct ColorGradingSettings : public IPostProcessSettings {
    // 代表して ToneMapping を Mode として返す（UI等の識別用）
    PostProcessMode GetMode() const override {
        return PostProcessMode::ToneMapping;
    }
    const char* GetName() const override {
        return "Color Grading";
    }

    void ApplyToManager(PostProcessManager* manager) override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    std::shared_ptr<IPostProcessSettings> Clone() const override;

    float exposure = 1.0f;
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
};

// ---------------------------------------------------------
// Vignette Settings
// ---------------------------------------------------------
struct VignetteSettings : public IPostProcessSettings {
    PostProcessMode GetMode() const override {
        return PostProcessMode::Vignette;
    }
    const char* GetName() const override {
        return "Vignette";
    }

    void ApplyToManager(PostProcessManager* manager) override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    std::shared_ptr<IPostProcessSettings> Clone() const override;

    float radius = 0.8f;
    float softness = 0.5f;
};

// ---------------------------------------------------------
// Outline Settings
// ---------------------------------------------------------
/**
 * @brief アウトライン描画設定構造体
 */
struct OutlineSettings : public IPostProcessSettings {
    PostProcessMode GetMode() const override {
        return PostProcessMode::DepthBasedOutline;
    }
    const char* GetName() const override {
        return "Outline";
    }

    void ApplyToManager(PostProcessManager* manager) override;
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    std::shared_ptr<IPostProcessSettings> Clone() const override;

    float intensity = 6.0f;    ///< アウトライン強度
    int32_t maskMaxRadius = 5; ///< マスクベースアウトラインの最大探索半径
};

// ---------------------------------------------------------
// Factory
// ---------------------------------------------------------
class PostProcessSettingsFactory {
public:
    static std::shared_ptr<IPostProcessSettings> Create(PostProcessMode mode);
};
