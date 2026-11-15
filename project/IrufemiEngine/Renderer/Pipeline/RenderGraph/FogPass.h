#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"

namespace Irufemi {

/**
 * @class FogPass
 * @brief 不透明オブジェクト描画後に大気・距離フォグを合成する独立レンダーパス
 * @details 深度バッファを参照し、全画面三角形のピクセルシェーダー（Fog.PS.hlsl）で
 *          シーン深度に応じたフォグカラーをメインレンダーターゲットにアルファ合成します。
 */
class FogPass : public IRenderPass {
public:
    FogPass() = default;
    ~FogPass() override = default;

    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(const RenderContext& rc) override;
};

} // namespace Irufemi
