#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"

namespace Irufemi {

/**
 * @class PostProcessCopyPass
 * @brief メインレンダーテクスチャをポストプロセス入力用の一時テクスチャへ安全に複製するパス
 * @details RenderGraph のステート自動追跡により、COPY_SOURCE / COPY_DEST のバリアを手動発行なしで実行します。
 */
class PostProcessCopyPass : public IRenderPass {
public:
    explicit PostProcessCopyPass(const std::string& targetName = "PP_Src",
                                 TransientResourceHandle dstHandle = kInvalidHandle);
    ~PostProcessCopyPass() override = default;

    void Setup(RenderGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(const RenderContext& rc) override;

    TransientResourceHandle GetOutputHandle() const {
        return dstHandle_;
    }

private:
    std::string targetName_;
    TransientResourceHandle dstHandle_ = kInvalidHandle;
};

} // namespace Irufemi
