#pragma once

namespace Irufemi {

class RenderGraphBuilder;

/**
 * @class IComputeTask
 * @brief エンジン側で一括実行するためのCompute Shaderタスクのインターフェース
 * @details 毎フレームの描画パスの直前に、エンジンが一括でDispatchCompute()を呼び出すための予約インターフェースです。
 */
class IComputeTask {
public:
    virtual ~IComputeTask() = default;

    /**
     * @brief GPU上での計算処理（Compute Shaderのディスパッチ等）を実行する
     * @details DrawManagerによって、毎フレームの描画前に1回だけ呼び出される
     */
    virtual void DispatchCompute() = 0;

    /**
     * @brief 描画前（RenderGraph構築時）にリソースの依存状態を宣言する
     */
    virtual void Setup(RenderGraphBuilder& builder) {}
};

} // namespace Irufemi
