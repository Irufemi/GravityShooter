#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @class IScreenEffectDriver
 * @brief 個別ポストエフェクトのパラメータ管理・補間・シリアライズを担当する戦略インターフェース (Strategy Pattern)
 */
class IScreenEffectDriver {
public:
    virtual ~IScreenEffectDriver() = default;

    virtual PostProcessMode GetMode() const = 0;
    virtual void CacheBaseParams(PostProcessManager* ppm) = 0;
    virtual void ApplyInterpolation(PostProcessManager* ppm, float t) = 0;
    virtual void RestoreBaseParams(PostProcessManager* ppm) = 0;
    virtual nlohmann::json Serialize() const = 0;
    virtual void Deserialize(const nlohmann::json& j) = 0;
    virtual std::unique_ptr<IScreenEffectDriver> Clone() const = 0;
};

/**
 * @class GlitchEffectDriver
 * @brief グリッチエフェクト専用ドライバ
 */
class GlitchEffectDriver : public IScreenEffectDriver {
public:
    PostProcessMode GetMode() const override {
        return PostProcessMode::Glitch;
    }
    void CacheBaseParams(PostProcessManager* ppm) override;
    void ApplyInterpolation(PostProcessManager* ppm, float t) override;
    void RestoreBaseParams(PostProcessManager* ppm) override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    std::unique_ptr<IScreenEffectDriver> Clone() const override;

    PostProcessManager::GlitchParams& GetTargetParams() {
        return targetParams_;
    }
    const PostProcessManager::GlitchParams& GetTargetParams() const {
        return targetParams_;
    }
    void SetTargetParams(const PostProcessManager::GlitchParams& p) {
        targetParams_ = p;
    }

private:
    PostProcessManager::GlitchParams baseParams_{};
    PostProcessManager::GlitchParams targetParams_{};
};

/**
 * @class VignetteEffectDriver
 * @brief ビネットエフェクト専用ドライバ
 */
class VignetteEffectDriver : public IScreenEffectDriver {
public:
    PostProcessMode GetMode() const override {
        return PostProcessMode::Vignette;
    }
    void CacheBaseParams(PostProcessManager* ppm) override;
    void ApplyInterpolation(PostProcessManager* ppm, float t) override;
    void RestoreBaseParams(PostProcessManager* ppm) override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    std::unique_ptr<IScreenEffectDriver> Clone() const override;

    PostProcessManager::VignetteParams& GetTargetParams() {
        return targetParams_;
    }
    const PostProcessManager::VignetteParams& GetTargetParams() const {
        return targetParams_;
    }
    void SetTargetParams(const PostProcessManager::VignetteParams& p) {
        targetParams_ = p;
    }

private:
    PostProcessManager::VignetteParams baseParams_{};
    PostProcessManager::VignetteParams targetParams_{};
};

/**
 * @class ChromaticAberrationEffectDriver
 * @brief 色収差エフェクト専用ドライバ
 */
class ChromaticAberrationEffectDriver : public IScreenEffectDriver {
public:
    PostProcessMode GetMode() const override {
        return PostProcessMode::ChromaticAberration;
    }
    void CacheBaseParams(PostProcessManager* ppm) override;
    void ApplyInterpolation(PostProcessManager* ppm, float t) override;
    void RestoreBaseParams(PostProcessManager* ppm) override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    std::unique_ptr<IScreenEffectDriver> Clone() const override;

    PostProcessManager::ChromaticAberrationParams& GetTargetParams() {
        return targetParams_;
    }
    const PostProcessManager::ChromaticAberrationParams& GetTargetParams() const {
        return targetParams_;
    }
    void SetTargetParams(const PostProcessManager::ChromaticAberrationParams& p) {
        targetParams_ = p;
    }

private:
    PostProcessManager::ChromaticAberrationParams baseParams_{};
    PostProcessManager::ChromaticAberrationParams targetParams_{};
};

/**
 * @class RadialBlurEffectDriver
 * @brief 放射状ブラーエフェクト専用ドライバ
 */
class RadialBlurEffectDriver : public IScreenEffectDriver {
public:
    PostProcessMode GetMode() const override {
        return PostProcessMode::RadialBlur;
    }
    void CacheBaseParams(PostProcessManager* ppm) override;
    void ApplyInterpolation(PostProcessManager* ppm, float t) override;
    void RestoreBaseParams(PostProcessManager* ppm) override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    std::unique_ptr<IScreenEffectDriver> Clone() const override;

    PostProcessManager::RadialBlurParams& GetTargetParams() {
        return targetParams_;
    }
    const PostProcessManager::RadialBlurParams& GetTargetParams() const {
        return targetParams_;
    }
    void SetTargetParams(const PostProcessManager::RadialBlurParams& p) {
        targetParams_ = p;
    }

private:
    PostProcessManager::RadialBlurParams baseParams_{};
    PostProcessManager::RadialBlurParams targetParams_{};
};

/**
 * @class ScreenEffectDriverFactory
 * @brief ドライバ生成ファクトリ (Factory Pattern)
 */
class ScreenEffectDriverFactory {
public:
    static std::unique_ptr<IScreenEffectDriver> CreateDriver(PostProcessMode mode);
};
