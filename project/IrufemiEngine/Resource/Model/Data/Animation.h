#pragma once

#include <unordered_map>
#include <string>
#include <atomic>
#include <cstdint>
#include "Resource/Model/Data/NodeAnimation.h"

namespace Irufemi {

/**
 * @brief アニメーション一意IDの生成
 */
inline uint64_t GenerateAnimationId() {
    static std::atomic<uint64_t> sNextId{1};
    return sNextId.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @struct Animation
 * @brief アニメーション全体を管理するデータ構造
 */
struct Animation {
    uint64_t id = GenerateAnimationId(); ///< アニメーションインスタンス固有の一意ID（ABA問題対策）
    float duration = 0.0f;               ///< アニメーション全体の尺(単位は秒)
    /// NodeAnimationの集合。Node名でひけるようにしておく
    std::unordered_map<std::string, NodeAnimation> nodeAnimations;
};

} // namespace Irufemi