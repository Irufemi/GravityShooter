#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include <functional>
#include <memory>

/**
 * @class ObjectPool
 * @brief ゼロ・アロケーションを実現する Handle ベースのオブジェクトプール
 *
 * キャッシュラインに最適化された連続メモリ配置と、Generation(世代)管理による安全な不正アクセス防止機能を持ちます。
 * @tparam T プールで管理するオブジェクトの型
 */
template <typename T> class ObjectPool {
public:
    /// @brief プール内のオブジェクトを安全に指し示すためのハンドル
    struct Handle {
        uint32_t index = 0xFFFFFFFF; ///< スロットのインデックス
        uint32_t generation = 0;     ///< 世代（古いハンドルでのアクセスを防ぐ）

#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
        T* debugPtr = nullptr; ///< エディタ上での視認性・デバッグ用のポインタ
#endif

        /**
         * @brief IsValid かどうかを判定する。
         * @return 判定結果 (true/false)
         */
        bool IsValid() const {
            return index != 0xFFFFFFFF;
        }
        bool operator==(const Handle& other) const {
            return index == other.index && generation == other.generation;
        }
        bool operator!=(const Handle& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief プールの初期化
     * @param capacity 最大保持可能数
     * @param factory 初期化時に各要素を構築するためのファクトリ関数（GameObjectのPrefab生成用）
     */
    explicit ObjectPool(size_t capacity, std::function<std::shared_ptr<T>()> factory = nullptr) {
        slots_.resize(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            slots_[i].nextFree = static_cast<uint32_t>(i + 1);
            slots_[i].generation = 1;
            slots_[i].active = false;

            if (factory) {
                // ファクトリが指定されている場合はPrefabなどを事前生成して格納
                slots_[i].data = factory();
            } else {
                slots_[i].data = std::make_shared<T>();
            }
        }
        // 最後の要素の nextFree は無効値
        if (capacity > 0) {
            slots_[capacity - 1].nextFree = 0xFFFFFFFF;
        }
        headFree_ = 0;
        freeCount_ = capacity;
    }

    ~ObjectPool() = default;

    /**
     * @brief オブジェクトを O(1) で取得する
     * @return 取得したオブジェクトの Handle。空きがない場合は無効な Handle を返す。
     */
    Handle Acquire() {
        if (headFree_ == 0xFFFFFFFF) {
            /**
             * @brief Handle を実行する。
             */
            return Handle(); // 空きなし
        }

        uint32_t index = headFree_;
        headFree_ = slots_[index].nextFree;

        slots_[index].active = true;
        --freeCount_;

        Handle h;
        h.index = index;
        h.generation = slots_[index].generation;

#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
        h.debugPtr = slots_[index].data.get();
#endif
        return h;
    }

    /**
     * @brief オブジェクトを O(1) で返却し、世代を進行させる
     * @param handle 返却するオブジェクトのハンドル
     */
    void Release(Handle handle) {
        if (!IsValidHandle(handle)) {
            return;
        }

        uint32_t index = handle.index;
        slots_[index].active = false;
        slots_[index].generation++; // 次の利用者のために世代を進める

        // フリーリストの先頭に追加
        slots_[index].nextFree = headFree_;
        headFree_ = index;
        ++freeCount_;
    }

    /**
     * @brief ハンドルから実体を取得する
     * @param handle アクセスするハンドル
     * @return 正常なハンドルであれば実体、不正なら初期値またはnullptr
     */
    const std::shared_ptr<T>& Resolve(Handle handle) const {
        static const std::shared_ptr<T> nullPtr = nullptr;
        if (!IsValidHandle(handle)) {
            return nullPtr;
        }
        return slots_[handle.index].data;
    }

    /**
     * @brief FreeCount を取得する。
     * @return 取得された FreeCount
     */
    size_t GetFreeCount() const {
        return freeCount_;
    }
    /**
     * @brief Capacity を取得する。
     * @return 取得された Capacity
     */
    size_t GetCapacity() const {
        return slots_.size();
    }

    /**
     * @brief プール内の全要素に対して処理を実行する（クリーンアップ・破棄時処理用）
     * @param func 各要素に対して呼び出される関数オブジェクト
     */
    void ForEach(const std::function<void(const std::shared_ptr<T>&)>& func) {
        if (!func) {
            return;
        }
        for (auto& slot : slots_) {
            if (slot.data) {
                func(slot.data);
            }
        }
    }

private:
    /**
     * @brief IsValidHandle かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsValidHandle(Handle handle) const {
        if (handle.index >= slots_.size()) {
            return false;
        }
        if (!slots_[handle.index].active) {
            return false;
        }
        if (slots_[handle.index].generation != handle.generation) {
            return false; // 世代アンマッチ（古いハンドル）
        }
        return true;
    }

    struct Slot {
        std::shared_ptr<T>
            data; ///< オブジェクトの実体（既存のFactoryと互換性を持たせるためshared_ptrを採用。アロケーションは起動時のみ）
        uint32_t nextFree;   ///< 次の空きスロットインデックス（Intrusive Free List用）
        uint32_t generation; ///< 現在の世代
        bool active;         ///< 使用中フラグ
    };

    std::vector<Slot> slots_;
    uint32_t headFree_ = 0xFFFFFFFF; ///< フリーリストの先頭インデックス
    size_t freeCount_ = 0;           ///< 空きスロット数
};
