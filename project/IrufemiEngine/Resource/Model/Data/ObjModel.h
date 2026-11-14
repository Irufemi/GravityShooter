#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "Renderer/Data/VertexData.h"
#include "Resource/Model/Data/Node.h"
#include "Core/Math/Math.h"
#include "Resource/Model/Data/JointWeightData.h"
#include "Core/Shape/Sphere.h"
#include "Core/Math/Geometry/AABB.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Irufemi {

struct ObjMaterial {
    // Kd
    Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    // Ka
    Irufemi::Vector3 ambient = {0.1f, 0.1f, 0.1f};
    // Ks
    Irufemi::Vector3 specular = {1.0f, 1.0f, 1.0f};

    float roughness = 0.5f;
    float metallic = 0.0f;
    float alpha = 1.0f;

    bool enableLighting = true;

    // ライティングモード (0:None, 1:Lambert, 2:Half-Lambert, 3:PBR)
    int32_t lightingMode = 3;

    // サンプラー設定 (0:WRAP, 1:CLAMP)
    int32_t useClampSampler = 0;

    // 環境マップの映り込み係数
    float environmentCoefficient = 0.0f;

    // アルファテスト用閾値 (0.0f = すべて通す, 1.0f = すべて棄却)
    float alphaReference = 0.5f;

    Irufemi::Matrix4x4 uvTransform = Irufemi::Math::MakeIdentity4x4();

    std::string textureFilePath = "";
    std::string normalMapFilePath = "";

    // エフェクトから保護するかどうか
    bool enableEffectMask = false;

    // カスタムエフェクトのタイプとパラメータ
    int32_t customEffectType = 0;
    float customEffectParam = 0.0f;
};

struct ObjMesh {

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices; //!< インデックスバッファデータ
    ObjMaterial material;
    std::string nodeName; //!< このメッシュが属するノード名
};

// 階層(Node)を統合した拡張版 ObjModel
/**
 * @class ObjModel
 * @brief Wavefront OBJ 形式などの3Dモデルデータを保持するクラス
 * @details 頂点データやマテリアル情報を管理し、描画パイプラインへモデルデータを供給します。
 */
struct ObjModel {
    /** @brief モデルを構成するメッシュ（頂点・インデックス・マテリアルのセット）のリスト */
    std::vector<ObjMesh> meshes;

    /** @brief シーン階層のルートノード */
    Node rootNode;

    /** @brief スキンクラスター（ボーンウェイト）データのマップ */
    std::unordered_map<std::string, JointWeightData> skinClusterData;

    /** @brief モデル全体の境界球（高速なカリング用） */
    Irufemi::Sphere boundingSphere;

    /** @brief モデル全体のローカルAABB（高精度ピッキング用） */
    Irufemi::AABB boundingBox;
};

} // namespace Irufemi