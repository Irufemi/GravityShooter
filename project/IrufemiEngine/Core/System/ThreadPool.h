#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include "Core/System/TaskGroup.h"

/**
 * @class ThreadPool
 * @brief 複数のワーカースレッドを用いてタスクを並列実行するクラス
 */
class ThreadPool {
public:
    /**
     * @brief スレッドプールの初期化
     * @param numThreads 生成するスレッド数
     */
    explicit ThreadPool(size_t numThreads);

    /**
     * @brief デストラクタ（全スレッドの終了を待機）
     */
    ~ThreadPool();

    /**
     * @brief タスクをキューに追加し、戻り値を追跡するための future を返す
     * @tparam F 関数型
     * @tparam Args 引数型
     * @param f 実行する関数
     * @param args 関数の引数
     * @return 実行結果を取得するための std::future
     */
    template <class F, class... Args>
    auto Enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

    /**
     * @brief タスクを特定の TaskGroup に紐付けてキューに追加し、future を返す
     */
    template <class F, class... Args>
    auto Enqueue(std::shared_ptr<TaskGroup> group, F&& f,
                 Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

    /**
     * @brief 現在処理中のスレッド数を取得
     */
    size_t GetActiveThreadCount() const {
        return activeThreads_.load();
    }

    /**
     * @brief キューで待機中のタスク数を取得
     */
    size_t GetQueuedTaskCount() const {
        /**
         * @brief lock を実行する。
         */
        std::lock_guard<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }

    /**
     * @brief 前回の取得から完了したタスクの数を取得し、内部カウンターをリセットする
     */
    size_t PopCompletedTaskCount() {
        return completedTasks_.exchange(0);
    }

    /**
     * @brief スレッドプールの全体のスレッド数を取得
     */
    size_t GetTotalThreadCount() const {
        return workers_.size();
    }

private:
    // ワーカースレッドの配列
    std::vector<std::thread> workers_;
    // タスクキュー
    std::queue<std::function<void()>> tasks_;

    // 同期用
    // NOTE: std::condition_variable::wait に渡す std::unique_lock<std::mutex> が
    // 言語仕様上必須のため、Synchronized<T> ではなく生の std::mutex を意図して維持しています。
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;

    // 監視用メトリクス
    std::atomic<size_t> activeThreads_{0};
    std::atomic<size_t> completedTasks_{0};
};

inline ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    /**
                     * @brief lock を実行する。
                     */
                    std::unique_lock<std::mutex> lock(this->queueMutex_);
                    this->condition_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }

                ++this->activeThreads_;
                task();
                --this->activeThreads_;
                ++this->completedTasks_;
            }
        });
    }
}

template <class F, class... Args>
auto ThreadPool::Enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        /**
         * @brief lock を実行する。
         */
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }
        tasks_.emplace([task]() { (*task)(); });
    }
    condition_.notify_one();
    return res;
}

template <class F, class... Args>
auto ThreadPool::Enqueue(std::shared_ptr<TaskGroup> group, F&& f,
                         Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
    if (group) {
        group->NotifyTaskStarted();
    }

    using return_type = typename std::invoke_result_t<F, Args...>;
    auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        /**
         * @brief lock を実行する。
         */
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            if (group) {
                group->NotifyTaskFinished();
            }
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        tasks_.emplace([task, group]() {
            (*task)();
            if (group) {
                group->NotifyTaskFinished();
            }
        });
    }
    condition_.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool() {
    {
        /**
         * @brief lock を実行する。
         */
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
        // アプリケーション終了時など、未実行のタスクを破棄して速やかにスレッドを終了させる
        std::queue<std::function<void()>> emptyQueue;
        std::swap(tasks_, emptyQueue);
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
