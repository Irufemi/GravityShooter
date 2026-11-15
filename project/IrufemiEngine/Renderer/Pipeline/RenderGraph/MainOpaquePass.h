#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"

namespace Irufemi {

/**
 * @class MainOpaquePass
 * @brief 不透明オブジェクト（3Dメッシュ等）を描画するメインパス
 */
class MainOpaquePass : public IRenderPass {
public:
    ~MainOpaquePass() override = default;

    /**
     * @brief パスのセットアップ処理
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（不透明描画コマンドの発行）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;
};

} // namespace Irufemi
