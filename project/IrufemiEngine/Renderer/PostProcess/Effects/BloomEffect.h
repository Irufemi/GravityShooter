#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <d3d12.h>
#include <wrl/client.h>

class DirectXCommon;

namespace Irufemi {

/**
 * @class BloomEffect
 * @brief ブルームエフェクト（高輝度抽出 ➔ ガウスブラー水平/垂直 ➔ 加算合成）
 * @details 自身の定数バッファおよび専用パイプラインステートを自己管理する自律型エフェクト
 */
class BloomEffect : public IPostProcessEffect {
public:
    BloomEffect() = default;
    ~BloomEffect() override = default;

    PostProcessMode GetMode() const {
        return PostProcessMode::Bloom;
    }

    bool RequiresSeparatePass() const override {
        return true;
    }

    /**
     * @brief 専用パイプラインステートおよび定数バッファの初期化
     */
    void Initialize(ID3D12Device* device, DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                    D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat);

    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) override;
    void ResolveResources(RenderGraph* renderGraph) override;
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

    BloomParams& GetParams() { return params_; }
    const BloomParams& GetParams() const { return params_; }
    void SetParams(const BloomParams& params);

    ID3D12PipelineState* GetExtractPSO() const { return extractPSO_.Get(); }
    ID3D12PipelineState* GetBlurHPSO() const { return blurHPSO_.Get(); }
    ID3D12PipelineState* GetBlurVPSO() const { return blurVPSO_.Get(); }
    ID3D12PipelineState* GetCombinePSO() const { return combinePSO_.Get(); }
    ID3D12PipelineState* GetFinalCombinePSO() const { return finalCombinePSO_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress() const {
        return constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;
    }

private:
    BloomParams params_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    BloomParams* mappedParams_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> extractPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurHPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurVPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> combinePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalCombinePSO_;

    TransientResourceHandle extractHandle_ = kInvalidHandle;
    TransientResourceHandle blurHandle_ = kInvalidHandle;
    RenderTexture* extractTex_ = nullptr;
    RenderTexture* blurTex_ = nullptr;
};

} // namespace Irufemi
