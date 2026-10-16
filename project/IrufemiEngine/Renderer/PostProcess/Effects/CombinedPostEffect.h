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

    bool RequiresSeparatePass() const override { return false; }
    void Render(const PostProcessRenderContext& context) override;

    /**
     * @brief エフェクトなしの画面コピー（Final Pass）を描画する
     */
    static void RenderFinalCopyPass(const PostProcessRenderContext& context);

private:
    std::vector<PostProcessManager::Mode> batch_;
};

} // namespace Irufemi
