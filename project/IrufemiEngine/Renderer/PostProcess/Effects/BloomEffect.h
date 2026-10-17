#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"

namespace Irufemi {

/**
 * @class BloomEffect
 * @brief ブルームエフェクト（高輝度抽出 ➔ ガウスブラー水平/垂直 ➔ 加算合成）
 */
class BloomEffect : public IPostProcessEffect {
public:
    BloomEffect() = default;
    ~BloomEffect() override = default;

    bool RequiresSeparatePass() const override {
        return true;
    }
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

private:
    TransientResourceHandle extractHandle_ = kInvalidHandle;
    TransientResourceHandle blurHandle_ = kInvalidHandle;
    RenderTexture* extractTex_ = nullptr;
    RenderTexture* blurTex_ = nullptr;
};

} // namespace Irufemi
