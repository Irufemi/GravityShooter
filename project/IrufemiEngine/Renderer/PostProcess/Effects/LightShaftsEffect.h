#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"

namespace Irufemi {

/**
 * @class LightShaftsEffect
 * @brief ゴッドレイ（光の筋）エフェクト（抽出 ➔ ラジアルブラー ➔ 加算合成）
 */
class LightShaftsEffect : public IPostProcessEffect {
public:
    LightShaftsEffect() = default;
    ~LightShaftsEffect() override = default;

    bool RequiresSeparatePass() const override { return true; }
    void Render(const PostProcessRenderContext& context) override;
};

} // namespace Irufemi
