#pragma once
#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <functional>
#include <cstddef>

/**
 * @class ComponentPool
 * @brief コンポーネントを連続したメモリ領域に配置する（DOD）ためのチャンクベース・メモリプール
 * @tparam T 対象となるコンポーネントの型
 * @tparam ChunkSize 1つのチャンク（ブロック）に格納する要素数。デフォルトは1024。
 */
template <typename T, size_t ChunkSize = 1024> class ComponentPool {
public:
    /**
     * @brief Instance を取得する。
     * @return 取得された Instance
     */
    static ComponentPool& GetInstance() {
        static ComponentPool instance;
        return instance;
    }

    /**
     * @brief プールからメモリを確保し、コンポーネントを生成する
     * @return 確保されたコンポーネントの shared_ptr（カスタムデリータ付き）
     */
    template <typename... Args>
    std::shared_ptr<T> Create(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        // フリーリストから O(1) で空きスロットを取得
        if (!freeList_.empty()) {
            Slot slot = freeList_.back();
            freeList_.pop_back();

            auto& chunk = chunks_[slot.chunkIdx];
            chunk->active[slot.itemIdx] = true;
            T* ptr = new (&chunk->data[slot.itemIdx]) T(std::forward<Args>(args)...);

            return std::shared_ptr<T>(
                ptr, [slot](T* p) { ComponentPool::GetInstance().Free(slot.chunkIdx, slot.itemIdx, p); });
        }

        // 全チャンクが埋まっていれば新しいチャンクを追加
        auto newChunk = std::make_unique<Chunk>();
        size_t newChunkIdx = chunks_.size();
        chunks_.push_back(std::move(newChunk));

        // 残りのスロットをフリーリストに追加（逆順で追加しインデックスの若い順に取り出せるようにする）
        for (size_t i = ChunkSize - 1; i >= 1; --i) {
            freeList_.push_back({newChunkIdx, i});
        }

        chunks_[newChunkIdx]->active[0] = true;
        T* ptr = new (&chunks_[newChunkIdx]->data[0]) T(std::forward<Args>(args)...);

        return std::shared_ptr<T>(ptr, [newChunkIdx](T* p) { ComponentPool::GetInstance().Free(newChunkIdx, 0, p); });
    }

    /**
     * @brief 確保されている全てのコンポーネントに対して一括で処理を行う（DODのコア）
     * @details メモリが連続しているため、CPUキャッシュヒット率が劇的に向上します
     */
    template <typename Func>
    void ForEach(Func f) {
        // パフォーマンス最優先のためロックを取らずに実行します（Update専用）
        for (auto& chunk : chunks_) {
            for (size_t i = 0; i < ChunkSize; ++i) {
                if (chunk->active[i]) {
                    f(*reinterpret_cast<T*>(&chunk->data[i]));
                }
            }
        }
    }

private:
    ComponentPool() = default;
    ~ComponentPool() {
        // プール破棄時の処理（念のため強制的にデストラクタを呼ぶ）
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& chunk : chunks_) {
            for (size_t i = 0; i < ChunkSize; ++i) {
                if (chunk->active[i]) {
                    reinterpret_cast<T*>(&chunk->data[i])->~T();
                    chunk->active[i] = false;
                }
            }
        }
        freeList_.clear();
    }

    ComponentPool(const ComponentPool&) = delete;
    ComponentPool& operator=(const ComponentPool&) = delete;

    /**
     * @brief shared_ptrの参照カウントが0になった時に呼ばれ、メモリを解放状態にする
     */
    void Free(size_t chunkIdx, size_t itemIdx, T* p) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!chunks_[chunkIdx]->active[itemIdx]) {
            return; // 既に破棄済みの場合は多重デストラクトを防ぐ
        }
        p->~T(); // デストラクタを明示的に呼び出す
        chunks_[chunkIdx]->active[itemIdx] = false;
        freeList_.push_back({chunkIdx, itemIdx});
    }

    struct Chunk {
        // std::vectorだと再配置でポインタが無効化されるため、
        // Placement new 用の固定サイズ・未初期化バイト配列（アライメント指定）を使用
        alignas(T) std::byte data[ChunkSize][sizeof(T)];
        bool active[ChunkSize] = {false};
    };

    struct Slot {
        size_t chunkIdx;
        size_t itemIdx;
    };

    std::vector<std::unique_ptr<Chunk>> chunks_;
    std::vector<Slot> freeList_;
    std::mutex mutex_;
};

/**
 * @brief 指定したコンポーネントがComponentPoolを使用するかどうかを判定するトレイト
 * @details プール対応させたいコンポーネントのヘッダで、この構造体を特殊化（std::true_type）してください。
 */
template <typename T> struct IsPooledComponent : std::false_type {};

class TransformComponent;
template <> struct IsPooledComponent<TransformComponent> : std::true_type {};
