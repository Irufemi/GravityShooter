#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include <utility>

namespace Irufemi {

/**
 * @class ScopedEffectHandle
 * @brief ポストプロセスエフェクトのRAII（自動寿命管理）ハンドル
 * @details このハンドルが破棄されるかスコープを抜けると、
 *          デストラクタ経由で自動的にPostProcessManagerからエフェクトが解除されます。
 *          開発者が手動でRemoveActiveModeを呼ぶ必要をなくし、演出残留バグを根絶します。
 */
class ScopedEffectHandle {
public:
    ScopedEffectHandle() = default;

    ScopedEffectHandle(PostProcessManager* manager, PostProcessMode mode, PostProcessManager::Layer layer)
        : manager_(manager), mode_(mode), layer_(layer), active_(true) {}

    ~ScopedEffectHandle() {
        Release();
    }

    // コピー禁止 (所有権の唯一性を保証)
    ScopedEffectHandle(const ScopedEffectHandle&) = delete;
    ScopedEffectHandle& operator=(const ScopedEffectHandle&) = delete;

    // ムーブセマンティクス対応
    ScopedEffectHandle(ScopedEffectHandle&& other) noexcept
        : manager_(other.manager_), mode_(other.mode_), layer_(other.layer_), active_(other.active_) {
        other.manager_ = nullptr;
        other.mode_ = PostProcessMode::None;
        other.active_ = false;
    }

    ScopedEffectHandle& operator=(ScopedEffectHandle&& other) noexcept {
        if (this != &other) {
            Release();
            manager_ = other.manager_;
            mode_ = other.mode_;
            layer_ = other.layer_;
            active_ = other.active_;
            other.manager_ = nullptr;
            other.mode_ = PostProcessMode::None;
            other.active_ = false;
        }
        return *this;
    }

    /**
     * @brief 明示的にエフェクトを解除する
     */
    void Release() {
        if (active_ && manager_) {
            manager_->RemoveActiveMode(mode_, layer_);
            active_ = false;
            manager_ = nullptr;
        }
    }

    /**
     * @brief エフェクトが現在有効かどうか
     */
    bool IsActive() const {
        return active_;
    }

    PostProcessMode GetMode() const {
        return mode_;
    }

    PostProcessManager::Layer GetLayer() const {
        return layer_;
    }

private:
    PostProcessManager* manager_ = nullptr;
    PostProcessMode mode_ = PostProcessMode::None;
    PostProcessManager::Layer layer_ = PostProcessManager::Layer::All;
    bool active_ = false;
};

} // namespace Irufemi
