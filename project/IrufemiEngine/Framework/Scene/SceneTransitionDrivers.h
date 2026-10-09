#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <vector>
#include <memory>

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
    explicit RadialBlurTransitionDriver(bool isWhite = false) : isWhite_(isWhite) {}
    void OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) override;
    void OnUpdate(PostProcessManager* ppm, float factor) override;

private:
    bool isWhite_ = false;
};
