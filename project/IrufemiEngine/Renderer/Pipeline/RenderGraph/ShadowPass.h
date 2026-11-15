#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"

namespace Irufemi {

/**
 * @class ShadowPass
 * @brief シャドウマップを生成するための深度描画パス
 */
class ShadowPass : public IRenderPass {
public:
    ~ShadowPass() override = default;

    /**
     * @brief パスのセットアップ処理
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（シャドウマップ深度描画コマンドの発行）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;
};

} // namespace Irufemi
