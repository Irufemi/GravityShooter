#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Resource/Model/Data/VertexWeightData.h"
#include <vector>

namespace Irufemi {
struct JointWeightData {
    Irufemi::Matrix4x4 inverseBindPoseMatrix;
    std::vector<VertexWeightData> vertexWeights;
};

} // namespace Irufemi