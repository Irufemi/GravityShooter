#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstdint>

namespace Irufemi {

struct PointLight {
    //!< ライトの色
    Vector4 color;
    //!< ライトの位置
    Vector3 position;
    //!< 輝度
    float intensity;
    //!< ライトの影響範囲
    float radius;
    //!< 減衰率
    float decay;
    //!< 有効フラグ
    int32_t isActive;

private:
    float padding;
};

static_assert(sizeof(PointLight) == 48, "PointLight size mismatch with HLSL");

} // namespace Irufemi