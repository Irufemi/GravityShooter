#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include <array>

namespace Irufemi {

/**
 * @class KawaseBlurEffect
 * @brief デュアルカワセブラー（ピラミッド型ダウンサンプリング ➔ アップサンプリング）
 */
class KawaseBlurEffect : public IPostProcessEffect {
public:
    KawaseBlurEffect() = default;
    ~KawaseBlurEffect() override = default;

    bool RequiresSeparatePass() const override {
        return true;
    }
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

private:
    std::array<TransientResourceHandle, PostProcessManager::kMaxKawaseIterations> handles_{};
    std::array<RenderTexture*, PostProcessManager::kMaxKawaseIterations> textures_{};
};

} // namespace Irufemi
