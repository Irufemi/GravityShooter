#pragma once

#include <memory>
#include <string>
#include "Core/Type/PrimitiveType.h"
#include "Core/Math/Transform.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Core/Math/Math.h"
#include "Core/System/ResourceHandle.h"

class Camera;
class PrimitiveManager;
class TextureManager;
struct PrimitiveData;

/**
 * @struct PrimitiveTransform
 * @brief 座標変換（位置・回転・スケール）を管理するコンポーネント
 */
struct PrimitiveTransform {
    Irufemi::Transform transform; //!< トランスフォーム情報
    bool isDirty = true;          //!< 行列再計算が必要な場合のフラグ

    /**
     * @brief トランスフォーム情報をリソースへ反映し、行列を更新する
     * @param[out] resource 反映先のリソース
     * @param[in] camera 描画に使用するカメラ
     */
    void UpdateTransform(Object3DResource* resource, const Camera& camera);
};

/**
 * @struct MeshDesc
 * @brief メッシュ形状（頂点・インデックス情報）を管理するデータ
 */
struct MeshDesc {
    Irufemi::PrimitiveType type;                //!< 現在のプリミティブ形状タイプ
    std::unique_ptr<Object3DResource> resource; //!< D3D12リソース

    /**
     * @brief 指定した形状タイプにメッシュを切り替える
     * @param[in] newType 新しい形状タイプ
     */
    void ChangeMesh(Irufemi::PrimitiveType newType);

    /**
     * @brief カスタムの PrimitiveData を用いて独自にメッシュリソースを再生成する
     * @param[in] data 再生成に使用する頂点・インデックスデータ
     */
    void ChangeMesh(const PrimitiveData& data);

    /**
     * @brief PrimitiveManager を設定する。
     * @param[in] pm 設定する PrimitiveManager の値
     */
    static void SetPrimitiveManager(PrimitiveManager* pm) {
        primitiveManager_ = pm;
    }

private:
    inline static PrimitiveManager* primitiveManager_ = nullptr;
};

/**
 * @struct MaterialDesc
 * @brief 色やテクスチャ、ライティング設定を管理するデータ
 */
struct MaterialDesc {
    std::string texturePath;                           //!< テクスチャパス
    std::string loadedTexturePath;                     //!< 前回ロードしたパス（変更検知用）
    ResourceHandle textureHandle;                      //!< テクスチャキャッシュ用ハンドル
    int selectedTextureIndex = 0;                      //!< ImGui選択用インデックス
    Irufemi::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; //!< ベースカラー
    bool enableLighting = true;                        //!< ライティングの有無
    int lightingMode = 3;   //!< ライティングモード (0:None, 1:Lambert, 2:Half-Lambert, 3:PBR)
    float metallic = 0.0f;  //!< 金属度
    float roughness = 0.5f; //!< 粗さ

    // clang-format off
    Irufemi::Matrix4x4 uvTransform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }; //!< UV変換行列（スクロール・反転等用）
    // clang-format on
    float alphaReference = 0.0f; //!< ディスカード閾値
    int32_t useClampSampler = 0; //!< サンプラー切替 (0:WRAP, 1:CLAMP)

    /**
     * @brief マテリアル設定をリソースへ反映する
     * @param[out] resource 反映先のリソース
     * @param[in] textureManager テクスチャハンドル取得用
     */
    void UpdateMaterial(Object3DResource* resource, TextureManager* textureManager);

    /**
     * @brief 保持しているハンドルを解放する（破棄時に呼ぶ）
     */
    void Release(TextureManager* textureManager);
};
