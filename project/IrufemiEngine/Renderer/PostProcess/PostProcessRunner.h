#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <d3d12.h>
#include <vector>
#include <memory>

namespace Irufemi {
class IPostProcessEffect;
class BloomEffect;
class LightShaftsEffect;
class SeparableBlurEffect;
class KawaseBlurEffect;
class CombinedPostEffect;
} // namespace Irufemi

/**
 * @class PostProcessRunner
 * @brief エフェクトのチェイン実行（Ping-Pong描画）に特化した軽量ディスパッチャ
 */
class PostProcessRunner {
public:
    PostProcessRunner();
    ~PostProcessRunner();

    /**
     * @brief Ping-Pong方式で指定されたエフェクト配列を順次ディスパッチする
     * @param manager PostProcessManagerのポインタ (パラメータやPSO取得用)
     * @param commandList コマンドリスト
     * @param modes 実行するエフェクトのリスト
     * @param srcTexture 最初の入力テクスチャ
     * @param rtvHandle 最終出力先のレンダーターゲットビュー (isFinalOutput=trueの場合に使用)
     * @param workspace Ping-Pong用の2枚のテクスチャなどのワークスペース
     * @param isFinalOutput このランナーの最後がバックバッファへの出力かどうか
     * @param isBackBufferTarget バックバッファ描画フラグ
     * @param targetViewport 明示的ビューポート
     * @param targetScissor 明示的シザー矩形
     * @return 最後の出力結果が書き込まれた RenderTexture のポインタ（スワップチェーン出力なら nullptr）
     */
    class RenderTexture* Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                             const std::vector<PostProcessManager::Mode>& modes, class RenderTexture* srcTexture,
                             D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                             const PostProcessManager::PostProcessWorkspace& workspace, bool isFinalOutput = true,
                             bool isBackBufferTarget = false, const D3D12_VIEWPORT* targetViewport = nullptr,
                             const D3D12_RECT* targetScissor = nullptr);

private:
    bool RequiresSeparatePass(PostProcessManager::Mode mode) const;

    std::unique_ptr<Irufemi::BloomEffect> bloomEffect_;
    std::unique_ptr<Irufemi::LightShaftsEffect> lightShaftsEffect_;
    std::unique_ptr<Irufemi::SeparableBlurEffect> smoothingEffect_;
    std::unique_ptr<Irufemi::SeparableBlurEffect> gaussianEffect_;
    std::unique_ptr<Irufemi::KawaseBlurEffect> kawaseBlurEffect_;
    std::unique_ptr<Irufemi::CombinedPostEffect> combinedEffect_;
};
