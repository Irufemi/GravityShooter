#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <d3d12.h>

class RenderTexture;

namespace Irufemi {

/**
 * @struct PostProcessRenderContext
 * @brief 各ポストプロセスエフェクトの描画に必要なコンテキスト情報
 */
struct PostProcessRenderContext {
    PostProcessManager* manager = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    RenderTexture* sourceTexture = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
    const PostProcessManager::PostProcessWorkspace* workspace = nullptr;
    bool writeToScreen = false;
    bool isBackBufferTarget = false;
    const D3D12_VIEWPORT* targetViewport = nullptr;
    const D3D12_RECT* targetScissor = nullptr;
};

/**
 * @class IPostProcessEffect
 * @brief 各ポストプロセスエフェクトの描画戦略インターフェース
 */
class IPostProcessEffect {
public:
    virtual ~IPostProcessEffect() = default;

    /**
     * @brief このエフェクトが独立した描画パスを要求するかどうか
     */
    virtual bool RequiresSeparatePass() const { return false; }

    /**
     * @brief エフェクトの描画コマンドを発行する
     * @param context 描画コンテキスト
     */
    virtual void Render(const PostProcessRenderContext& context) = 0;
};

} // namespace Irufemi
