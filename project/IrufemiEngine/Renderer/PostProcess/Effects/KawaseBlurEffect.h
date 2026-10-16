#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"

namespace Irufemi {

/**
 * @class KawaseBlurEffect
 * @brief デュアルカワセブラー（ピラミッド型ダウンサンプリング ➔ アップサンプリング）
 */
class KawaseBlurEffect : public IPostProcessEffect {
public:
    KawaseBlurEffect() = default;
    ~KawaseBlurEffect() override = default;

    bool RequiresSeparatePass() const override { return true; }
    void Render(const PostProcessRenderContext& context) override;
};

} // namespace Irufemi
