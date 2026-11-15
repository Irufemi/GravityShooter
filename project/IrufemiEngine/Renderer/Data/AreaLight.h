#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstdint>

namespace Irufemi {

struct AreaLight {
    //!< ライトの色
    Vector4 color;
    //!< ライトの位置
    Vector3 position;
    //!< 輝度
    float intensity;
    //!< ライトの向き
    Vector3 direction;
    //!< ライトの届く最大距離
    float range;
    //!< 矩形のサイズ(幅、高さ)
    Vector2 size;
    //!< 有効フラグ
    int32_t isActive;

private:
    float padding;
};

static_assert(sizeof(AreaLight) % 16 == 0, "AreaLight must be 16-byte aligned for HLSL constant buffer");

} // namespace Irufemi