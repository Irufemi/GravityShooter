#pragma once

namespace Irufemi {

// 前方宣言
class RenderGraphBuilder;
struct RenderContext;

/**
 * @class IRenderPass
 * @brief すべての描画パスの基底となるインターフェース
 * @details レンダーグラフに登録され、特定の描画工程（シャドウ、不透明、半透明など）をカプセル化します。
 */
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    /**
     * @brief パスのセットアップ処理
     * @details リソースバリアの自動解決のため、入力・出力リソースの要求ステートを登録します。
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    virtual void Setup(RenderGraphBuilder& builder, const RenderContext& rc) {}

    /**
     * @brief パスの実行処理
     * @param[in] rc 描画コンテキスト
     * @details 実際の DrawCall（描画コマンドの積み込み）をこの中で行います。
     */
    virtual void Execute(const RenderContext& rc) = 0;
};

} // namespace Irufemi
