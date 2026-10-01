#pragma once
#include <cstdint>

namespace Irufemi {

/**
 * @enum RenderStage
 * @brief パイプライン内のカスタム描画注入ポイント（RenderPass Injection Point）
 * @details 業界標準の RenderGraph / FrameGraph 設計に基づき、
 *          特定のパス直前・直後にカスタム描画コマンドを安全かつ明示的に挿入します。
 */
enum class RenderStage : uint8_t {
    BeforeOpaque = 0,       ///< 3D不透明パス直前・Skybox直後（最奥背景・星雲など）
    BeforeTransparent,      ///< 3D不透明パス完了後・半透明パーティクル直前（深度テスト利用カスタムなど）
    BeforePostProcess,      ///< 半透明完了後・ポストプロセス直前（ブルーム対象のカスタムパスなど）
    AfterUI,                ///< 全UI描画完了後（従来のPostRender・最前面オーバーレイなど）
    Count                   ///< ステージ総数
};

} // namespace Irufemi
