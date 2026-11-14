#pragma once
#include <string>
#include <cstdint>
#include "Resource/Model/Data/Animation.h"

namespace Irufemi {

class AnimationSerializer {
public:
    static constexpr uint32_t kMagicNumber = 0x4D4E4149; // "IANM"
    static constexpr uint32_t kVersion = 1;

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t sourceLastWriteTime;
    };

    /**
     * @brief Animationをバイナリファイルに書き出す
     */
    static bool Serialize(const std::string& filepath, const Animation& animation, uint64_t sourceLastWriteTime);

    /**
     * @brief バイナリファイルからAnimationを読み込む
     */
    static bool Deserialize(const std::string& filepath, Animation& outAnimation, uint64_t& outSourceLastWriteTime);

    /**
     * @brief ヘッダーを読み込む
     */
    static bool ReadHeader(const std::string& filepath, Header& outHeader);
};

} // namespace Irufemi
