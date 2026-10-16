#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"

namespace Irufemi {

/**
 * @class SeparableBlurEffect
 * @brief 分離可能ブラーエフェクト（平滑化およびガウスぼかしの水平/垂直2パス処理）
 */
class SeparableBlurEffect : public IPostProcessEffect {
public:
    explicit SeparableBlurEffect(PostProcessManager::Mode mode);
    ~SeparableBlurEffect() override = default;

    bool RequiresSeparatePass() const override { return true; }
    void Render(const PostProcessRenderContext& context) override;

private:
    PostProcessManager::Mode mode_;
};

} // namespace Irufemi
