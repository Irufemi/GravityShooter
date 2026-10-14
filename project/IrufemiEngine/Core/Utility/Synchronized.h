#pragma once
#include <shared_mutex>
#include <mutex>
#include <utility>

namespace Irufemi {

/**
 * @class Synchronized
 * @brief データとその保護用共有ミューテックスを一体化し、スレッド安全なアクセスを強制するRAIIモニタ型ラッパー
 * @details 内部データへのアクセスには必ず ReadLock または WriteLock の取得を要するため、
 *          マルチスレッド環境におけるロックの取得忘れやデータ競合をコンパイルレベルで根絶します。
 * @tparam T スレッドセーフに保護する対象のデータ型
 * @tparam MutexType 使用するミューテックス型（デフォルト: std::shared_mutex）
 */
template <typename T, typename MutexType = std::shared_mutex>
class Synchronized {
public:
    /**
     * @class ReadGuard
     * @brief 読み取り専用アクセスを提供するRAII共有ロックガード（Proxy）
     */
    class ReadGuard {
    public:
        ReadGuard(const T& data, MutexType& mutex)
            : data_(data), lock_(mutex) {}

        const T* operator->() const noexcept { return &data_; }
        const T& operator*() const noexcept { return data_; }
        const T& Get() const noexcept { return data_; }

    private:
        const T& data_;
        std::shared_lock<MutexType> lock_;
    };

    /**
     * @class WriteGuard
     * @brief 読み書きアクセスを提供するRAII排他ロックガード（Proxy）
     */
    class WriteGuard {
    public:
        WriteGuard(T& data, MutexType& mutex)
            : data_(data), lock_(mutex) {}

        T* operator->() noexcept { return &data_; }
        T& operator*() noexcept { return data_; }
        T& Get() noexcept { return data_; }

    private:
        T& data_;
        std::unique_lock<MutexType> lock_;
    };

    Synchronized() = default;
    explicit Synchronized(const T& initialValue) : data_(initialValue) {}
    explicit Synchronized(T&& initialValue) : data_(std::move(initialValue)) {}

    template <typename... Args>
    explicit Synchronized(Args&&... args) : data_(std::forward<Args>(args)...) {}

    ~Synchronized() = default;

    Synchronized(const Synchronized&) = delete;
    Synchronized& operator=(const Synchronized&) = delete;
    Synchronized(Synchronized&&) = delete;
    Synchronized& operator=(Synchronized&&) = delete;

    /**
     * @brief 読み取り専用アクセス用ガードを取得（共有ロック）
     * @return 読み取り専用RAIIガード
     */
    [[nodiscard]] ReadGuard Read() const {
        return ReadGuard(data_, mutex_);
    }

    /**
     * @brief 読み書きアクセス用ガードを取得（排他ロック）
     * @return 読み書き用RAIIガード
     */
    [[nodiscard]] WriteGuard Write() {
        return WriteGuard(data_, mutex_);
    }

    /**
     * @brief 共有ロックを取得してラムダ式を実行（関数型スコープアクセス）
     * @tparam Func 実行する関数・ラムダ式
     * @param func 読み取り専用データ参照を受け取る関数
     * @return 関数の戻り値
     */
    template <typename Func>
    auto WithRead(Func&& func) const -> decltype(func(std::declval<const T&>())) {
        std::shared_lock<MutexType> lock(mutex_);
        return func(data_);
    }

    /**
     * @brief 排他ロックを取得してラムダ式を実行（関数型スコープアクセス）
     * @tparam Func 実行する関数・ラムダ式
     * @param func 変更可能データ参照を受け取る関数
     * @return 関数の戻り値
     */
    template <typename Func>
    auto WithWrite(Func&& func) -> decltype(func(std::declval<T&>())) {
        std::unique_lock<MutexType> lock(mutex_);
        return func(data_);
    }

private:
    T data_{};
    mutable MutexType mutex_;
};

} // namespace Irufemi
