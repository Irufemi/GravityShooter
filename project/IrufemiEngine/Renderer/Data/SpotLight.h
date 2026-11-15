#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstdint>

namespace Irufemi {

struct SpotLight {
    //!< ライトの色
    Vector4 color;
    //!< ライトの位置
    Vector3 position;
    //! 輝度
    float intensity;
    //!< スポットライトの方向
    Vector3 direction;
    //!< ライトの届く最大距離
    float distance;
    //!< 減衰率
    float decay;
    //!< スポットライトの余弦
    float cosAngle;
    //!< フォールオフ
    float falloff;
    //!< 有効フラグ
    int32_t isActive;

private:
    float padding[4]; // 16byteアラインメント用 (合計80バイト)
};

static_assert(sizeof(SpotLight) == 80, "SpotLight size mismatch with HLSL");

} // namespace Irufemi