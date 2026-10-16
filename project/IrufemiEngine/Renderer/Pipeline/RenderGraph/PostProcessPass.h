#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include <vector>
#include <array>

class PostProcessPass : public IRenderPass {
public:
    PostProcessPass() = default;
    ~PostProcessPass() override = default;

    /**
     * @brief パスのセットアップ処理（ブルームやブラー等の一時レンダーターゲット確保とバリア設定）
     * @param[in,out] builder リソース使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(class RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) override;

    /**
     * @brief パスの実行処理（各種ポストエフェクトの描画・合成）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const Irufemi::RenderContext& rc) override;

private:
    TransientResourceHandle preUiSrcHandle_ = kInvalidHandle;
};
