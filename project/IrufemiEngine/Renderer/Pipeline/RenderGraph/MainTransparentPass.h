#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include <cstdint>
#include <vector>

namespace Irufemi {

class MainTransparentPass : public IRenderPass {
public:
    /**
     * @brief パスのセットアップ処理（半透明描画パスで必要なリソース状態やバリアをレンダーグラフに登録する）
     * @param[in,out] builder リソース使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（カメラ距離による奥から手前のソートおよび半透明描画）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;

private:
    /// @brief 半透明描画のインデックスソート用軽量キー (8 bytes)
    struct TransparentSortKey {
        float distanceToCamera;
        uint32_t packetIndex;
    };

    /// @brief 毎フレームのヒープアロケーションを防ぐための再利用ソートキーバッファ
    std::vector<TransparentSortKey> sortKeys_;
};

} // namespace Irufemi
