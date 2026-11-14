#pragma once
#include <cstdint>

namespace Irufemi {

/**
 * @struct ResourceHandle
 * @brief リソースを一意に識別するための軽量なハンドル。
 * 生ポインタや shared_ptr の代わりに保持することで、安全なリソース管理を実現する。
 */
struct ResourceHandle {
    uint32_t index = 0xFFFFFFFF;      // プール内のインデックス
    uint32_t generation = 0xFFFFFFFF; // 世代（使い回し時の誤参照防止用）

    /**
     * @brief IsValid かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsValid() const {
        return index != 0xFFFFFFFF;
    }

    bool operator==(const ResourceHandle& other) const {
        return index == other.index && generation == other.generation;
    }

    bool operator!=(const ResourceHandle& other) const {
        return !(*this == other);
    }
};

} // namespace Irufemi
