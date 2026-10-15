#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <d3d12.h>
#include <vector>

/**
 * @class PostProcessRunner
 * @brief エフェクトのチェイン実行（Ping-Pong描画）に特化したランナークラス
 */
class PostProcessRunner {
public:
    PostProcessRunner() = default;
    ~PostProcessRunner() = default;

    /**
     * @brief Ping-Pong方式で指定されたエフェクト配列を順次実行する
     * @param manager PostProcessManagerのポインタ (パラメータやPSO取得用)
     * @param commandList コマンドリスト
     * @param modes 実行するエフェクトのリスト
     * @param srcTexture 最初の入力テクスチャ
     * @param rtvHandle 最終出力先のレンダーターゲットビュー (isFinalOutput=trueの場合に使用)
     * @param workspace Ping-Pong用の2枚のテクスチャなどのワークスペース
     * @param isFinalOutput このランナーの最後がバックバッファへの出力かどうか
     * @return 最後の出力結果が書き込まれた RenderTexture のポインタ（スワップチェーン出力なら nullptr）
     */
    class RenderTexture* Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                             const std::vector<PostProcessManager::Mode>& modes, class RenderTexture* srcTexture,
                             D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                             const PostProcessManager::PostProcessWorkspace& workspace, bool isFinalOutput = true,
                             bool isBackBufferTarget = false, const D3D12_VIEWPORT* targetViewport = nullptr,
                             const D3D12_RECT* targetScissor = nullptr);

private:
    /**
     * @brief RequiresSeparatePass を実行する。
     */
    bool RequiresSeparatePass(PostProcessManager::Mode mode) const;
};
