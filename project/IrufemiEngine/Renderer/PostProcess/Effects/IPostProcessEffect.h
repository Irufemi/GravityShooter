#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include <d3d12.h>

class RenderTexture;
class RenderGraph;

namespace Irufemi {

/**
 * @struct PostProcessRenderContext
 * @brief 各ポストプロセスエフェクトの描画に必要なコンテキスト情報
 */
struct PostProcessRenderContext {
    PostProcessManager* manager = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    RenderTexture* sourceTexture = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
    bool writeToScreen = false;
    bool isBackBufferTarget = false;
    const D3D12_VIEWPORT* targetViewport = nullptr;
    const D3D12_RECT* targetScissor = nullptr;
};

/**
 * @class IPostProcessEffect
 * @brief 各ポストプロセスエフェクトの描画戦略インターフェース
 */
class IPostProcessEffect {
public:
    virtual ~IPostProcessEffect() = default;

    /**
     * @brief このエフェクトが独立した描画パスを要求するかどうか
     */
    virtual bool RequiresSeparatePass() const {
        return false;
    }

    /**
     * @brief このエフェクトが必要とする一時リソースを申告する
     * @param builder RenderGraphビルダー
     * @param baseDesc 入力リソースの基本デスクリプション
     */
    virtual void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc) {}

    /**
     * @brief RenderGraphから一時リソースの解決（ポインタ取得）を行う
     * @param renderGraph RenderGraphインスタンス
     */
    virtual void ResolveResources(RenderGraph* renderGraph) {}

    /**
     * @brief エフェクトの描画コマンドを発行する
     * @param context 描画コンテキスト
     */
    virtual void Render(const PostProcessRenderContext& context) = 0;

    /**
     * @brief このエフェクトの描画工程を RenderGraph にパスとして登録する
     */
    virtual void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                                TransientResourceHandle inputHandle, TransientResourceHandle outputHandle,
                                bool writeToScreen) {}
};

} // namespace Irufemi
