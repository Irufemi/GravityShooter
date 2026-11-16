#pragma once

namespace Irufemi {

/**
 * @class IRenderable
 * @brief 描画可能オブジェクトのインターフェース
 * @details 描画前に状態を同期する処理と実際の描画処理の規約を定義します。
 */
class IRenderable {
public:
    virtual ~IRenderable() = default;

    /**
     * @brief 描画直前の同期処理
     * @details
     * リソースクラスなどに保持されているダーティフラグを評価し、変更があれば定数バッファ等のGPU転送を行います。
     */
    virtual void SyncBeforeDraw() = 0;

    /**
     * @brief 描画処理
     * @details 実際の描画コマンドの積み込み（DrawIndexedInstanced等）を行います。
     */
    virtual void Draw() = 0;

    /**
     * @brief 選択中の輪郭マスク描画処理
     */
    virtual void DrawOutlineMask() {}
};

} // namespace Irufemi
