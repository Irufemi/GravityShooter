#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include <cstdint>

namespace Irufemi {

/**
 * @class SelectionOutlinePass
 * @brief 選択中のオブジェクトのマスクを描画し、シルエットのアウトラインを合成するパス
 */
class SelectionOutlinePass : public IRenderPass {
public:
    ~SelectionOutlinePass() override = default;

    /**
     * @brief パスのセットアップ処理（選択マスク用一時テクスチャの確保・要求）
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（マスク描画およびアウトライン合成）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;

private:
    uint32_t maskHandle_ = static_cast<uint32_t>(-1);
};

} // namespace Irufemi
