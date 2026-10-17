#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"

namespace Irufemi {

/**
 * @class SeparableBlurEffect
 * @brief 分離可能ブラーエフェクト（平滑化およびガウスぼかしの水平/垂直2パス処理）
 */
class SeparableBlurEffect : public IPostProcessEffect {
public:
    explicit SeparableBlurEffect(PostProcessManager::Mode mode);
    ~SeparableBlurEffect() override = default;

    bool RequiresSeparatePass() const override {
        return true;
    }
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

private:
    PostProcessManager::Mode mode_;
    TransientResourceHandle blurIntermediateHandle_ = kInvalidHandle;
    RenderTexture* blurIntermediateTex_ = nullptr;
};

} // namespace Irufemi
