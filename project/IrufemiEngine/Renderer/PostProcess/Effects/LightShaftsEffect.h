#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <d3d12.h>
#include <wrl/client.h>

class DirectXCommon;

namespace Irufemi {

/**
 * @class LightShaftsEffect
 * @brief ゴッドレイ（光の筋）エフェクト（抽出 ➔ ラジアルブラー ➔ 加算合成）
 * @details 自身の定数バッファおよび専用パイプラインステートを自己管理する自律型エフェクト
 */
class LightShaftsEffect : public IPostProcessEffect {
public:
    LightShaftsEffect() = default;
    ~LightShaftsEffect() override = default;

    PostProcessMode GetMode() const {
        return PostProcessMode::LightShafts;
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

    LightShaftsParams& GetParams() { return params_; }
    const LightShaftsParams& GetParams() const { return params_; }
    void SetParams(const LightShaftsParams& params);

    ID3D12PipelineState* GetExtractPSO() const { return extractPSO_.Get(); }
    ID3D12PipelineState* GetRadialBlurPSO() const { return radialBlurPSO_.Get(); }
    ID3D12PipelineState* GetCombinePSO() const { return combinePSO_.Get(); }
    ID3D12PipelineState* GetFinalCombinePSO() const { return finalCombinePSO_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress() const {
        return constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;
    }

private:
    LightShaftsParams params_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    LightShaftsParams* mappedParams_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> extractPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> combinePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalCombinePSO_;

    TransientResourceHandle lsExtractHandle_ = kInvalidHandle;
    TransientResourceHandle lsBlurHandle_ = kInvalidHandle;
    RenderTexture* lsExtract_ = nullptr;
    RenderTexture* lsBlur_ = nullptr;
};

} // namespace Irufemi
