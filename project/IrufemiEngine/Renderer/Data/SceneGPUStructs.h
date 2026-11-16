#pragma once

#include "Renderer/Data/CameraForGPU.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/AreaLight.h"
#include <cstdint>

namespace Irufemi {

/**
 * @struct PerFrameData
 * @brief フレーム全体で共有されるデータ構造体 (Camera, Timeなど)
 */
struct PerFrameData {
    CameraForGPU camera; //!< カメラ情報 (view, projection, worldPosition)
    float time;          //!< フレーム経過時間 (秒)
    float deltaTime;     //!< フレーム差分時間 (秒)
    Vector2 resolution;  //!< 画面解像度 (x: width, y: height)

    // [Bindless] 各種グローバルリソースのインデックス
    uint32_t envMapIndex;    //!< 環境マップ (TextureCube, space2)
    uint32_t shadowMapIndex; //!< シャドウマップ (Texture2D, space3)
    uint32_t depthMapIndex;  //!< デプスマップ (Texture2D, space3)
    uint32_t padding[1];     //!< 16バイトアライメント用パディング
};

static_assert(sizeof(PerFrameData) == 176, "PerFrameData size mismatch with HLSL");

} // namespace Irufemi
