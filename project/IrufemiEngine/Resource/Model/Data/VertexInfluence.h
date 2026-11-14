#pragma once

#include <cstdint>
#include <array>

namespace Irufemi {

inline constexpr uint32_t kNumMaxInfluence = 4;

/**
 * @struct VertexInfluence
 * @brief 頂点ごとのボーン影響度とインデックス
 */
struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights = {};
    std::array<int32_t, kNumMaxInfluence> jointIndices = {};
};

} // namespace Irufemi