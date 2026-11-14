#pragma once

#include <cstdint>

namespace Irufemi {

/**
 * @struct VertexWeightData
 * @brief 単一頂点に対するボーン影響度データ
 */
struct VertexWeightData {
    float weight = 0.0f;
    uint32_t vertexIndex = 0;
};

} // namespace Irufemi