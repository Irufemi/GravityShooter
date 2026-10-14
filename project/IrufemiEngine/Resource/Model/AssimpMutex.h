#pragma once
#include <mutex>

namespace Irufemi {
class AssimpMutex {
public:
    /**
     * @brief Assimp の非スレッドセーフなパース処理を直列化するための排他制御ミューテックスを取得する
     * @details NOTE: 外部 C-API (Assimp) のインポート処理をグローバルに直列化する目的のため、生の std::mutex
     * を意図して使用しています。
     * @return スレッド間で共有される排他制御ミューテックスの参照
     */
    static std::mutex& Get() {
        static std::mutex mutex;
        return mutex;
    }
};
} // namespace Irufemi
