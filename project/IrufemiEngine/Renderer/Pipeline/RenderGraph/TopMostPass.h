#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"

namespace Irufemi {

/**
 * @class TopMostPass
 * @brief 最前面UI（デバッグ用テキストやトランジション等）を描画するパス
 * @details ポストプロセスのさらに後、バックバッファ（あるいは最終画面）に直接描画します。
 */
class TopMostPass : public IRenderPass {
public:
    ~TopMostPass() override = default;

    /**
     * @brief パスのセットアップ処理
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（最前面UIの描画コマンド発行）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;
};

} // namespace Irufemi
