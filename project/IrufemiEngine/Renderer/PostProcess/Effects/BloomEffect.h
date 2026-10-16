#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"

namespace Irufemi {

/**
 * @class BloomEffect
 * @brief ブルームエフェクト（高輝度抽出 ➔ ガウスブラー水平/垂直 ➔ 加算合成）
 */
class BloomEffect : public IPostProcessEffect {
public:
    BloomEffect() = default;
    ~BloomEffect() override = default;

    bool RequiresSeparatePass() const override { return true; }
    void Render(const PostProcessRenderContext& context) override;
};

} // namespace Irufemi
