#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @struct VertexData
 * @brief 3Dモデルなどの頂点データを表す構造体
 */
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< [追加] 頂点カラー
};

} // namespace Irufemi
