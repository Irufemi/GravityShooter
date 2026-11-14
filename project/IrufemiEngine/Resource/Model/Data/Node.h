#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/QuaternionTransform.h"
#include <string>
#include <vector>

namespace Irufemi {

/**
 * @class Node
 * @brief 3Dモデルの階層構造を表現するノードデータ
 * @details ローカル変換行列と親子関係を持ち、シーングラフやスケルトン構造の構築に使用されます。
 */
struct Node {
    /** @brief ローカル空間での位置・回転・スケール */
    QuaternionTransform transform;

    /** @brief 計算済みのローカル変換行列 */
    Matrix4x4 localMatrix;

    /** @brief ノードの名前 */
    std::string name;

    /** @brief 子ノードのリスト */
    std::vector<Node> children;
};

} // namespace Irufemi