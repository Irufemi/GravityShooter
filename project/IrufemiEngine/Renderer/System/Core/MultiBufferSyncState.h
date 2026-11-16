#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include "RHI/DirectX12/DirectXCommon.h" // kMaxFramesInFlight

namespace Irufemi {

/**
 * @class MultiBufferSyncState
 * @brief 定数バッファ等のマルチバッファリングにおける同期フラグ状態を管理するMixinクラス
 * @details 各フレームごとの「バッファ更新が必要か」という状態（ダーティフラグ）を管理します。
 * 主にBaseResourceやIRenderable実装クラスに継承させて利用します。
 */
class MultiBufferSyncState {
public:
    MultiBufferSyncState() {
        // 初期状態ではすべてのフレームのバッファ更新が必要
        MarkAsDirty();
    }

    virtual ~MultiBufferSyncState() = default;

    /**
     * @brief 全フレームバッファをダーティ状態（更新が必要）にする
     * @details オブジェクトの座標や色、マテリアルパラメータなどが変更された際に呼び出します。
     */
    virtual void MarkAsDirty() {
        isDirtyBuffer_.fill(true);
    }

    /**
     * @brief 指定したフレームバッファがダーティか確認し、ダーティであればフラグを下ろす
     * @param frameIndex 確認するフレームインデックス（通常はDirectXCommonから取得）
     * @return ダーティだった場合は true（範囲外の場合は false）
     */
    bool CheckAndClearDirty(uint32_t frameIndex) {
        assert(frameIndex < Irufemi::kMaxFramesInFlight && "frameIndex exceeds kMaxFramesInFlight!");
        if (frameIndex >= Irufemi::kMaxFramesInFlight) {
            return false;
        }

        if (isDirtyBuffer_[frameIndex]) {
            isDirtyBuffer_[frameIndex] = false;
            return true;
        }
        return false;
    }

protected:
    std::array<bool, Irufemi::kMaxFramesInFlight> isDirtyBuffer_{};
};

} // namespace Irufemi
