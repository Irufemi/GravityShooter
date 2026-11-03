#pragma once

#include "Renderer/PostProcess/Effects/IPostProcessEffect.h"
#include "Renderer/PostProcess/PostProcessParams.h"
#include <vector>

namespace Irufemi {

/**
 * @class CombinedPostEffect
 * @brief 統合ポストプロセスエフェクト（Uber Shader による単一/複数エフェクトの一括合成）
 * @details シングルパス系の各種設定パラメータ（ToneMapping, HSV, Vignette, Outline等）を保持・管理
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

    // --- シングルパスパラメータ アクセサ ---
    ToneMappingParams& GetToneMappingParams() { return toneMappingParams_; }
    const ToneMappingParams& GetToneMappingParams() const { return toneMappingParams_; }
    void SetToneMappingParams(const ToneMappingParams& p) { toneMappingParams_ = p; }

    HSVParams& GetHSVParams() { return hsvParams_; }
    const HSVParams& GetHSVParams() const { return hsvParams_; }
    void SetHSVParams(const HSVParams& p) { hsvParams_ = p; }

    VignetteParams& GetVignetteParams() { return vignetteParams_; }
    const VignetteParams& GetVignetteParams() const { return vignetteParams_; }
    void SetVignetteParams(const VignetteParams& p) { vignetteParams_ = p; }

    OutlineParams& GetOutlineParams() { return outlineParams_; }
    const OutlineParams& GetOutlineParams() const { return outlineParams_; }
    void SetOutlineParams(const OutlineParams& p) { outlineParams_ = p; }

    /**
     * @brief 自身が保持するシングルパスパラメータをマネージャーの CombinedParams に流し込む
     */
    void SyncParamsToCombined(CombinedParams& combined) const;

private:
    std::vector<PostProcessManager::Mode> batch_;

    ToneMappingParams toneMappingParams_{};
    HSVParams hsvParams_{};
    VignetteParams vignetteParams_{};
    OutlineParams outlineParams_{};
};

} // namespace Irufemi
