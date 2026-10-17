#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"

namespace Irufemi {

/**
 * @class LightShaftsEffect
 * @brief ゴッドレイ（光の筋）エフェクト（抽出 ➔ ラジアルブラー ➔ 加算合成）
 */
class LightShaftsEffect : public IPostProcessEffect {
public:
    LightShaftsEffect() = default;
    ~LightShaftsEffect() override = default;

    bool RequiresSeparatePass() const override {
        return true;
    }
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

private:
    TransientResourceHandle lsExtractHandle_ = kInvalidHandle;
    TransientResourceHandle lsBlurHandle_ = kInvalidHandle;
    RenderTexture* lsExtract_ = nullptr;
    RenderTexture* lsBlur_ = nullptr;
};

} // namespace Irufemi
