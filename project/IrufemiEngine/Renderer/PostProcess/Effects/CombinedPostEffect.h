#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include <vector>

namespace Irufemi {

/**
 * @class CombinedPostEffect
 * @brief 統合ポストプロセスエフェクト（Uber Shader による単一/複数エフェクトの一括合成）
 */
class CombinedPostEffect : public IPostProcessEffect {
public:
    CombinedPostEffect() = default;
    ~CombinedPostEffect() override = default;

    /**
     * @brief バッチ実行するエフェクト一覧を設定する
     */
    void SetBatch(const std::vector<PostProcessManager::Mode>& batch) {
        batch_ = batch;
    }

    /**
     * @brief バッチをクリアする
     */
    void ClearBatch() {
        batch_.clear();
    }

    bool RequiresSeparatePass() const override {
        return false;
    }
    void Render(const PostProcessRenderContext& context) override;
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen) override;

    /**
     * @brief バッチおよびマスクフラグを直接指定してパスを構築する
     */
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager, TransientResourceHandle inputHandle,
                        TransientResourceHandle outputHandle, bool writeToScreen,
                        const std::vector<PostProcessManager::Mode>& batch, bool useMask);

    /**
     * @brief 指定したバッチ情報を用いて描画を実行する
     */
    void RenderWithBatch(const PostProcessRenderContext& context, const std::vector<PostProcessManager::Mode>& batch);

    /**
     * @brief エフェクトなしの画面コピー（Final Pass）を描画する
     */
    static void RenderFinalCopyPass(const PostProcessRenderContext& context);

private:
    std::vector<PostProcessManager::Mode> batch_;
};

} // namespace Irufemi
