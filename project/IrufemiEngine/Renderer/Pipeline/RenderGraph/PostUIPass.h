#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Core/Math/Math.h"
#include <wrl/client.h>
#include <vector>
#include <array>

namespace Irufemi {

/**
 * @class PostUIPass
 * @brief UI描画後のポストプロセス処理（画面遷移暗転フェード等のオーバーレイ）を実行するパス
 */
class PostUIPass : public IRenderPass {
public:
    PostUIPass() = default;
    ~PostUIPass() override = default;

    /**
     * @brief パスのセットアップ処理
     * @param[in,out] builder リソースの使用状態を記録するビルダー
     * @param[in] rc 描画コンテキスト
     */
    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;

    /**
     * @brief パスの実行処理（画面遷移フェード等の描画）
     * @param[in] rc 描画コンテキスト
     */
    void Execute(const RenderContext& rc) override;

private:
    struct FadeConstantBuffer {
        Vector4 color{0.0f, 0.0f, 0.0f, 1.0f};
        float intensity = 0.0f;
        float padding[3]{};
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> fadeConstantBuffer_;
    FadeConstantBuffer* mappedFadeBuffer_ = nullptr;
};

} // namespace Irufemi
