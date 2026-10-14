#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include "Core/Math/Vector4.h"
#include <vector>
#include <memory>
#include <optional>

/**
 * @class ITransitionDriver
 * @brief シーン遷移演出の個別エフェクト制御を担当する戦略インターフェース (Strategy Pattern)
 */
class ITransitionDriver {
public:
    virtual ~ITransitionDriver() = default;

    virtual void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) = 0;
    virtual void OnUpdate(PostProcessManager* ppm, float factor) = 0;
};

/**
 * @class FadeTransitionDriver
 * @brief フェード遷移ドライバ
 */
class FadeTransitionDriver : public ITransitionDriver {
public:
    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;
};

/**
 * @class DissolveTransitionDriver
 * @brief ディゾルブ遷移ドライバ
 */
class DissolveTransitionDriver : public ITransitionDriver {
public:
    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;
};

/**
 * @class SlideTransitionDriver
 * @brief スライド遷移ドライバ
 */
class SlideTransitionDriver : public ITransitionDriver {
public:
    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;
};

/**
 * @class RadialBlurTransitionDriver
 * @brief 放射状ブラー遷移ドライバ（黒 / 白 両対応）
 */
class RadialBlurTransitionDriver : public ITransitionDriver {
public:
    explicit RadialBlurTransitionDriver(bool isWhite = false,
                                        const std::optional<Irufemi::Vector4>& customColor = std::nullopt)
        : isWhite_(isWhite), customColor_(customColor) {}

    void SetCustomColor(const std::optional<Irufemi::Vector4>& color) {
        customColor_ = color;
    }

    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;

private:
    bool isWhite_ = false;
    std::optional<Irufemi::Vector4> customColor_;
};

/**
 * @class GravitySingularityTransitionDriver
 * @brief 重力特異点跳躍（Gravity Singularity）遷移ドライバ
 * @details 空間歪み（RadialBlur）、重力赤方偏移（ChromaticAberration）、事象の地平線（Fade）、
 *          および 2D GPGPU 流体シミュレーション（FluidSimulation2D）の重力渦インパルスを統合制御します。
 */
class GravitySingularityTransitionDriver : public ITransitionDriver {
public:
    explicit GravitySingularityTransitionDriver(const std::optional<Irufemi::Vector4>& customColor = std::nullopt)
        : customColor_(customColor) {}

    void SetCustomColor(const std::optional<Irufemi::Vector4>& color) {
        customColor_ = color;
    }

    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;

private:
    std::optional<Irufemi::Vector4> customColor_;
};
