#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"

namespace Irufemi {

/**
 * @class ComputePass
 * @brief RenderGraph の最初のパスとして、コンピュートシェーダを一括実行するパス
 */
class ComputePass : public IRenderPass {
public:
    ~ComputePass() override = default;

    /**
     * @brief パスのセットアップ処理
     * @param[in,out] builder リソース使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（GPUコンピュートタスクの一括ディスパッチ）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;
};

} // namespace Irufemi
