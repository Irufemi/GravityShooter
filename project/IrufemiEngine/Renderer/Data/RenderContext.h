#pragma once
#include <d3d12.h>
#include <cstdint>

class IrufemiEngine;
class DirectXCommon;
class DrawManager;
class TextureManager;
class FontManager;
class PSOManager;

namespace Irufemi {

/**
 * @struct RenderContext
 * @brief 描画フレームおよび各レンダーパスの実行に必要なコンテキスト情報を集約する構造体
 */
struct RenderContext {
    IrufemiEngine* engine = nullptr;
    DirectXCommon* dxCommon = nullptr;
    DrawManager* drawManager = nullptr;
    TextureManager* textureManager = nullptr;
    FontManager* fontManager = nullptr;
    PSOManager* psoManager = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    uint32_t frameIndex = 0;

    // 出力先レンダーターゲットおよび描画領域の一元管理
    ID3D12Resource* outputResource = nullptr; //!< 出力先リソース（Editor時はMainRenderTexture、Standalone時はnullptr）
    D3D12_CPU_DESCRIPTOR_HANDLE outputRTV{}; //!< 出力先RTVハンドル（Editor時はMainRenderTexture、Standalone時はBackBuffer）
    D3D12_VIEWPORT viewport{};               //!< 出力先ビューポート（レターボックス計算済み）
    D3D12_RECT scissorRect{};                //!< 出力先シザー矩形（レターボックス計算済み）
    bool isEditorMode = false;               //!< エディタモード実行中かどうか
};

} // namespace Irufemi
