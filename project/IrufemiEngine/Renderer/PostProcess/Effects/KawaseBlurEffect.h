#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <array>

class DirectXCommon;

namespace Irufemi {

/**
 * @class KawaseBlurEffect
 * @brief デュアルカワセブラー（ピラミッド型ダウンサンプリング ➔ アップサンプリング）
 */
class KawaseBlurEffect : public IPostProcessEffect {
public:
    KawaseBlurEffect() = default;
    ~KawaseBlurEffect() override = default;

    /**
     * @brief 専用PSOおよび定数バッファの自己完結初期化
     */
    void Initialize(ID3D12Device* device, DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                    D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat);

    bool RequiresSeparatePass() const override {
        return true;
    }
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Reset() override { params_ = DualKawaseBlurParams{}; }
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

    DualKawaseBlurParams& GetParams() { return params_; }
    const DualKawaseBlurParams& GetParams() const { return params_; }
    void SetParams(const DualKawaseBlurParams& params) { params_ = params; }

    ID3D12Resource* GetConstantBuffer() const { return constantBuffer_.Get(); }

private:
    DualKawaseBlurParams params_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    DualKawaseBlurParams* mappedParams_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> downsamplePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> upsamplePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalUpsamplePSO_;

    std::array<TransientResourceHandle, PostProcessManager::kMaxKawaseIterations> handles_{};
    std::array<RenderTexture*, PostProcessManager::kMaxKawaseIterations> textures_{};
};

} // namespace Irufemi
