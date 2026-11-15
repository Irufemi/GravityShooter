#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

namespace Irufemi {

struct CameraForGPU {
    Matrix4x4 view = {};
    Matrix4x4 projection = {};
    Vector3 worldPosition = {};
    float padding = 0.0f; //!< 16バイトアライメント用パディング
};

static_assert(sizeof(CameraForGPU) == 144, "CameraForGPU must be 144 bytes for 16-byte alignment");

} // namespace Irufemi
