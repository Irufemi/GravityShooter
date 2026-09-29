#pragma once

#include <string>
#include "Core/Math/Vector3.h"

/**
 * @struct PrefabMetrics
 * @brief プレハブアセットから抽出された基準寸法・当たり判定・モデル情報
 */
struct PrefabMetrics {
    Irufemi::Vector3 baseScale = {1.0f, 1.0f, 1.0f}; //!< 基準スケール
    float colliderRadius = 1.0f;                     //!< スフィアコライダーの半径
    bool hasSphereCollider = false;                  //!< スフィアコライダーが存在するか
    Irufemi::Vector3 obbSize = {1.0f, 1.0f, 1.0f};   //!< OBBコライダーの半幅サイズ (localSize)
    bool hasOBBCollider = false;                     //!< OBBコライダーが存在するか
    std::string modelPath = "";                      //!< 参照モデルのファイルパス
};

/**
 * @class PrefabUtility
 * @brief プレハブデータ（Archetype）から寸法や当たり判定等のメトリクス情報を安全かつ高速に抽出する汎用ユーティリティ
 */
class PrefabUtility {
public:
    /**
     * @brief 指定したプレハブファイルから基本スケールや当たり判定半径などのメトリクスを抽出する
     * @param prefabPath プレハブJSONのファイルパス
     * @return 抽出されたPrefabMetrics
     */
    static PrefabMetrics ExtractMetrics(const std::string& prefabPath);
};
