#pragma once
#include <cstdint>
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @struct BombCoreParams
 * @brief 爆弾のコアエフェクト用定数バッファパラメータ構造体
 * @note 16バイト境界アライメント（合計64バイト）
 */
struct BombCoreParams {
    Vector4 edgeColor{1.0f, 0.2f, 0.0f, 1.0f};  //!< フチ（外側）の色
    Vector4 coreColor{1.0f, 1.0f, 0.5f, 1.0f};  //!< 中心（内側）の色
    Vector4 crackColor{1.0f, 0.5f, 0.0f, 1.0f}; //!< 亀裂から漏れ出る光の色
    float noiseScale = 1.0f;                    //!< ノイズのスケール
    float distortion = 0.5f;                    //!< 亀裂の歪み具合
    float pulseSpeed = 1.0f;                    //!< 明滅の速度
    float intensity = 1.0f;                     //!< 全体の発光強度
};

static_assert(sizeof(BombCoreParams) == 64, "BombCoreParams size must be 64 bytes (16-byte aligned)");

} // namespace Irufemi
