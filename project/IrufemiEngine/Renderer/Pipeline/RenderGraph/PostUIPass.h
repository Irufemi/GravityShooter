#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include <vector>
#include <array>

/**
 * @class PostUIPass
 * @brief UI描画後のポストプロセス処理（画面全体の最終ポストエフェクト等）を実行するパス
 */
class PostUIPass : public IRenderPass {
public:
    PostUIPass() = default;
    ~PostUIPass() override = default;

    /**
     * @brief パスのセットアップ処理（ポストUI用の一時テクスチャ確保・要求）
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(class RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) override;

    /**
     * @brief パスの実行処理（ポストUIエフェクトの描画）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const Irufemi::RenderContext& rc) override;

private:
    TransientResourceHandle postUiSrcHandle_ = kInvalidHandle;
};
