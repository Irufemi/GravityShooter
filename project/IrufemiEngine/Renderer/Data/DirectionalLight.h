#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

/*LambertianReflectance*/

namespace Irufemi {

struct DirectionalLight {
    //!< ライトの色
    Vector4 color;
    //!< ライトの向き
    Vector3 direction;
    //!< 輝度
    float intensity;
};

static_assert(sizeof(DirectionalLight) == 32, "DirectionalLight must be 32 bytes for 16-byte alignment");

} // namespace Irufemi