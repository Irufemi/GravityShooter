#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/QuaternionTransform.h"

namespace Irufemi {

/**
 * @struct JointData
 * @brief 静的なボーン（Joint）定義。全インスタンスで共有される。
 */
struct JointData {
    std::string name;
    int32_t index = 0;
    std::optional<int32_t> parent;
    std::vector<int32_t> children;

    // Bind Pose (初期状態のTransformとローカル行列)
    QuaternionTransform bindTransform;
    Matrix4x4 bindLocalMatrix;
};

/**
 * @struct SkeletonData
 * @brief ボーン階層構造の静的データ。モデルのロード時に1度だけ作成される。
 */
struct SkeletonData {
    int32_t root = -1;
    std::unordered_map<std::string, int32_t> jointMap;
    std::vector<JointData> joints;
};

} // namespace Irufemi
