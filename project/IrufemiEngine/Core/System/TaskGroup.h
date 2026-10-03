#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <condition_variable>

/**
 * @class TaskGroup
 * @brief 非同期タスクのグループ進捗（残数）を管理するクラス
 */
class TaskGroup {
public:
    TaskGroup() : pendingCount_(0) {}
    ~TaskGroup() = default;

    /**
     * @brief タスクの開始を通知（カウントアップ）
     */
    void NotifyTaskStarted() {
        pendingCount_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief タスクの完了を通知（カウントダウン）
     */
    void NotifyTaskFinished() {
        if (pendingCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(mutex_);
            cv_.notify_all();
        }
    }

    /**
     * @brief 全てのタスクが完了するまでブロック待機
     */
    void Wait() {
        if (IsAllDone()) {
            return;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return pendingCount_.load(std::memory_order_acquire) == 0; });
    }

    /**
     * @brief 全てのタスクが完了したか確認
     * @return true: 全完了, false: 未完了タスクあり
     */
    bool IsAllDone() const {
        return pendingCount_.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief 現在の待機中タスク数を取得
     */
    uint32_t GetPendingCount() const {
        return pendingCount_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> pendingCount_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
