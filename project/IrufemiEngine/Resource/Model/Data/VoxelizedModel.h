#pragma once

#include "Core/Type/Voxel.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector3Int.h"
#include <vector>

namespace Irufemi {

/**
 * @struct VoxelizedModel
 * @brief ボクセル化されたモデル全体を管理する構造体
 */
struct VoxelizedModel {
    std::vector<Voxel> voxels;
    Vector3 aabbMin;
    Vector3 aabbMax;
    Vector3Int resolution;
};

} // namespace Irufemi